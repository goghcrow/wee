# all: clean novadump table_test sniff_test cloure_test server_test sa_test socket_test buffer_test poll_test threadpool_test queue_test mq_test mq_ts_test

# novadump: nova/novadump.c net/sniff.c base/buffer.c nova/Nova.c nova/BinaryData.c
	# $(CC) -std=c99 -g -Wall -lpcap -o $@ $^

table_test: base/table_test.c base/table.c
	$(CC) -std=c99 -g -Wall -o $@ $^

sniff_test: net/sniff_test.c net/sniff.c
	$(CC) -std=c99 -g -Wall -lpcap -o $@ $^

cloure_test: base/closure_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

sa_test: net/sa_test.c net/sa.c
	$(CC) -std=c99 -g -Wall -o $@ $^

server_test: net/server_test.c net/socket.c net/sa.c
	$(CC) -std=c99 -g -Wall -o $@ $^

socket_test: net/socket_test.c net/socket.c net/sa.c
	$(CC) -std=c99 -g -Wall -o $@ $^

buffer_test: base/buffer.c base/buffer_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

poll_test: net/poller_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

threadpool_test: base/threadpool.c base/threadpool_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

queue_test: base/queue_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_test: base/mq.c base/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_ts_test: base/mq.ts.c base/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

clean:
	-rm -f a.out
	-rm -f test
	-rm -f novadump
	-rm -f table_test
	-rm -f sniff_test
	-rm -f cloure_test
	-rm -f sa_test
	-rm -f server_test
	-rm -f socket_test
	-rm -f poll_test
	-rm -f buffer_test
	-rm -f queue_test
	-rm -f threadpool_test
	-rm -f mq_test
	-rm -f mq_ts_test
	-rm -rf *.dSYM