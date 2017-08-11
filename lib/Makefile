buffer_test: buffer/buffer.c buffer/buffer_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

poll_test: poll/socket_poll_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

threadpool_test: threadpool/threadpool.c threadpool/threadpool_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

q_test: threadpool/queue_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_test: mq/mq.c mq/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_ts_test: mq/mq.ts.c mq/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

clean:
	-rm -f a.out
	-rm -f poll_test
	-rm -f buffer_test
	-rm -f q_test
	-rm -f threadpool_test
	-rm -f mq_test
	-rm -f mq_ts_test
	-rm -rf *.dSYM