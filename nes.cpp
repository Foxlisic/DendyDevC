#include "nes.h"

NES::NES(int argc, char** argv)
{
    scale        = 3;               // Увеличить пиксели
    width        = 256;             // Ширина экрана
    height       = 240;             // Высота экрана
    frame_length = (1000/50);       // 50 FPS
    pticks       = 0;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        exit(1);
    }

    SDL_ClearError();
    sdl_window          = SDL_CreateWindow("SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, scale * width, scale * height, SDL_WINDOW_SHOWN);
    sdl_renderer        = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_PRESENTVSYNC);
    screen_buffer       = (Uint32*) malloc(width * height * sizeof(Uint32));
    sdl_screen_texture  = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, width, height);
    SDL_SetTextureBlendMode(sdl_screen_texture, SDL_BLENDMODE_NONE);

    // Инициализация
    cpu = new CPU();
    ppu = new PPU();
    cpu->reset();
    cpu->ce = 1;

    // Разбор arguments
    int i = 1;
    while (i < argc) {

        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'L': ppu->load(argv[++i]); break;
                case 'O': ppu->loadoam(argv[++i]); break;
            }
        }

        i++;
    }
}

// Один фрейм
void NES::frame()
{
    Uint32 start = SDL_GetTicks();

    // Отсчитывать пока не закончится внутренний кадр
    while (1) {

        // 1CPU = 3PPU
        for (int j = 0; j < 3; j++) {

            if (ppu->clock()) {

                for (int y = 0; y < 240; y++)
                for (int x = 0; x < 256; x++)
                    pset(x, y, ppu->frame[y][x]);

                // printf("%d\n", SDL_GetTicks() - start);
                return;
            }
        }
    }

}

// Основной цикл работы
int NES::main()
{
    SDL_Rect dstRect;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = scale * width;
    dstRect.h = scale * height;

    for (;;) {

        Uint32 ticks = SDL_GetTicks();
        while (SDL_PollEvent(& evt)) {

            switch (evt.type) {
                case SDL_QUIT: return 0;
            }
        }

        // Обновление экрана
        if (ticks - pticks >= frame_length) {

            pticks = ticks;

            frame();

            SDL_UpdateTexture       (sdl_screen_texture, NULL, screen_buffer, width * sizeof(Uint32));
            SDL_SetRenderDrawColor  (sdl_renderer, 0, 0, 0, 0);
            SDL_RenderClear         (sdl_renderer);
            SDL_RenderCopy          (sdl_renderer, sdl_screen_texture, NULL, & dstRect);
            SDL_RenderPresent       (sdl_renderer);

            return 1;
        }

        SDL_Delay(1);
    }
}

// Убрать окно из памяти
int NES::destroy()
{
    free(screen_buffer);
    SDL_DestroyTexture(sdl_screen_texture);
    SDL_FreeFormat(sdl_pixel_format);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
    return 0;
}

// Установка точки
void NES::pset(int x, int y, Uint32 cl)
{
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }

    screen_buffer[width*y + x] = cl;
}
