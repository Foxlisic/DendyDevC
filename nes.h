class NES
{
protected:

    SDL_Surface*        screen_surface;
    SDL_Window*         sdl_window;
    SDL_Renderer*       sdl_renderer;
    SDL_PixelFormat*    sdl_pixel_format;
    SDL_Texture*        sdl_screen_texture;
    SDL_Event           evt;
    Uint32*             screen_buffer;
    int                 width, height, scale, frame_length, pticks;

    // Подключить модули
    CPU*    cpu;
    PPU*    ppu;

public:

    NES(int argc, char** argv);

    void    tick();
    void    frame();
    int     main();
    int     destroy();
    void    pset(int x, int y, Uint32 cl);
};
