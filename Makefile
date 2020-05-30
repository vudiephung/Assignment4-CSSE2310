CFLAGS = -Wall -pedantic -g -std=gnu99
.DEFAULT_GOAL := all

all: mapper2310 control2310 roc2310

mapper2310: utils.o server.o mapper2310.o
	gcc $(CFLAGS) -pthread -o mapper2310 utils.o server.o mapper2310.o

control2310: utils.o server.o control2310.o
	gcc $(CFLAGS) -pthread -o control2310 utils.o server.o control2310.o

roc2310: utils.o server.o roc2310.o
	gcc $(CFLAGS) -o roc2310 utils.o server.o roc2310.o

mapper2310.o: mapper2310.c
	gcc $(CFLAGS) -c mapper2310.c

control2310.o: control2310.c
	gcc $(CFLAGS) -c control2310.c

roc2310.o: roc2310.c
	gcc $(CFLAGS) -c roc2310.c

utils.o: utils.c
	gcc $(CFLAGS) -c utils.c

server.o: server.c
	gcc $(CFLAGS) -c server.c

clean:
	rm *.o mapper2310 control2310