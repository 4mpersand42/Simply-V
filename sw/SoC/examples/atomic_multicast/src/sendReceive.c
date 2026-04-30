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
    // Determine processor endianness at runtime
    union{
        uint16_t u16;
        uint8_t b[2];
    } t;
    // store 1 in the union memory area
    // the union also exposes the value as a two-byte array
    // allowing us to inspect the byte order
    t.u16 = 1; 
    // If the least significant byte contains 1 the runtime is
    // little-endian and we must swap to convert to big-endian
    if(t.b[0] == 1) {
        return (uint16_t)((x << 8) | (x >> 8));
    }
    return x;
}


int acast_send(Node* node, void* msg, msgtype_t type, g_id_t dst){
    // These addresses are stubs; the acast_send/receive interface
    // should be changed so that the recipient MAC address is passed
    // The sender MAC address must be retrieved from the NIC interface
    unsigned char dest[] = {0x00,0x12,0x34,0x56,0x78,0x90};
    unsigned char src[] = {0x90,0x87,0x65,0x43,0x21,0x00};
    unsigned short proto = 0x1234;

    if(msg == NULL){ return -1; }

    union ethframe p;
    memset(&p, 0, sizeof(p));

    if(type == MULTICAST){
        // Serialize the message
        unsigned char buffer[sizeof(multicast_msg_t)];
        memcpy(buffer,(const unsigned char*)msg,sizeof(multicast_msg_t));
        for(int i = 0; i<sizeof(buffer);i++){
            printf("%02x",buffer[i]);
        }
        printf("\n");

        // Fill the frame fields
        memcpy(p.field.header.h_dest, dest, ETH_ALEN); // fill destination MAC field
        memcpy(p.field.header.h_source, src, ETH_ALEN); // fill source MAC field
        p.field.header.h_proto = netbyteorder(proto); // fill protocol field
        // copy the message into the frame payload field
        memcpy(p.field.data, buffer, sizeof(multicast_msg_t));
        printf("MULTICAST frame payload content:\n");
        for(int i = 0; i<ETH_DATA_LEN;i++){
            printf("%02x",p.field.data[i]);
        }
        printf("\n");
    }else if(type == PROPOSE){
        // Serialize the message
        unsigned char buffer [sizeof(propose_msg_t)];
        memcpy(buffer,(const unsigned char*)msg,sizeof(propose_msg_t));
        // Fill the frame fields
        memcpy(p.field.header.h_dest, dest, ETH_ALEN); // fill destination MAC field
        memcpy(p.field.header.h_source, src, ETH_ALEN); // fill source MAC field
        p.field.header.h_proto = netbyteorder(proto); // fill protocol field
        // copy the message into the frame payload field
        memcpy(p.field.data, buffer, sizeof(propose_msg_t));
    }else{
        return -1;
    }

    size_t tx_buf_size = sizeof(struct ethhdr) + MAXPAYLOAD_LEN;
    // Ethernet padding
    if(tx_buf_size < 60){
        tx_buf_size = 60;
    }

    size_t byte_sent = xlnx_tx_axis_fifo_data(AXIS_FIFO_BASEADDR, AXIS_FIFO_DATA_BASEADDR, (const uint8_t*)p.buffer, tx_buf_size);

    if(byte_sent > 0){
        return 1;
    }else{
        return -1;
    }
    return 1;
}

// This function parses the received message and distinguishes the different
// message types. A proper unmarshalling implementation should be added here.
// For now we analyze the raw bytes of the ethernet frame payload
// to distinguish between message types. The message type is visible
// in the first payload byte which corresponds to the `type` field of the message structures
void packet_to_heap_msg(void *pr, const union ethframe *p){

    // Parse the received frame and extract the type
    // by copying the first byte of the frame payload
    msgtype_t type = p->field.data[0];
    
    // Unmarshall: copy the raw frame data into the correct structure
    // according to the message type.
    if(type == MULTICAST){
        memcpy(pr, p->field.data, sizeof(multicast_msg_t));
        return;
    }else if(type == PROPOSE){
        memcpy(pr, p->field.data, sizeof(propose_msg_t));
        return;
    }
    return;
}

// Maybe this way of receiving packets is not the most efficient
void acast_receive(Node* node, void* msg, msgtype_t type){
    if(!node){ 
        return;
 }
    if(g_qid < 0){ 
        return; 
    }

    union ethframe p;
    memset(&p, 0, sizeof(p));

    size_t received_bytes = xlnx_rx_axis_fifo_data(AXIS_FIFO_BASEADDR, AXIS_FIFO_DATA_BASEADDR, (uint8_t*)p.buffer, sizeof(p.buffer));
    if(received_bytes > 0){
        packet_to_heap_msg(msg, &p);
        return;
    }else {
        return;
    }
}