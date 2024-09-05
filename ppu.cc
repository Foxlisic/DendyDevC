#include "ppu.h"

class PPU
{
protected:

    int x, y;
    int t, v, finex, finex_;
    int A, D;
    int c0, c1;
    int _attr, attr, mask;

    // CHR-DATA $0000-$1FFF
    // VRAM     $2000-$2FFF
    // PALETTE  $3F00-$3F1F
    uint8_t memory[0x4000];
    uint8_t oam[0x100];
    uint8_t row[341];

public:

    int      vs, zs, nmi, ov;
    uint32_t frame[240][256];

    PPU() : x(0), y(0), t(0), v(0), finex(0), finex_(0), vs(0), zs(0), nmi(0), ov(0), c0(0x10), c1(0x1E) { }

    // Загрузка образа памяти из Fceux
    void load(const char* filename)
    {
        FILE* fp = fopen(filename, "rb");
        if (fp) { fread(memory, 1, 16384, fp); fclose(fp); }
        else    { printf("[%s] invalid BIN\n", filename); exit(1); }
    }

    // Загрузка образа OAM
    void loadoam(const char* filename)
    {
        FILE* fp = fopen(filename, "rb");
        if (fp) { fread(oam, 1, 256, fp); fclose(fp); }
        else    { printf("[%s] invalid OAM\n", filename); exit(1); }
    }

    // Чтение из видеопамяти
    uint8_t readv(uint16_t a)
    {
        if (a < 0x2000) {
            return memory[a];
        } else if (a < 0x3F00) {
            return memory[0x2000 + (a & 0x0FFF)];
        } else if (a < 0x4000) {
            return memory[0x3F00 + (a & 3 ? a & 0x1F : 0)];
        } else {
            return 0xFF;
        }
    }

    // 3PPU = 1CPU
    int clock()
    {
        int c;
        int nt = (v     ) & 0x0C00; // Nametable [11:10]
        int cx = (v     ) & 0x001F, // CoarseX   [ 4:0]
            cy = (v >> 5) & 0x001F; // CoarseY   [ 9:5]

        // 16PX + 0..255 Чтение пиксельных данных
        if (x < 256+16 && y < 240) {

            // Фон не отображается
            if ((c1 & 0x08) == 0) mask = 0;

            // Фон не виден в левом углу
            if ((c1 & 0x02) == 0 && x < 24) mask = 0;

            c = 7 - finex;
            c = ((mask >> c) & 1) + 2*((mask >> (8 + c)) & 1);

            // Запись пикселя тайла на линию
            row[x] = c ? 4*attr + c : 0;

            switch (finex)
            {
                // Запрос ATTR
                case 0: A = 0x23C0 + ((cx >> 2) & 7) + 8*((cy >> 2) & 7) + nt; break;
                case 1: _attr = (readv(A) >> ((2*(cy & 2) | (cx & 2)))) & 3; break;

                // Запрос TILE
                case 2: A = 0x2000 + cx + (cy << 5) + nt; break;
                case 3: D = readv(A); break;

                // Запрос BGL
                case 4: A = ((c0 & 0x10) << 8) + 16*D + ((v >> 12) & 7); break;
                case 5: D = readv(A); break;

                 // Запрос BGH
                case 6: A += 8; break;
                case 7:

                    mask = 256*readv(A) + D;
                    attr = _attr;

                    if ((v & 0x1F) == 0x1F) v ^= 0x400;     // Смена NT [горизонтальный]
                    v = (v & ~0x001F) | ((v + 1) & 0x1F);   // Инкремент CoarseX
                    break;
            }

            finex = (finex + 1) & 7;
        }

        // Нарисовать тайлы и спрайты :: к следующей линии
        if (x == 340) {

            v = (v & ~0x0400) | (t & 0x0400); // T[ 10] -> V[ 10]
            v = (v & ~0x001F) | (t & 0x001F); // T[4:0] -> V[4:0]

            // Сделать отступ [cx-1] для пререндеринга тайла
            v = (v & ~0x001F) | ((v - 1) & 0x1F);

            // Если есть переполнение, то поменять NTx
            if (v & 0x1F == 0x1F) v ^= 0x400;

            // FineY
            int fy = (v >> 12) & 7;

            if (fy == 7) {

                // Смена NT [Vertical]
                if      (cy == 29) { cy = 0; v ^= 0x800; }
                else if (cy == 31) { cy = 0; }
                else cy++;

                // CoarseY++
                v = (v & ~0x03E0) | (cy << 5);
            }

            // Не делать инкремент Y после 240-й линии
            if (y < 240) fy = (fy + 1) & 7;

            // FineY++
            v = (v & ~0x7000) | (fy << 12); finex = finex_;

            // Высота спрайта либо 16, либо 8
            int sp_height = (c0 & 0x20) ? 16 : 8;
            int sp_cnt    = 0;

            // Если OV=1, значит, спрайтов больше на линии, чем 8
            ov = 0;

            // Обработка спрайтов
            if (c1 & 0x10)
            for (int j = 0; j < 256; j += 4) {

                // Где спрайт начинается
                int sp_y = y - (oam[j] + 1);

                // Рисовать спрайт на линии
                if (sp_y >= 0 && sp_y < sp_height && sp_cnt < 8) {

                    int sp_ch = oam[j + 1];     // Иконка спрайта
                    int sp_at = oam[j + 2];     // Атрибуты
                    int sp_x  = oam[j + 3];     // Откуда начинается

                    // Отражение по вертикальной оси
                    if (sp_at & 0x80) sp_y = (sp_height - 1) - sp_y;

                    // Цветовая палитра
                    int sp_a  = (sp_y & 7) + (c0 & 8 ? 0x1000 : 0x0000);
                    int sp_pl = (sp_at & 3);

                    // 16x8 или 8x8
                    sp_a += (c0 & 0x20) ? ((sp_ch & 0xFE) << 4) + (sp_y & 8 ? 16 : 0) : sp_ch << 4;

                    // Прочесть пиксельные данные
                    int sp_pp = readv(sp_a) + 256*readv(sp_a + 8);

                    // Отрисовка 8 бит
                    for (int k = 0; k < 8; k++) {

                        int kx    = (sp_at & 0x40) ? k : (7-k);
                        int sp_cc = ((sp_pp >> kx) & 1) | 2*((sp_pp >> (8 + kx) & 1));

                        if (sp_cc) {

                            int bk = row[16 + sp_x + k];
                            int fr = sp_at & 0x20 ? 1 : 0;

                            // Спрайт за фоном или перед фоном
                            if (fr && (bk & 0b10011 == 0) || !fr) {
                                row[sp_x + k + 16] = 16 + 4*sp_pl + sp_cc;
                            }

                            // Достигнут Sprite0Hit
                            if (j == 0) zs = 1;
                        }
                    }

                    sp_cnt++;
                    if (sp_cnt >= 8) ov = 1;
                }
            }

            // Рендеринг строки [BG + SP]
            if (y < 240) {
                for (int j = 0; j < 256; j++) {
                    frame[y][j] = palette[memory[0x3F00 + row[j + 16]]];
                }
            }
        }

        // NMI: Возникает на 240-й линии
        if (x == 1 && y == 240) {

            vs  = 1;
            nmi = !!(c0 & 0x80);
        }

        // Последняя строчка кадра, NMI заканчивается на 260-й
        if (x == 340 && y == 260) {

            // 14:11, 9:5 :: 0yyy u0YY YYY0 0000
            v   = (v & ~0x7BE0) | (t & 0x7BE0);
            vs  = 0;
            zs  = 0;
            nmi = 0;
        }

        // Счетчик кадров
        y = (y + (x == 340)) % 262;
        x = (x + 1) % 341;

        return x == 0 && y == 0;
    }
};
