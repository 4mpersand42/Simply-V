#ifndef _AMCAST_TYPES_H_
#define _AMCAST_TYPES_H_
#include "simplyv.h"

#define MAXPAYLOAD_LEN 20
#define MAX_NUMBER_OF_GROUPS 8 // For now assume each group is formed by a single reliable process which therefore cannot fail
#define MAX_MESSAGES 64

typedef unsigned int clk_t;
typedef unsigned int g_id_t;
typedef enum {START, PROPOSED, COMMITTED} phase_t;
typedef enum {PROPOSE, MULTICAST, DELIVER} msgtype_t;
typedef char payload_t [MAXPAYLOAD_LEN];

typedef struct {
    g_id_t g_id;
    clk_t clock;
}ts_t;


// Multicast message structure:
// This message must include an id field to identify it in the queue
// a payload field that contains the actual message to be delivered
// to make it more flexible the payload could become a void pointer in the future
// the destination vector with the related counter field
typedef struct {
    msgtype_t type;
    int msg_id;
    payload_t payload;
    g_id_t dstgrp[MAX_NUMBER_OF_GROUPS];
    int dst_count;
}multicast_msg_t;


// Propose message structure:
// this message type is used to send the proposal for the timestamp to be assigned
// to a message that was previously multicast
// it must therefore contain the msg_id of the broadcast message
// and the group id of the proposer
typedef struct {
    msgtype_t type;
    int msg_id;
    g_id_t g_id;
    ts_t lts;
}propose_msg_t;

// NODE structure: this struct abstracts all the local variables
// of a process to avoid using globals.
// To implement Skeen's algorithm each node must have:
// - an id
// - an lts vector to store local timestamps of received messages
// - a gts vector containing global timestamps of received messages
// - a vector indexed by message id that keeps track
//   of the different phases a message is in
// - a message queue for messages waiting to be delivered
// - the local clock
// - variables to track the boundaries of lts and gts vectors: lts_count gts_count
typedef struct {
    g_id_t g_id;
    clk_t clock;
    ts_t lts[MAX_MESSAGES];
    ts_t gts[MAX_MESSAGES];
    phase_t phase[MAX_MESSAGES];
    multicast_msg_t msg_queue[MAX_MESSAGES];
    uint8_t delivered[MAX_MESSAGES];    
    // PROPOSE per-message (needed for all-to-all)
    propose_msg_t props[MAX_MESSAGES][MAX_NUMBER_OF_GROUPS];
    int propose_count[MAX_MESSAGES];
    
    int delivered_count;
    int lts_count;
    int gts_count; 
    int phase_count; 
    int queue_count;
}Node;


// ==== FUNCTIONS FOR STRUCTURE MANAGEMENT ====

// Node constructor
void init_node(Node *node, g_id_t g_id);
// Node destructor
//void destroy_node(Node* node);

// Function to create a multicast message:
// to build this message you need to provide the payload,
// the message id and the list of recipients
void create_multicast_msg(multicast_msg_t *msg, payload_t payload, int id, const g_id_t *dstgrp, int dst_count);
// Multicast message destructor
//void destroy_multicast_msg(multicast_msg_t *msg);

// Propose message constructor
void create_propose_msg(propose_msg_t *msg, int id, g_id_t g_id, ts_t lts);
// Propose message destructor
//void destroy_propose_msg(propose_msg_t *msg);

// Function that returns true if two timestamps have the same clock
// and the same g_id
uint8_t timestamp_cmp(ts_t a, ts_t b);

// Function that receives two timestamps and returns
// the one with the larger clock
ts_t timestamp_max(ts_t a, ts_t b);

// function to copy multicast messages
void multicast_msg_cpy(const multicast_msg_t *src, multicast_msg_t *dst);

// function to copy propose messages
void propose_msg_cpy(const propose_msg_t *src, propose_msg_t *dst);



// Delivery stub: returns -1 on failure, 1 on successful delivery
int deliver(Node* node, multicast_msg_t* msg);

// ==== FUNCTIONS FOR ALGORITHM IMPLEMENTATION ==== 
// Send multicast message
int multicast(Node* node, multicast_msg_t* msg);

// Handling reception of MULTICAST messages
int handle_multicast(Node* node, multicast_msg_t* msg);

// Handling reception of PROPOSE messages
int handle_propose(Node* node, propose_msg_t* msg);

// Function to commit messages
void commit(Node *node, const multicast_msg_t *msg);

// This function handles delivering messages following total order:
// it searches for all messages that are COMMITTED but not yet delivered,
// and for each of them checks that there are no messages not yet COMMITTED
// that have a timestamp smaller than the global timestamp (in that case
// those messages must be delivered first)
void TOrder_deliver(Node *node);

#endif 
