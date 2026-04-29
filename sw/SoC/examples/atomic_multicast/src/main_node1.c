#include "amulticast_types.h"
#include "sendReceive.h"
#include "simplyv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_MESSAGES 3

#define CMAC_BASEADDR   ((uintptr_t)_peripheral_CMAC_CSR_start)
// AXIS FIFO register offsets in xlnx_cmac.h already include +0x10000.
#define AXIS_FIFO_BASEADDR   ((uintptr_t)_peripheral_CMAC_CSR_start)
// Axis FIFO Data Base Address
#define AXIS_FIFO_DATA_BASEADDR   ((uintptr_t)_peripheral_CMAC_DATA_start)

static void send_propose_all(Node *node, const multicast_msg_t *m){
    propose_msg_t p; 
    create_propose_msg(p, m->msg_id, node->g_id, node->lts[m->msg_id]);
    printf("Invio proposte...\n");
    for(int i = 0; i < m->dst_count; i++){
        if(acast_send(node, p, PROPOSE, m->dstgrp[i]) < 0){
            printf("ERRORE send(PROPOSE)");
            return;
        }
    }
    printf("Proposte inviate\n");
}

static void handle_one_multicast(Node *node){
    multicast_msg_t *m = (multicast_msg_t*)acast_receive(node, NULL, MULTICAST);
    if(!m){
        printf("ERROR receive(MULTICAST)");
        return;
    }

    if(handle_multicast(node, m) < 0){
        printf("handle_multicast failed\n");
        return;
    }

    // All-to-all propose
    send_propose_all(node, m);

    // Regola di completamento: attendo dst_count propose
    for(int i = 0; i < m->dst_count; i++){
        propose_msg_t *p = (propose_msg_t*)acast_receive(node, NULL, PROPOSE);
        if(!p){
            printf("ERROR receive(PROPOSE)");
            return;
        }

        if(handle_propose(node, p) < 0){
            printf("handle_propose failed\n");
            return;
        }
    }

}

int main(void){
    // nodo 1
    Node n1; 
    init_node(n1, 1);
    

    // DONE: AGGIUNGERE IL CODICE PER INIZIALIZZARE IL CMAC 
    xlnx_cmac_init(CMAC_BASEADDR);
    xlnx_axis_fifo_init(AXIS_FIFO_BASEADDR);
    

    g_id_t dst[2] = {1, 2};

    // invia NUM_MESSAGES multicast
    for(int id = 0; id < NUM_MESSAGES; id++){
        payload_t pl;
        memset(pl, 0, sizeof(pl));
        printf("m%d-da-1", id);

        multicast_msg_t m;
        create_multicast_msg(m, pl, id, dst, 2);

        if(multicast(n1, m) < 0){
            fprintf(stderr, "multicast failed for msg_id=%d\n", id);
            return 1;
        }
    }

    // gestisce i NUM_MESSAGES multicast che gli arrivano
    for(int i = 0; i < NUM_MESSAGES; i++){
        handle_one_multicast(n1);
    }
    // Devo tenerlo?
    // mq_cleanup(n1, 1);
    return 0;
}