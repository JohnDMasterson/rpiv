rpiv: src/rpiv.c
	gcc src/rpiv.c -o bin/rpiv -lasound -lpthread

cava_mod: src/cava_mod.c
	gcc src/cava_mod.c -o bin/cava_mod -lasound -lpthread -lm -lfftw3 -lwiringPi