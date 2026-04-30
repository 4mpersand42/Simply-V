#ifndef SENDRECEIVE_H
#define SENDRECEIVE_H

#include "amulticast_types.h"
#include "simplyv.h"
#define ETH_ALEN 6
#define ETH_DATA_LEN 1500
#define ETH_FRAME_LEN 1514
static int g_qid = -1;
// To enable use on RISC-V it may be necessary
// to define the macros: ETH_DATA_LEN, ETH_FRAME_LEN, ETH_ALEN and the struct ethhdr
// Structure of the ethernet frame

struct ethhdr {
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	uint16_t		h_proto;		/* packet type ID field	*/
} __attribute__((packed));


union ethframe{
    struct{
        struct ethhdr header;
        unsigned char data[ETH_DATA_LEN];
    } field;
    unsigned char buffer[ETH_FRAME_LEN];
};

// utility to convert to network byte order
uint16_t netbyteorder(uint16_t x); // same as htons; use that if available

// utility to parse the message
// when porting to real RISC-V the header will need to be modified
// this function should receive ethframe directly instead of sysv_packet_t
void packet_to_heap_msg(void *pr, const union ethframe *p);


// Send and receive using System V message queues
int acast_send(Node* node, void* msg, msgtype_t type, g_id_t dst);
void acast_receive(Node* node, void* msg, msgtype_t type);
#endif