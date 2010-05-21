CC	= g++
CFLAGS	= `pth-config --cflags`
LDFLAGS	= `pth-config --ldflags`
LIBS	= `pth-config --libs`

all: server client
server: server.o
	$(CC) $(LDFLAGS) -lrt	 -o server server.o $(LIBS)
server.o: server.cpp
	$(CC) $(CFLAGS) -c server.cpp
client: client.o
	$(CC) -o client client.o
client.o: client.cpp
	$(CC) -c client.cpp
clean:
	rm -f server server.o client client.o



