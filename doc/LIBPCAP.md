依赖libpcap-dev
sudo apt-get install libpcap-dev
sudo yum install -y libpcap-devel.x86_64

必须链接 pcap进行编译
gcc <filename> -lpcap

man pcap
man 3 pcap
man 7 pcap-filter
man pcap_open_live
man pcap filter

## litebsd tcpip calltree 

http://tcpipv2.org/en/latest/calltree/init.html

#### man pcap_next

Do  NOT  assume  that the packets for a given capture or ``savefile`` will have
any given link-layer header type, such as DLT_EN10MB for Ethernet.   For  exam-
ple,  the  "any"  device  on  Linux  will  have  a  link-layer  header  type of
DLT_LINUX_SLL even if all devices on the system at the time the "any" device is
opened have some other data link type, such as DLT_EN10MB for Ethernet.

#### pcap_next vs pcap_next_ex

pcap_next 无法判断是否有错误发生，或者是超时

```c
       int pcap_next_ex(pcap_t *p, struct pcap_pkthdr **pkt_header,
               const u_char **pkt_data);
       const u_char *pcap_next(pcap_t *p, struct pcap_pkthdr *h);
```

RETURN VALUE
       pcap_next_ex()  returns 1 if the packet was read without problems, 0 if packets
       are being read from a live capture, and the timeout expired,  -1  if  an  error
       occurred  while  reading  the  packet,  and -2 if packets are being read from a
       ``savefile'', and there are no more packets to read from the savefile.   If  -1
       is returned, pcap_geterr() or pcap_perror() may be called with p as an argument
       to fetch or display the error text.

       pcap_next() returns a pointer to the packet data on success, and  returns  NULL
       if  an  error occurred, or if no packets were read from a live capture (if, for
       example, they were discarded because they didn't pass the packet filter, or if,
       on platforms that support a read timeout that starts before any packets arrive,
       the timeout expires before any packets arrive, or if the  file  descriptor  for
       the  capture device is in non-blocking mode and no packets were available to be
       read), or if no more packets are available in a  ``savefile.''   Unfortunately,
       there is no way to determine whether an error occurred or not.

```c
int pcap_loop(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
```
The contract we have to satisfy with our callback function

```c
typedef void (*pcap_handler)(u_char *, const struct pcap_pkthdr *, const u_char *);
```

libpcap packet const char* 

payload_pointer = packet_pointer + len(Ethernet header) + len(IP header) + len(TCP header)

1. The ethernet header is always 14 bytes as defined by standards.
2. The IP header length is always stored in a 4 byte integer at byte offset 4 of the IP header.
3. The TCP header length is always stored in a 4 byte integer at byte offset 12 of the TCP header.
4. The payload starts at packet base location plus all the header lengths.


The IP header and TCP are typically about 20 bytes each if there are no options passed.
That means the first 54 bytes are the header layers, and the rest is actual data. We should not guess or assume the headers will always be 20 bytes each though. We need to get the actual header length for both IP and TCP layers in order to calculate the offset for the payload.

To clarify the difference between promiscuous mode and monitor mode: monitor mode is just for wireless cards and promiscuous is for wireless and wired. Monitor mode lets the card listen to wireless packets without being associated to an access point. Promiscuous mode lets the card listen to all packets, even ones not intended for it.

Pass any non-zero integer to turn it on and 0 to turn off.
pcap_can_set_rfmon() can be used to see if a device has the capability.

```c
int pcap_set_rfmon(pcap_t *p, int rfmon);
```

pcap_open_live = create + active

We have been using pcap_open_live() to get the device handle, but that creates the device handle and activates it at the same time. To set the rfmon mode before activating the device handle must be manually created. Use pcap_create() and pcap_activate().

```c
char error_buffer[PCAP_ERRBUF_SIZE];
pcap_t *handle = pcap_create("wlan0", error_buffer);
pcap_set_rfmon(handler, 1);
pcap_set_promisc(handler, 1); /* Capture packets that are not yours */
pcap_set_snaplen(handler, 2048); /* Snapshot length */
pcap_set_timeout(handler, 1000); /* Timeout in milliseconds */
pcap_activate(handle);
/* handle is ready for use with pcap_next() or pcap_loop() */
```

### filter

```c
int pcap_compile(pcap_t *p, struct bpf_program *fp, char *str, int optimize, 
        bpf_u_int32 netmask)

int pcap_setfilter(pcap_t *p, struct bpf_program *fp)
```

netmask 的唯一用途是用来检查IPv4广播地址;

any 用来capture多个网卡, 子网掩码这时候是PCAP_NETMASK_UNKNOWN;

man pcap_compile

netmask specifies the
IPv4  netmask  of  the  network on which packets are being captured; it is used
only when checking for IPv4 broadcast addresses in the filter program.  If  the
netmask  of  the network on which packets are being captured isn't known to the
program, or if packets are being captured on the Linux  "any"  pseudo-interface
that  can capture on more than one network, a value of PCAP_NETMASK_UNKNOWN can
be supplied; tests for IPv4 broadcast addresses will fail to compile,  but  all
other tests in the filter program will be OK.


man pcap_next


#### Example Filters

```
# Packets to or from host
host box4
# or by IP
host 8.8.8.8

# Packets between hosts
host box2 and box4

# Packets by source
src 10.2.4.1

# By destination
dst 99.99.2.2

# By port
port 143
portange 1-1024

# By source/destination port
src port 22
dst port 80

# By Protocol
tcp
udp
icmp

# And
src localhost and dst port 22
src localhost && dst port 22

# Or
port 80 or 22
port 80 || 22

# Grouping
src localhost and (dst port 80 or 22 or 443)
```

### Sending Packets


```
int pcap_inject(pcap_t *p, const void *buf, size_t size);
int bytes_written = pcap_inject(handle, &raw_bytes, sizeof(raw_bytes));
```


### pcap_datalink

!!! 注意: 不能假定是数据链路层是ether, 所以不能直接 将pkt指针强转成 struct ether_header *
mac 测试当网卡不是any时，返回1， ether，any时返回12 DLT_RAW

```c
#define DLT_NULL	0	/* BSD loopback encapsulation */
#define DLT_EN10MB	1	/* Ethernet (10Mb) */
#define DLT_EN3MB	2	/* Experimental Ethernet (3Mb) */
#define DLT_AX25	3	/* Amateur Radio AX.25 */
#define DLT_PRONET	4	/* Proteon ProNET Token Ring */
#define DLT_CHAOS	5	/* Chaos */
#define DLT_IEEE802	6	/* 802.5 Token Ring */

// ...

/*
 * This is for Linux cooked sockets.
 */
#define DLT_LINUX_SLL	113

```

原始IP（raw IP）；数据包以IP头开始。

```c
#ifdef __OpenBSD__
#define DLT_RAW		14	/* raw IP */
#else
#define DLT_RAW		12	/* raw IP */
#endif
```

<!-- linux any device 返回的ether_typ DLT_LINUX_SSL -->

Datalink is important because the packet will start with a datalink-specific
header, which we must skip to get to IP. But how many bytes to skip?
Different DLTs have different lengths. We can either hard-code them, 
or try setting DLT_RAW, to get rid of the datalink header entirely.
This will lose us info such as on which intreface the packet arrived,
and from what MAC address. --SB: RAW does not work on "any".


pcap_datalink() returns the link-layer header type for the live capture or``savefile'' specified by p.

It must not be called on a pcap descriptor created by pcap_create() that has not yetbeen activated by pcap_activate().

http://www.tcpdump.org/linktypes.html  lists  the values pcap_datalink() can returnand describes the packet formats that correspond to
those values.

Do NOT assume that the packets for a given capture or ``savefile`` will have anygiven link-layer header type, such as  DLT_EN10MB  for
Ethernet.  For example, the "any" device on Linux will have a link-layer header typeof DLT_LINUX_SLL even if all devices on the system
at the time the "any" device is opened have some other data link type, such asDLT_EN10MB for Ethernet.
