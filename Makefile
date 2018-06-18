CFLAGS=-O0 -g3 -Wall

all: client daemon

%.o : %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

client: client.o

daemon: daemon.o

clean:
	$(RM) *.o client daemon
