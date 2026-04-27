#ifndef SENDRECEIVE_H
#define SENDRECEIVE_H

#include "amulticast_types.h"
#include <linux/if_ether.h>
#include <stdint.h>
static int g_qid = -1;
// Per consentire l'utilizzo su risc V potrebbe essere necessario 
// definire le macro: ETH_DATA_LEN, ETH_FRAME_LEN, ETH_ALEN e la struct ethhdr
// Structure of the ethernet frame 
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
void* packet_to_heap_msg(const union ethframe *p);
// Setup/cleanup coda System V per un nodo
int mq_setup(Node *node);
void mq_cleanup(Node *node, int remove_queue);

// Invia e riceve usando System V message queues
int acast_send(Node* node, void* msg, msgtype_t type, g_id_t dst);
void* acast_receive(Node* node, void* msg_unused, msgtype_t type);

#endif