CFLAGS = -Wall -pedantic -g -std=gnu99
.DEFAULT_GOAL := all

all: mapper2310

mapper2310: utils.o mapper2310.o
	gcc $(CFLAGS) -o mapper2310 utils.o mapper2310.o

mapper2310.o: mapper2310.c
	gcc $(CFLAGS) -c mapper2310.c

utils.o: utils.c
	gcc $(CFLAGS) -c utils.c

clean:
	rm *.o mapper2310