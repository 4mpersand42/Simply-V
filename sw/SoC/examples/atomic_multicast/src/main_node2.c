#include "amulticast_types.h"
#include "sendReceive.h"
#include "simplyv.h"
#include <stdio.h>
#include <stdlib.h>

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
    // nodo 2
    Node *n2 = init_node(2);
    if(!n2) return 1;

    if(mq_setup(n2) < 0) return 1;

    for(int i = 0; i < NUM_MESSAGES; i++){
        handle_one_multicast(n2);
    }

    // mq_cleanup(n2, 1);
    destroy_node(n2);
    return 0;
}