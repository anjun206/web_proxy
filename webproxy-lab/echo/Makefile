CC = gcc
CFLAGS = -g -Wall -I..
LDFLAGS = -lpthread

all: echo_srv echo_cli

echo_srv: echo_srv.o ../csapp.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

echo_cli: echo_cli.o ../csapp.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c ../csapp.h
	$(CC) $(CFLAGS) -c $<

# 루트에 csapp.o 없으면 자동으로 만들어줌
../csapp.o: ../csapp.c ../csapp.h
	$(CC) $(CFLAGS) -c ../csapp.c -o ../csapp.o

clean:
	rm -f *.o echo_srv echo_cli
