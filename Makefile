cloure_test: base/closure_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

socket_test: net/socket_test.c net/socket.c
	$(CC) -std=c99 -g -Wall -o $@ $^

buffer_test: base/buffer.c base/buffer_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

poll_test: net/poller_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

threadpool_test: threadpool/threadpool.c threadpool/threadpool_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

q_test: threadpool/queue_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_test: mq/mq.c mq/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_ts_test: mq/mq.ts.c mq/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

clean:
	-rm -f a.out
	-rm -f test
	-rm -f cloure_test
	-rm -f socket_test
	-rm -f poll_test
	-rm -f buffer_test
	-rm -f q_test
	-rm -f threadpool_test
	-rm -f mq_test
	-rm -f mq_ts_test
	-rm -rf *.dSYM