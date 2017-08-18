#include <pcap/pcap.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>


#define MAXBYTES2CAPTURE 65535

void processPacket(u_char *arg, const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
    int *counter = (int *)arg;

    printf("Packet Count: %d\n", ++(*counter));
    printf("Received Packet Size: %d %d\n", pkthdr->len, pkthdr->caplen);
    printf("Payload: \n");

    assert(pkthdr->caplen == pkthdr->len);

    int i;
    for (i = 0; i < pkthdr->len; i++)
    {
        if (isprint(packet[i]))
        {
            printf("%c ", packet[i]);
        }
        else
        {
            printf(". ");
        }

        if ((i % 32 == 0 && i != 0) || i == pkthdr->len - 1)
        {
            printf("\n");
        }
    }
    return;
}

int main(int argc, char **argv)
{
    int count = 0;
    char errbuf[PCAP_ERRBUF_SIZE];
    memset(errbuf, 0, sizeof(errbuf));

    // Get the name of the first device suitable for capture

    char *device;
    if (argc > 1)
    {
        device = argv[1];
    }
    else
    {
        device = pcap_lookupdev(errbuf);
    }

    if (device == NULL)
    {
        fprintf(stderr, "ERROR pcap_lookupdev, %s", errbuf);
        exit(1);
    }

    printf("Opening device %s\n", device);

    // Open device in promiscuous mode
    pcap_t *descr = pcap_open_live(device, MAXBYTES2CAPTURE, 1, 512, errbuf);
    if (descr == NULL)
    {
        fprintf(stderr, "ERROR pcap_open_live, %s", errbuf);
        exit(1);
    }

    // Loop forever & call processPacket() for every received packet
    if (pcap_loop(descr, -1, processPacket, (u_char *)&count) == -1)
    {
        fprintf(stderr, "ERROR pcap_loop, %s", errbuf);
        exit(1);
    }

    return 0;
}