#include <SDL2/SDL.h>

#include "cpu.cc"
#include "ppu.cc"
#include "nes.cpp"

/**
 * -L <file.bin> -- загрузка образа видеопамяти 16K
 */

int main(int argc, char** argv) {

    NES* nes = new NES(argc, argv);
    while (nes->main()) { };
    return nes->destroy();
}
