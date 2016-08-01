all: mca2wav

mca2wav: main.c mca.c mca.h
	gcc -std=c99 -O3 -c mca.c
	gcc -std=c99 -O3 -c main.c
	gcc mca.o main.o -o mca2wav

clean:
	rm -rf *.o *.exe mca2wav

