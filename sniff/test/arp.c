#include <pcap/pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>

/*
struct pcap_pkthdr
{
    struct timeval ts; // Timestamp of capture
    bpf_u_int32 caplen;// Number of bytes that were stored
    bpf_u_int32 len;   // Total length of the packet
};
*/

#define ARP_REQUEST 1
#define ARP_REPLY 2

struct arphdr
{
    u_int16_t htype; // Hardware type
    u_int16_t ptype; // Protocol type
    u_char hlen;     // Hardware Address Length
    u_char plen;     // Protocol Address Length
    u_int16_t oper;  // Operation Code
    u_char sha[6];   // Sender Hardware Address
    u_char spa[4];   // Sender Ip Address
    u_char tha[6];   // Target Hardware Address
    u_char tpa[4];   // Target IP Address
};

#define MAXBYTES2CAPTURE 2048

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("USAGE: arpsniffer <interface>\n");
        exit(1);
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *descr = pcap_open_live(argv[1], MAXBYTES2CAPTURE, 0, 512, errbuf);
    if (descr == NULL)
    {
        fprintf(stderr, "ERROR pcap_open_live, %s\n", errbuf);
        exit(1);
    }

    bpf_u_int32 netaddr = 0;
    bpf_u_int32 mask = 0;
    // Look uo info from the capture device
    if (pcap_lookupnet(argv[1], &netaddr, &mask, errbuf) == -1)
    {
        fprintf(stderr, "ERROR pcap_lookupnet, %s\n", errbuf);
        exit(1);
    }

    struct bpf_program filter;
    // Compiles the filter expression into a BPF filter program
    if (pcap_compile(descr, &filter, "arp", 1, mask) == -1)
    {
        fprintf(stderr, "ERROR pcap_compile, %s\n", pcap_geterr(descr));
        exit(1);
    }

    // Load the fitler program into the pakcet capture device
    if (pcap_setfilter(descr, &filter) == -1)
    {
        fprintf(stderr, "ERROR pcap_setfilter, %s\n", pcap_geterr(descr));
        exit(1);
    }

    struct pcap_pkthdr *pkthdr;
    struct arphdr *arphdr;
    const u_char *packet = NULL;
    int r;

    while (1)
    {
        // packet = pcap_next(descr, &pkthdr);
        // if (packet == NULL)
        // {
        //     perror("ERROR pcap_next");
        //     exit(1);
        // }

        // pcap_next_ex() returns 1 if the packet was read without problems, 0 if packets are being read from a
        // live capture, and the timeout expired, -1 if an error occurred while reading the packet, and  -2  if
        // packets are being read from a ``savefile'', and there are no more packets to read from the savefile.
        // If -1 is returned, pcap_geterr() or pcap_perror() may be called with p as an argument  to  fetch  or
        // display the error text.

        r = pcap_next_ex(descr, &pkthdr, &packet);
        if (r == 0)
        {
            continue;
        }
        if (r == -1)
        {
            fprintf(stderr, "ERROR pcap_next, %s\n", pcap_geterr(descr));
            exit(1);
        }

        assert(pkthdr->len == pkthdr->caplen);

        arphdr = (struct arphdr *)(packet + 14);

        printf("\n\nReceived Packet Size: %d bytes\n", pkthdr->len);
        printf("Hardware type: %s\n", (ntohs(arphdr->htype) == 1) ? "Ethernet" : "Unknown");
        printf("Protocol type: %s\n", (ntohs(arphdr->ptype) == 0x0800) ? "IPv4" : "Unknown");
        printf("Operation: %s\n", (ntohs(arphdr->oper) == ARP_REQUEST) ? "ARP Request" : "ARP Reply");

        // If is Ethernet and IPv4, print packet contents
        if (ntohs(arphdr->htype) == 1 && ntohs(arphdr->ptype) == 0x0800)
        {
            int i = 0;
            printf("Sender MAC: ");
            for (i = 0; i < 6; i++)
            {
                printf("%02X:", arphdr->sha[i]);
            }

            printf("\nSender IP: ");
            for (i = 0; i < 4; i++)
            {
                printf("%d.", arphdr->spa[i]);
            }

            printf("\nTarget MAC: ");
            for (i = 0; i < 6; i++)
            {
                printf("%02X:", arphdr->tha[i]);
            }

            printf("\nTarget IP: ");
            for (i = 0; i < 4; i++)
            {
                printf("%d.", arphdr->tpa[i]);
            }

            printf("\n");
        }
    }

    return 0;
}
