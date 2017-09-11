nova: nova.c codec.c generic.c ../base/cJSON.c ../base/buffer.c ../net/socket.c
	$(CC) -std=c99 -D_GNU_SOURCE -g -Wall -o $@ $^

novadump-dev: novadump.c codec.c ../base/buffer.c ../net/sniff.c
	$(CC) -std=c99 -D_GNU_SOURCE -D_BSD_SOURCE -D__USE_BSD -D__FAVOR_BSD -lpcap -g -O0 -Wall -o $@ $^

novadump: novadump.c codec.c ../base/buffer.c ../net/sniff.c
	$(CC) -std=c99 -D_GNU_SOURCE -D_BSD_SOURCE -D__USE_BSD -D__FAVOR_BSD -DNDEBUG -lpcap -O3 -Wall -o $@ $^

clean:
	-rm -f nova
	-rm -f novadump
	-rm -f novadump-dev
	-rm -rf *.dSYM