# all: clean cloure_test server_test sa_test socket_test buffer_test poll_test threadpool_test q_test mq_test mq_ts_test

novadump: nova/novadump.c sniff/tcpsniff.c base/buffer.c nova/Nova.c nova/BinaryData.c
	$(CC) -std=c99 -g -Wall -lpcap -o $@ $^

table_test: base/table_test.c base/table.c
	$(CC) -std=c99 -g -Wall -o $@ $^

tcpsniff: sniff/tcpsniff_test.c sniff/tcpsniff.c
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

threadpool_test: threadpool/threadpool.c threadpool/threadpool_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

q_test: threadpool/queue_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_test: base/mq.c base/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_ts_test: base/mq.ts.c base/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

clean:
	-rm -f a.out
	-rm -f novadump
	-rm -f table_test
	-rm -f tcpsniff
	-rm -f test
	-rm -f cloure_test
	-rm -f sa_test
	-rm -f server_test
	-rm -f socket_test
	-rm -f poll_test
	-rm -f buffer_test
	-rm -f q_test
	-rm -f threadpool_test
	-rm -f mq_test
	-rm -f mq_ts_test
	-rm -rf *.dSYM