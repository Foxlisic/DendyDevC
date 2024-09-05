#include "cpu.h"

class CPU
{
protected:

    // Регистры
    uint8_t  a, x, y, p, s;
    uint16_t pc;

    // Внутреннее состояние; p=NV1B|DIZC
    uint8_t  t, T, E, opcode;

public:

    // Интерфейс взаимодействия
    uint8_t  ce;
    uint16_t A;
    uint8_t  I, R, W, D;

    void reset()
    {
        pc = 0xFFFF;
        t  = 0;
    }

    // Исполнение одного цикла CPU
    void clock()
    {
        // Процессор не работает
        if (ce == 0) return;

        // Прочесть опкод
        if (t == 0) { opcode = I; A = ++pc; }

        int optype = operand_types[opcode];
        int opname = opcode_names [opcode];

        t++;

        R = 0; // Чтение по умолчанию
        W = 0; // Запись по требованию
        D = opname == STX ? x : (opname == STY ? y : a);

        // Запись по умолчанию [STA, STX, STY]
        int _w = (opname == STA || opname == STX || opname == STY) ? 1 : 0;
        int _r = 1 - _w;

        // Инструкции, которые записывают в память
        int _u = opname == ASL || opname == LSR || opname == ROL || opname == ROR ||
                 opname == DEC || opname == INC || _w;

        // Декодировать инструкции в зависимости от типа операндов
        switch (optype) {

            // 6T Indirect,X
            case NDX: {

                switch (t)
                {
                    case 2: A = I; pc++; break;
                    case 3: A = (I + x) & 0xFF; break;
                    case 4: A = (A & 0xFF00) + ((A + 1) & 0xFF); E = I; break;
                    case 5: A = 256*I + E; W = _w; R = _r; break;
                    case 6: A = pc; t = 0; instr(I); break;
                }

                break;
            }

            // 6T+ Indirect,Y
            case NDY: {

                switch (t)
                {
                    case 2: A = I; pc++; break;
                    case 3: E = I; A++; break;
                    case 4:

                        E += y;
                        A = I*256 + E;
                        t = (E & 0x100) || _u ? 5 : 6;
                        W = _w; R = _r;
                        break;

                    case 5: break;
                    case 6: A = pc; t = 0; instr(I); break;
                }

                break;
            }

            // 3T ZeroPage
            case ZP: {

                switch (t)
                {
                    case 2: A = I; pc++; break;
                    case 3: A = pc; t = 0; instr(I); break;
                }

                break;
            }

            // 4T ZeroPage,XY
            case ZPX:
            case ZPY: {

                switch (t)
                {
                    case 2: A = I; pc++; break;
                    case 3: A = ((optype == ZPX ? x : y) + A) & 255;
                    case 4: A = pc; t = 0; instr(I); break;
                }

                break;
            }

            // 4T Absolute
            case ABS: {

                switch (t)
                {
                    case 2: E = I; pc++; break;
                    case 3: A = E + 256*I; pc++; W = _w; R  = _r;  break;
                    case 4: A = pc; t = 0; instr(I); break;
                }

                break;
            }

            // Absolute,X
            case ABX:
            case ABY: {

                switch (t)
                {
                    case 2: E = I; pc++; break;
                    case 3:

                        E += (optype == ABX ? x : y);
                        A  = E + 256*I;
                        t = (E & 0x100) || _u ? 4 : 5;
                        W = _w; R = _r; pc++;
                        break;

                    case 4: break;
                    case 5: A = pc; t = 0; instr(I); break;
                }

                break;
            }

            // 3T JMP ABS
            case JMP: {

                switch (t)
                {
                    case 2: E = I; pc++; break;
                    case 3: t = 0; pc = 256*I + E; break;
                }

                break;
            }

            // 5T JMP (IND)
            case IND: {

                switch (t)
                {
                    case 2: E  = I; pc++; break;
                    case 3: A  = E + 256*I; pc++; R = 1; break;
                    case 4: E  = I; A++; R = 1; break;
                    case 5: pc = E + 256*I; t = 0; break;
                }

                break;
            }

            // JSR ABS
            case JSR: {

                switch (t)
                {
                    case 2: E = I; pc++; break;
                    case 3: W = 1; D = pc >> 8;  A = 0x100 + s; s--; pc = (I*256) + (pc & 255); break;
                    case 4: W = 1; D = pc & 255; A = 0x100 + s; s--; break;
                    case 5: pc = (pc & 0xFF00) + E; break;
                    case 6: t = 0; A = pc; break;
                }

                break;
            }

            // IMM, IMP, ACC
            case IMM:
            case IMP: {

                switch (t)
                {
                    case 2: pc++; t = 0; instr(I); break;
                }

                break;
            }

            // 2**T Условный переход
            case REL: {

                break;
            }
        }
    }

    // Исполнение инструкции
    void instr(uint8_t b)
    {
        switch (opcode)
        {
            case LDA: case ORA:
            case EOR: case AND:
            case ADC: case SBC:

                a = alu(opcode >> 5, a, b);
                break;

            case LDX: x = alu(5, 0, b); break;
            case LDY: y = alu(5, 0, b); break;
            case CMP: alu(6, a, b); break;
            case CPX: alu(6, x, b); break;
            case CPY: alu(6, y, b); break;
        }
    }

    // Базовое АЛУ
    uint8_t alu(uint8_t mode, uint8_t op1, uint8_t op2)
    {
        int r = 0;

        // Вычислить результат
        switch (mode)
        {
            case 0: r = op1 | op2; break;           // ORA
            case 1: r = op1 & op2; break;           // AND
            case 2: r = op1 ^ op2; break;           // EOR
            case 3: r = op1 + op2 + (p & 1); break; // ADC
            case 4: r = op1; break;                 // STA
            case 5: r = op2; break;                 // LDA
            case 6: r = op1 - op2; break;           // CMP
            case 7: r = op1 - op2 - !(p & 1);       // SBC
        }

        // Вычислить флаги
        int nf = (r & 0x80),
            zf = ((r & 0xFF) == 0 ? 2 : 0),
            cf = (r & 0x100) ? 1 : 0;

        switch (mode)
        {
            // 0 ORA, 1 AND, 2 EOR, 5 LDA
            case 0:
            case 1:
            case 2:
            case 5:

                p = (p & 0b01111101) | nf | zf;
                break;

            // ADC
            case 3:

                p = (p & 0b00111100) | nf | zf | cf;
                if ((op1 ^ op2 ^ 0x80) & (op1 ^ r) & 0x80) p |= 0x40;
                break;

            // CMP
            case 6:

                p = (p & 0b01111100) | nf | zf | (1-cf);
                break;

            // SBC
            case 7:

                p = (p & 0b00111100) | nf | zf | cf;
                if ((op1 ^ op2) & (op1 ^ r) & 0x80) p |= 0x40;
                break;
        }

        return r & 255;
    }
};
