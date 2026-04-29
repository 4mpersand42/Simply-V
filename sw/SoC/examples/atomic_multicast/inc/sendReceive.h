#ifndef SENDRECEIVE_H
#define SENDRECEIVE_H

#include "amulticast_types.h"
#include "simplyv.h"
#define ETH_ALEN 6
#define ETH_DATA_LEN 1500
#define ETH_FRAME_LEN 1514
static int g_qid = -1;
// Per consentire l'utilizzo su risc V potrebbe essere necessario 
// definire le macro: ETH_DATA_LEN, ETH_FRAME_LEN, ETH_ALEN e la struct ethhdr
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

// utility per convertire in network byte order
uint16_t netbyteorder(uint16_t x); // è la stessa cosa di htons usare quella nel caso sia disponibile

// utility per fare il parsing del messaggio 
// quando si passa su risk V effettivo l'intestazione andrà modificata
// questa funzione dovrà ricevere direttamente ethframe e non più sysv_packet_t
void packet_to_heap_msg(void *pr, const union ethframe *p);


// Invia e riceve usando System V message queues
int acast_send(Node* node, void* msg, msgtype_t type, g_id_t dst);
void acast_receive(void *pr, Node* node, void* msg_unused, msgtype_t type);
#endif