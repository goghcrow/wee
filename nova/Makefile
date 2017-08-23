nova: nova.c novacodec.c generic.c ../base/cJSON.c ../base/buffer.c ../net/socket.c
	$(CC) -std=c99 -D_GNU_SOURCE -g -Wall -o $@ $^

clean:
	-rm nova
	-rm -r *.dSYM