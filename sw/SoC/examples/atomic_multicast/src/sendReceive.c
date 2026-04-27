#include "sendReceive.h"
#include "amulticast_types.h"
#include "xlnx_cmac.h"
#include <linux/if_ether.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <netinet/in.h>
#define BASEADDR 2
#define DATA_BASEADDR 2


static long sysv_mtype_for(msgtype_t type){
    if(type == MULTICAST) return 1;
    if(type == PROPOSE) return 2;
    return 3;
}

uint16_t netbyteorder(uint16_t x){
    // Determina la endianness del processore a runtime 
    union{
        uint16_t u16;
        uint8_t b[2];
    } t;
    // copia 1 nell'area di memoria della union 
    // che avendo anche un campo costituito da un vettore di interi unsigned a 8 bit 
    // può essere scomposta in una parte più significativa e una meno 
    t.u16 = 1; 
    // Se 1 è stato copiato prima nella parte meno significativa allora il runtime è 
    // little endian, devo eseguire lo swap per convertirlo in big endian
    if(t.b[0] == 1) {
        return (uint16_t)((x << 8) | (x >> 8));
    }
    return x;
}


int acast_send(Node* node, void* msg, msgtype_t type, g_id_t dst){
    // Questi indirizzi sono degli stub, bisogna cambiare l'interfaccia delle funzioni
    // acast_send/receive per fare in modo che passino anche l'indirizzo MAC del destinatario
    // L'indirizzo MAC del mittente deve essere recuperato dall'interfaccia del NIC
    unsigned char dest[] = {0x00,0x12,0x34,0x56,0x78,0x90};
    unsigned char src[] = {0x90,0x87,0x65,0x43,0x21,0x00};
    unsigned short proto = 0x1234;

    (void)node;
    if(!msg){ errno = EINVAL; return -1; }

    union ethframe p;
    memset(&p, 0, sizeof(p));

    if(type == MULTICAST){
        // Serializza il messaggio 
        unsigned char *buffer = (unsigned char*)malloc(sizeof(multicast_msg_t));
        memcpy(buffer,(const unsigned char*)msg,sizeof(multicast_msg_t));
        for(int i = 0; i<sizeof(buffer);i++){
            printf("%02x",buffer[i]);
        }
        printf("\n");

        // Riempi i campi della frame 
        memcpy(p.field.header.h_dest, dest, ETH_ALEN); // riempi il campo MAC destinazione
        memcpy(p.field.header.h_source, src, ETH_ALEN); // riempi il campo MAC sorgente 
        p.field.header.h_proto = netbyteorder(proto); // riempi il campo MAC protocollo 
        // copia il messaggio nel campo payload della frame
        memcpy(p.field.data, buffer, sizeof(multicast_msg_t));
        printf("Contenuto payload frame MULTICAST: \n");
        for(int i = 0; i<ETH_DATA_LEN;i++){
            printf("%02x",p.field.data[i]);
        }
        printf("\n");
        free(buffer);
    }else if(type == PROPOSE){
        // Serializza il messaggio 
        unsigned char *buffer = (unsigned char*)malloc(sizeof(propose_msg_t));
        memcpy(buffer,(const unsigned char*)msg,sizeof(propose_msg_t));
        // Riempi i campi della frame 
        memcpy(p.field.header.h_dest, dest, ETH_ALEN); // riempi il campo MAC destinazione
        memcpy(p.field.header.h_source, src, ETH_ALEN); // riempi il campo MAC sorgente 
        p.field.header.h_proto = netbyteorder(proto); // riempi il campo MAC protocollo 
        // copia il messaggio nel campo payload della frame
        memcpy(p.field.data, buffer, sizeof(propose_msg_t));
        free(buffer);
    }else{
        errno = EINVAL;
        return -1;
    }

    uint32_t v;
    // qui devo chiamare la primitiva del driver tx_axis_fifo_data
    // per trasmettere quattro byte alla volta.
    for(int i = 0; i < sizeof(union ethframe); i++){
        memcpy(&v, p.buffer+i, sizeof(v));
        tx_axis_fifo_data(BASEADDR, v, v, sizeof(v));
    }

    return 1;
}

// questa funzione si occupa di fare il parsing del messaggio ricevuto e di distinguere i diversi 
// tipi di messaggi. Un ipotetico unmarshalling deve essere implementato qui 
// per ora ci limitiamo ad analizzare i bytes raw del payload della frame ethernet 
// per distinguere tra le diverse tipologie di messaggio. Il tipo di messaggio sarà visibile
// nelle prime due cifre esadecimali che corrispondono al primo byte corrispondenente al
// campo type di entrambe le strutture di messaggio 
void* packet_to_heap_msg(const union ethframe *p){

    // Effettua il parsing della frame ricevuta e ne estrae il tipo 
    // copiando il primo byte del campo payload della frame. 
    msgtype_t type = p->field.data[0];
    
    // "Unmarshalling" copia i dati raw della frame nella struttura corretta
    // in base al tipo.
    if(type == MULTICAST){
        multicast_msg_t *m = (multicast_msg_t*)malloc(sizeof(multicast_msg_t));
        if(!m) return NULL;
        memcpy(m, p->field.data, sizeof(multicast_msg_t));
        return m;
    }else if(type == PROPOSE){
        propose_msg_t *pr = (propose_msg_t*)malloc(sizeof(propose_msg_t));
        if(!pr) return NULL;
        memcpy(pr, p->field.data, sizeof(propose_msg_t));
        return pr;
    }
    errno = EINVAL;
    return NULL;
}

void* acast_receive(Node* node, void* msg_unused, msgtype_t type){
    (void)msg_unused;
    if(!node){ errno = EINVAL; return NULL; }
    if(g_qid < 0){ errno = EBADF; return NULL; }

    union ethframe p;
    memset(&p, 0, sizeof(p));


    // qui devo chiamare la primitiva del driver tx_axis_fifo_data
    // per ricevere quattro byte alla volta.
    uint32_t v;
    int ret=0;
    for(int i = 0;i<sizeof(union ethframe);i++){
        ret=rx_axis_fifo_data(BASEADDR, v);
        if(ret>0){ //se non riceve niente non copiare niente 
            memcpy(p.buffer+i, &v, sizeof(uint32_t));
        }
    }

    return packet_to_heap_msg(&p);
}