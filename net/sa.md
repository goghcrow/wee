
```c

struct sockaddr {
	__uint8_t	sa_len;		    /* total length */
	sa_family_t	sa_family;	    /* address family */
	char		sa_data[14];	/* addr value (actually larger) */
};

struct sockaddr_storage {
	__uint8_t	ss_len;		    /* address length */
	sa_family_t	ss_family;	    /* address family */
	char		__ss_pad1[_SS_PAD1SIZE];
	__int64_t	__ss_align;	    /* force structure storage alignment */
	char		__ss_pad2[_SS_PAD2SIZE];
};

/* Internet socket address */
struct sockaddr_in {
    sa_family_t sin_family;     /* address family: AF_INET */
    uint16_t sin_port;          /* port in network byte order */
    struct in_addr sin_addr;    /* internet address */
};

/* Internet address */
typedef uint32_t in_addr_t;
struct in_addr {
    in_addr_t s_addr; /* address in network byte order */
};

struct sockaddr_in6 {
    sa_family_t sin6_family;   /* address family: AF_INET6 */
    uint16_t sin6_port;        /* port in network byte order */
    uint32_t sin6_flowinfo;    /* IPv6 flow information */
    struct in6_addr sin6_addr; /* IPv6 address */
    uint32_t sin6_scope_id;    /* IPv6 scope-id */
};
```