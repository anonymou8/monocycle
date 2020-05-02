
all: enscribe monocycle

enscribe:
	gcc -lgd -lpng -lz -ljpeg -lfreetype -lm -lsndfile   enscribe.c -o enscribe

monocycle:
	gcc -lm -lfftw3 -lpng   monocycle.c -o monocycle

