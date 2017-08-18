
依赖libpcap-dev
sudo apt-get install libpcap-dev

必须链接 pcap进行编译
gcc <filename> -lpcap

```c
#include <pcap/pcap.h>

/* The total packet length, including all headers
       and the data payload is stored in
       header->len and header->caplen. Caplen is
       the amount actually available, and len is the
       total packet length even if it is larger
       than what we currently have captured. If the snapshot
       length set with pcap_open_live() is too small, you may
       not have the whole packet. */
struct pcap_pkthdr
{
    struct timeval ts;      /* Timestamp of capture             */
    bpf_u_int32 caplen;     /* Number of bytes that were stored */
    bpf_u_int32 len;        /* Total length of the packet       */
};


```c
#include <net/ethernet.h>

struct	ether_header {
	u_char	ether_dhost[6];     /* dst MAC   */
	u_char	ether_shost[6];     /* src MAC   */
	u_short	ether_type;         /* IP ARP ...*/
};
```

```c
struct	ether_arp {
	struct	arphdr ea_hdr;	    /* fixed-size header       */
	u_char	arp_sha[6];	        /* sender hardware address */
	u_char	arp_spa[4];	        /* sender protocol address */
	u_char	arp_tha[6];	        /* target hardware address */
	u_char	arp_tpa[4];	        /* target protocol address */
};
```

netinet/ip.h

http://unix.superglobalmegacorp.com/BSD4.4/newsrc/netinet/ip.h.html

```c
#define	IP_MAXPACKET	65535

struct ip {
#if BYTE_ORDER == LITTLE_ENDIAN 
	u_char	ip_hl:4,		/* header length */
		    ip_v:4;			/* version */
#endif
#if BYTE_ORDER == BIG_ENDIAN 
	u_char	ip_v:4,			/* version */
	    	ip_hl:4;		/* header length */
#endif
	u_char	ip_tos;			/* type of service */
	short	ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	short	ip_off;			/* fragment offset field */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	struct	in_addr ip_src,ip_dst;	/* source and dest address */
};
```

netinet/tcp.h

http://unix.superglobalmegacorp.com/BSD4.4/newsrc/netinet/tcp.h.html

```c

typedef	u_long	tcp_seq;
/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */
struct tcphdr {
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	tcp_seq	th_seq;			/* sequence number */
	tcp_seq	th_ack;			/* acknowledgement number */
#if BYTE_ORDER == LITTLE_ENDIAN 
	u_char	th_x2:4,		/* (unused) */
		    th_off:4;		/* data offset */
#endif
#if BYTE_ORDER == BIG_ENDIAN 
	u_char	th_off:4,		/* data offset */
		    th_x2:4;		/* (unused) */
#endif
	u_char	th_flags;
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
	u_short	th_win;			/* window */
	u_short	th_sum;			/* checksum */
	u_short	th_urp;			/* urgent pointer */
};

```

http://www.cse.scu.edu/~dclark/am_256_graph_theory/linux_2_6_stack/linux_2ip_8h-source.html

```c
struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
        __u8    ihl:4,
                version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
        __u8    version:4,
                ihl:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
        __u8    tos;
        __u16   tot_len;            /* 偏移四字节为ippkt长度 */
        __u16   id;
        __u16   frag_off;
        __u8    ttl;
        __u8    protocol;
        __u16   check;
        __u32   saddr;
        __u32   daddr;
        /* The options start here. */
};
```

http://www.cse.scu.edu/~dclark/am_256_graph_theory/linux_2_6_stack/linux_2tcp_8h-source.html

```c
struct tcphdr {
        __u16   source;
        __u16   dest;
        __u32   seq;
        __u32   ack_seq;
#if defined(__LITTLE_ENDIAN_BITFIELD)
        __u16   res1:4,
                doff:4,
                fin:1,
                syn:1,
                rst:1,
                psh:1,
                ack:1,
                urg:1,
                ece:1,
                cwr:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
        __u16   doff:4,
                res1:4,
                cwr:1,
                ece:1,
                urg:1,
                ack:1,
                psh:1,
                rst:1,
                syn:1,
                fin:1;
#else
#error  "Adjust your <asm/byteorder.h> defines"
#endif  
        __u16   window;
        __u16   check;
        __u16   urg_ptr;
};
```

libpcap packet const char* 

payload_pointer = packet_pointer + len(Ethernet header) + len(IP header) + len(TCP header)

1. The ethernet header is always 14 bytes as defined by standards.
2. The IP header length is always stored in a 4 byte integer at byte offset 4 of the IP header.
3. The TCP header length is always stored in a 4 byte integer at byte offset 12 of the TCP header.
4. The payload starts at packet base location plus all the header lengths.


The IP header and TCP are typically about 20 bytes each if there are no options passed.
That means the first 54 bytes are the header layers, and the rest is actual data. We should not guess or assume the headers will always be 20 bytes each though. We need to get the actual header length for both IP and TCP layers in order to calculate the offset for the payload.