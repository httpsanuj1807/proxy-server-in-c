CC=g++
CFLAGS= -g -Wall

all: proxy

proxy: proxyServer.c
	$(CC) $(CFLAGS) -o proxy_parse.o -c proxy_parse.c -lpthread
	$(CC) $(CFLAGS) -o map.o -c map.c -lpthread
	$(CC) $(CFLAGS) -o lru_cache.o -c lru_cache.c -lpthread
	$(CC) $(CFLAGS) -o proxy.o -c proxyServer.c -lpthread
	$(CC) $(CFLAGS) -o proxy proxy.o proxy_parse.o map.o lru_cache.o -lpthread

clean:
	rm -f *.o proxy


