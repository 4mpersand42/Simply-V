#include "amulticast_types.h"
#include "sendReceive.h"
#include "simplyv.h"

// ==== FUNCTIONS FOR STRUCTURE MANAGEMENT ====

// Node constructor
void init_node(Node *node, g_id_t g_id){
    node->g_id = g_id;
    node->clock = 0;
    node->lts_count = 0;
    node->gts_count = 0;
    node->phase_count = 0;
    node->delivered_count = 0;
    for (int i = 0;i<MAX_MESSAGES;i++){
        node->phase[i] = START;
        node->delivered[i] = 0;
        node->lts[i].clock = 0;
        node->lts[i].g_id = 0;
        node->gts[i].clock = 0;
        node->gts[i].g_id = 0;
        node->propose_count[i] = 0;
        for(int j = 0; j < MAX_NUMBER_OF_GROUPS; j++){
            node->props[i][j].msg_id = 0;
            node->props[i][j].g_id = 0;
            node->props[i][j].lts.clock = 0;
            node->props[i][j].lts.g_id = 0;
        }
    }
    node->queue_count = 0;
}

// Node destructor
/*void destroy_node(Node* node){
    free(node);
}*/



// Message initializer: To review
void create_multicast_msg(multicast_msg_t *msg, payload_t payload, int id, const g_id_t *dstgrp, int dst_count){
    msg->type = MULTICAST;
    msg->msg_id = id;
    memcpy(msg->payload, payload, sizeof(msg->payload));
    for (int i = 0; i < dst_count; i++){
        msg->dstgrp[i] = dstgrp[i];
    }
    msg->dst_count = dst_count;
}
// Message destructor
/*void destroy_multicast_msg(multicast_msg_t *msg){
    free(msg);
}*/

void multicast_msg_cpy(const multicast_msg_t *src, multicast_msg_t *dst){
    dst->dst_count = src->dst_count;
    for(int i = 0; i < src->dst_count; i++){
        dst->dstgrp[i] = src->dstgrp[i];
    }
    dst->msg_id = src->msg_id;
    memcpy(dst->payload, src->payload, MAXPAYLOAD_LEN);
    dst->type = src->type; 
}


void create_propose_msg(propose_msg_t *msg, int id, g_id_t g_id, ts_t lts){
    msg->type = PROPOSE;
    msg->msg_id = id;
    msg->g_id = g_id;
    msg->lts = lts;
}

/*void destroy_propose_msg(propose_msg_t *msg){
    free(msg);
}*/

void propose_msg_cpy(const propose_msg_t *src, propose_msg_t *dst){
    dst->g_id = src->g_id;
    dst->lts = src->lts;
    dst->msg_id = src->msg_id;
    dst->type = src->type;
}

uint8_t timestamp_cmp(ts_t a, ts_t b){
    return a.clock == b.clock && a.g_id == b.g_id;
}

ts_t timestamp_max(ts_t a, ts_t b){
    if (a.clock > b.clock) return a;
    else return b;
}





// Delivery stub: returns -1 on failure, 1 on successful delivery
int deliver(Node* node, multicast_msg_t* msg){
    if(msg != NULL){
        printf("Message with timestamp: g_id %d, clock:%d delivered\n",node->gts[msg->msg_id].g_id,node->gts[msg->msg_id].clock);
        printf("MESSAGE CONTENT: %s\n",msg->payload);
        return 1;
    }
    else 
        return -1;
}
// ==== FUNCTIONS FOR ALGORITHM IMPLEMENTATION ==== 
// Send multicast message: Sends the multicast message
// to every recipient listed in the message
int multicast(Node* node, multicast_msg_t* msg){
    int err;
    printf("The node sends a MULTICAST message with id: %d\n", msg->msg_id);
    for(int i = 0; i<msg->dst_count;i++){
        err = acast_send(node, msg,MULTICAST,msg->dstgrp[i]);
        if (err < 0) {
            printf("ERROR: PID:%d unable to send MULTICAST message with id: %d to destination group: %d (acast_send=%d)\n", msg->msg_id, msg->dstgrp[i], err);
            return -1;
        }
    }
    return 1;
}

// Handling reception of MULTICAST messages
// This function only adds the multicast message to the
// node and updates its fields.
// After receiving the multicast message, the calling process must
// send timestamp proposals to all recipients of the message
int handle_multicast(Node* node, multicast_msg_t* msg){
    printf("Received a MULTICAST message with id: %d\n",msg->msg_id); // # DEBUG
    
    node->clock++; // increment local clock
    node->lts[msg->msg_id].clock = node->clock; // |
    node->lts[msg->msg_id].g_id = node->g_id;   // | save the timestamp in the local timestamp vector
    node->lts_count++; 
    node->phase[msg->msg_id] = PROPOSED;
    node->phase_count++;
    multicast_msg_cpy(msg, &node->msg_queue[msg->msg_id]); // add the message to the buffer 
    node->queue_count++;
    printf("Proposed timestamp: (g_id:%d ,clock:%d )\n",node->g_id,node->clock);
    return 1;
}
// Handling reception of PROPOSE messages
// This function only updates the node fields
// upon receiving each proposal.
int handle_propose(Node* node, propose_msg_t* msg){
    printf("Received a PROPOSE message with id: %d\n",msg->msg_id);
    int id = msg->msg_id;
    int count = node->propose_count[id];

    if(count >= MAX_NUMBER_OF_GROUPS - 1) return -1;


    // Copy the message into the queue
    propose_msg_cpy(msg, &node->props[id][count]);
    // Increment the counter of received proposals
    node->propose_count[id]++;
    // If we have all proposals from recipients, we can commit
    if(node->propose_count[id] == node->msg_queue[id].dst_count){
        commit(node, &node->msg_queue[id]);
        TOrder_deliver(node);
    }

    return 1;
}

void commit(Node* node, const multicast_msg_t *msg){
    int id = msg->msg_id;
    // Compute the global timestamp as the timestamp with the largest clock in the proposals vector
    unsigned int max_clock = 0;
    int index = 0;
    for(int i = 0; i < node->propose_count[id]; i++){
        if(max_clock < node->props[id][i].lts.clock){
            max_clock = node->props[id][i].lts.clock;
            index = i;
        }
    }
    node->gts[id].clock = node->props[id][index].lts.clock;
    node->gts[id].g_id = node->props[id][index].lts.g_id;
    node->gts_count++;
    // Update the clock
    if(node->clock<node->gts[msg->msg_id].clock) {
        node->clock = node->gts[msg->msg_id].clock;
    }
    // Commit the message
    node->phase[msg->msg_id]=COMMITTED;
    printf("Committed with id: %d\n",msg->msg_id);

}

static uint8_t ts_less(ts_t a, ts_t b) {
    if (a.clock < b.clock) return true;
    if (a.clock > b.clock) return false;
    return a.g_id < b.g_id;
}

void TOrder_deliver(Node *node){
    printf("Started message delivery...\n");

    for(int i = 0; i < MAX_MESSAGES; i++){
        if(node->phase[i] == COMMITTED && node->delivered[i] == 0){

            uint8_t can_deliver = true;

            for(int j = 0; j < MAX_MESSAGES; j++){
                if(node->phase[j] == PROPOSED && ts_less(node->lts[j], node->gts[i])){
                    // there exists a message not yet committed that should come before
                    can_deliver = false;
                    break;
                }
            }

            if(can_deliver){
                deliver(node, &node->msg_queue[i]);
                node->delivered[i] = 1;
                node->delivered_count++;
            }
        }
    }
    printf("Message delivery completed\n");
}