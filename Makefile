CFLAGS  = -std=c99 -Wall -fopenmp
CFLAGS += -O3 -ffast-math -mtune=native -ftree-vectorizer-verbose=3
LDLIBS =-lm
thorn: thorn.c
clean:
	rm -f thorn
