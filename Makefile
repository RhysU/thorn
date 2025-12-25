CFLAGS  = -std=c99 -Wall -fopenmp
CFLAGS += -O3 -ffast-math -mtune=native -ftree-vectorizer-verbose=3
LDLIBS =-lm
thorn: thorn.c

# Settings and resolutions occasionally of interest
backgrounds: thorn_2560_1440.png thorn_1920_1080.png thorn_1600_0900.png
thorn_2560_1440.png: thorn
	./thorn -s 2560,1440 -y -0.5,+0.5 $(basename $@).pgm
	convert $(basename $@).pgm $@
	rm -f $(basename $@).pgm
thorn_1920_1080.png: thorn
	./thorn -s 1920,1080 -y -0.5,+0.5 $(basename $@).pgm
	convert $(basename $@).pgm $@
	rm -f $(basename $@).pgm
thorn_1600_0900.png: thorn
	./thorn -s 1600,900 -y -0.5,+0.5 $(basename $@).pgm
	convert $(basename $@).pgm $@
	rm -f $(basename $@).pgm

clean:
	rm -f thorn thorn_*.pgm
