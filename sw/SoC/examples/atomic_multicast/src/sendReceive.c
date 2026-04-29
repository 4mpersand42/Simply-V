#include "sendReceive.h"
#include "amulticast_types.h"
#include "simplyv.h"

// CMAC Base Address
#define CMAC_BASEADDR   ((uintptr_t)_peripheral_CMAC_CSR_start)
// AXIS FIFO register offsets in xlnx_cmac.h already include +0x10000.
#define AXIS_FIFO_BASEADDR   ((uintptr_t)_peripheral_CMAC_CSR_start)
// Axis FIFO Data Base Address
#define AXIS_FIFO_DATA_BASEADDR   ((uintptr_t)_peripheral_CMAC_DATA_start)

#define ETH_FRAME_BYTES        64u

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
        unsigned char buffer[sizeof(multicast_msg_t)];
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
    }else if(type == PROPOSE){
        // Serializza il messaggio 
        unsigned char buffer [sizeof(propose_msg_t)];
        memcpy(buffer,(const unsigned char*)msg,sizeof(propose_msg_t));
        // Riempi i campi della frame 
        memcpy(p.field.header.h_dest, dest, ETH_ALEN); // riempi il campo MAC destinazione
        memcpy(p.field.header.h_source, src, ETH_ALEN); // riempi il campo MAC sorgente 
        p.field.header.h_proto = netbyteorder(proto); // riempi il campo MAC protocollo 
        // copia il messaggio nel campo payload della frame
        memcpy(p.field.data, buffer, sizeof(propose_msg_t));
    }else{
        errno = EINVAL;
        return -1;
    }

    size_t tx_buf_size = sizeof(struct ethhdr) + MAXPAYLOAD_LEN;
    // Padding Ethernet 
    if(tx_buf_size < 60){
        tx_buf_size = 60;
    }

    size_t byte_sent = xlnx_tx_axis_fifo_data(AXIS_FIFO_BASEADDR, AXIS_FIFO_DATA_BASEADDR, (const uint8_t*)p.buffer, tx_buf_size);

    if(byte_sent > 0){
        return 1;
    }else{
        return -1;
    }
    
}

// questa funzione si occupa di fare il parsing del messaggio ricevuto e di distinguere i diversi 
// tipi di messaggi. Un ipotetico unmarshalling deve essere implementato qui 
// per ora ci limitiamo ad analizzare i bytes raw del payload della frame ethernet 
// per distinguere tra le diverse tipologie di messaggio. Il tipo di messaggio sarà visibile
// nelle prime due cifre esadecimali che corrispondono al primo byte corrispondenente al
// campo type di entrambe le strutture di messaggio 
void packet_to_heap_msg(void *pr, const union ethframe *p){

    // Effettua il parsing della frame ricevuta e ne estrae il tipo 
    // copiando il primo byte del campo payload della frame. 
    msgtype_t type = p->field.data[0];
    
    // "Unmarshalling" copia i dati raw della frame nella struttura corretta
    // in base al tipo.
    if(type == MULTICAST){
        memcpy(pr, p->field.data, sizeof(multicast_msg_t));
        return;
    }else if(type == PROPOSE){
        memcpy(pr, p->field.data, sizeof(propose_msg_t));
        return;
    }
    return;
}

// Forse questo modo di ricevere i pacchetti non è il più efficiente
void acast_receive(void *pr, Node* node, void* msg_unused, msgtype_t type){
    (void)msg_unused;
    if(!node){ errno = EINVAL; return NULL; }
    if(g_qid < 0){ errno = EBADF; return NULL; }

    union ethframe p;
    memset(&p, 0, sizeof(p));

    size_t received_bytes = xlnx_rx_axis_fifo_data(AXIS_FIFO_BASEADDR, AXIS_FIFO_DATA_BASEADDR, (uint8_t*)p.buffer, sizeof(p.buffer));
    if(received_bytes > 0){
        return packet_to_heap_msg(&pr, &p);
    }else {
        return NULL;
    }
    
}