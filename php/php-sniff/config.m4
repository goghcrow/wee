PHP_ARG_ENABLE(tcpsniff, whether to enable ae support,
[  --enable-tcpsniff           Enable ae support])

if test "$PHP_TCPSNIFF" != "no"; then
  PHP_NEW_EXTENSION(ae, 
  	php_ae.c						\
  	ae/ae.c 						\
  	aeUtil.c 						\
    aeTcpServer.c       \
    aeEventLoop.c,
  $ext_shared)

  CFLAGS="-std=gnu99 -g -Wall -lpcap"
  PHP_NEW_EXTENSION(tcpsniff, tcpsniff.c, $ext_shared)
  
  PHP_ADD_BUILD_DIR([$ext_builddir/coroutine])  
  PHP_ADD_BUILD_DIR([$ext_builddir/ae])
fi