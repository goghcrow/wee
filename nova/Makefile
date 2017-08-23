nova: nova.c novacodec.c generic.c ../base/cJSON.c ../base/buffer.c ../net/socket.c
	$(CC) -std=c99 -D_GNU_SOURCE -g -Wall -o $@ $^

novadump: novadump.c novacodec.c ../base/buffer.c ../net/sniff.c
	$(CC) -std=c99 -D_GNU_SOURCE -D_BSD_SOURCE -D__USE_BSD -D__FAVOR_BSD -lpcap -g -Wall -o $@ $^

clean:
	-rm -f nova
	-rm -f novadump
	-rm -fr *.dSYM