all: app
	./nes -L roms/01_video.bin -O roms/01_oam.bin > nes.log
app:
	g++ -o nes nes.cc -lSDL2
clean:
	rm nes nes.log
