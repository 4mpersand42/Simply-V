#include "amulticast_types.h"
#include "sendReceive.h"
#include "simplyv.h"


#define NUM_MESSAGES 3
// CMAC Base Address
#define CMAC_BASEADDR   ((uintptr_t)_peripheral_CMAC_CSR_start)
// AXIS FIFO register offsets in xlnx_cmac.h already include +0x10000.
#define AXIS_FIFO_BASEADDR   ((uintptr_t)_peripheral_CMAC_CSR_start)
// Axis FIFO Data Base Address
#define AXIS_FIFO_DATA_BASEADDR   ((uintptr_t)_peripheral_CMAC_DATA_start)

static void send_propose_all(Node *node, const multicast_msg_t *m){
    propose_msg_t p;
    create_propose_msg(&p, m->msg_id, node->g_id, node->lts[m->msg_id]);
    printf("Sending proposals...\n");
    for(int i = 0; i < m->dst_count; i++){
        if(acast_send(node, &p, PROPOSE, m->dstgrp[i]) < 0){
            printf("ERROR send(PROPOSE) to destination group %d\n", m->dstgrp[i]);
            return;
        }
    }
    printf("Proposals sent\n");
    //destroy_propose_msg(&p);
}

static void handle_one_multicast(Node *node){
    multicast_msg_t m; 
    acast_receive(node, &m, MULTICAST);
    /*if(!m){
        printf("ERROR receive(MULTICAST)\n");
        return;
    }*/

    if(handle_multicast(node, &m) < 0){
        printf("handle_multicast failed\n");
        return;
    }

    // All-to-all proposals
    send_propose_all(node, &m);

    // Completion rule: wait for dst_count proposals
    for(int i = 0; i < m.dst_count; i++){
        propose_msg_t p;
        acast_receive(node, &p, PROPOSE);


        if(handle_propose(node, &p) < 0){
            printf("handle_propose failed\n");
            return;
        }
    }
}

int main(void){
    // node 2
    Node n2;
    init_node(&n2, 2);


    // DONE: ADD CODE TO INITIALIZE THE CMAC
    xlnx_cmac_init(CMAC_BASEADDR);
    xlnx_axis_fifo_init(AXIS_FIFO_BASEADDR);
    for(int i = 0; i < NUM_MESSAGES; i++){
        handle_one_multicast(&n2);
    }

    // mq_cleanup(n2, 1);
    return 0;
}