# all:

.PHONY: nova dubbo

table_test: base/table_test.c base/table.c
	$(CC) -std=c99 -g -Wall -o $@ $^

sniff_test: net/sniff_test.c net/sniff.c base/buffer.c
	$(CC) -std=c99 -g3 -O0 -Wall -lpcap -o $@ $^

cloure_test: base/closure_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

sa_test: net/sa_test.c net/sa.c
	$(CC) -std=c99 -g -Wall -o $@ $^

server_test: net/server_test.c net/socket.c net/sa.c
	$(CC) -std=c99 -g -Wall -o $@ $^

socket_test: net/socket_test.c net/socket.c net/sa.c
	$(CC) -std=c99 -g -Wall -o $@ $^

# buffer_test: base/buffer2.c base/buffer_test.c
buffer_test: base/buffer.c base/buffer_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

poll_test: net/poller_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

threadpool_test: base/threadpool.c base/threadpool_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

queue_test: base/queue_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

mq_test: base/mq.c base/mq_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

forward_test: net/sa.c net/socket.c net/socket_forward_test.c base/waitgroup.c
	$(CC) -std=gnu99 -g -Wall -o $@ $^ -lpthread

mtxlock_test: base/mtxlock.c base/mtxlock_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

cond_test: base/mtxlock.c base/cond.c base/cond_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

waiter_test: base/mtxlock.c base/cond.c base/waiter.c base/waiter_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

waitgroup_test: base/waitgroup.c base/waitgroup_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread

chan_test: base/mtxlock.c base/cond.c base/mq.c base/chan.c base/chan_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^ -lpthread -DMQ_THREAD_SAFE

hs_test: dubbo/hessian.c dubbo/hessian_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

ae_test: ae/anet.c ae/ae.c ae/ae_test.c
	$(CC) -std=c99 -g -Wall -o $@ $^

dubbo_debug: base/utf8_decode.c base/cJSON.c base/buffer.c base/dbg.c net/socket.c net/sa.c 3rd/ae/ae.c dubbo_client/dubbo_hessian.c dubbo_client/dubbo_codec.c dubbo_client/dubbo_client.c dubbo_client/dubbo.c
	$(CC)  -I3rd/ae -Ibase -Inet -fsanitize=address -fno-omit-frame-pointer -D_GNU_SOURCE -std=gnu99 -g3 -O0 -Wall -o $@ $^

dubbo: base/utf8_decode.c base/cJSON.c base/buffer.c base/dbg.c net/socket.c net/sa.c 3rd/ae/ae.c dubbo_client/dubbo_hessian.c dubbo_client/dubbo_codec.c dubbo_client/dubbo_client.c dubbo_client/dubbo.c
	$(CC) -I3rd/ae -Ibase -Inet -D_GNU_SOURCE -std=gnu99 -g -Wall -o $@ $^

nova: nova_client/nova.c nova_client/codec.c nova_client/generic.c base/cJSON.c base/buffer.c net/socket.c
	$(CC) -Ibase -Inet -std=c99 -D_GNU_SOURCE -g -Wall -o $@ $^

novadump-dev: nova_client/novadump.c nova_client/codec.c base/buffer.c net/sniff.c
	$(CC) -Ibase -Inet -std=c99 -D_GNU_SOURCE -D_BSD_SOURCE -D__USE_BSD -D__FAVOR_BSD -lpcap -g -O0 -Wall -o $@ $^

novadump: novadump.c codec.c ../base/buffer.c ../net/sniff.c
	$(CC) -Ibase -Inet -std=c99 -D_GNU_SOURCE -D_BSD_SOURCE -D__USE_BSD -D__FAVOR_BSD -DNDEBUG -lpcap -O3 -Wall -o $@ $^

clean:
	-/bin/rm -f a.out
	-/bin/rm -f table_test
	-/bin/rm -f sniff_test
	-/bin/rm -f cloure_test
	-/bin/rm -f sa_test
	-/bin/rm -f server_test
	-/bin/rm -f socket_test
	-/bin/rm -f poll_test
	-/bin/rm -f buffer_test
	-/bin/rm -f queue_test
	-/bin/rm -f threadpool_test
	-/bin/rm -f mq_test
	-/bin/rm -f forward_test
	-/bin/rm -f mtxlock_test
	-/bin/rm -f cond_test
	-/bin/rm -f waiter_test
	-/bin/rm -f waitgroup_test
	-/bin/rm -f chan_test
	-/bin/rm -f hs_test
	-/bin/rm -f ae_test
	-/bin/rm -f dubbo_debug
	-/bin/rm -f dubbo
	-/bin/rm -f nova
	-/bin/rm -f novadump
	-/bin/rm -f novadump-dev
	-/bin/rm -rf *.dSYM