#include "amulticast_types.h"
#include "sendReceive.h"
#include "simplyv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_MESSAGES 3

static void send_propose_all(Node *node, const multicast_msg_t *m){
    propose_msg_t *p = create_propose_msg(m->msg_id, node->g_id, node->lts[m->msg_id]);
    printf("Invio proposte...\n");
    for(int i = 0; i < m->dst_count; i++){
        if(acast_send(node, p, PROPOSE, m->dstgrp[i]) < 0){
            perror("send(PROPOSE)");
            exit(1);
        }
    }
    printf("Proposte inviate\n");
    destroy_propose_msg(p);
}

static void handle_one_multicast(Node *node){
    multicast_msg_t *m = (multicast_msg_t*)acast_receive(node, NULL, MULTICAST);
    if(!m){
        perror("receive(MULTICAST)");
        exit(1);
    }

    if(handle_multicast(node, m) < 0){
        fprintf(stderr, "handle_multicast failed\n");
        exit(1);
    }

    // All-to-all propose
    send_propose_all(node, m);

    // Regola di completamento: attendo dst_count propose
    for(int i = 0; i < m->dst_count; i++){
        propose_msg_t *p = (propose_msg_t*)acast_receive(node, NULL, PROPOSE);
        if(!p){
            perror("receive(PROPOSE)");
            exit(1);
        }

        if(handle_propose(node, p) < 0){
            fprintf(stderr, "handle_propose failed\n");
            exit(1);
        }

        free(p);
    }

    free(m);
}

int main(void){
    // nodo 1
    Node *n1 = init_node(1);
    if(!n1) return 1;

    if(mq_setup(n1) < 0) return 1;

    // In un setup a due eseguibili: avvia prima node2, poi node1.
    sleep(1);

    g_id_t dst[2] = {1, 2};

    // invia NUM_MESSAGES multicast
    for(int id = 0; id < NUM_MESSAGES; id++){
        payload_t pl;
        memset(pl, 0, sizeof(pl));
        snprintf(pl, MAXPAYLOAD_LEN, "m%d-da-1", id);

        multicast_msg_t *m = create_multicast_msg(pl, id, dst, 2);
        if(!m) return 1;

        if(multicast(n1, m) < 0){
            fprintf(stderr, "multicast failed for msg_id=%d\n", id);
            return 1;
        }

        destroy_multicast_msg(m);
    }

    // gestisce i NUM_MESSAGES multicast che gli arrivano
    for(int i = 0; i < NUM_MESSAGES; i++){
        handle_one_multicast(n1);
    }
    // Devo tenerlo?
    // mq_cleanup(n1, 1);
    destroy_node(n1);
    return 0;
}