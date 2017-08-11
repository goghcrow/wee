socket-server: socket_server.c test.clean
	gcc -g -Wall -o $@ $^ -lpthread

clean:
	rm socket-server