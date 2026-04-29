#include "amulticast_types.h"
#include "sendReceive.h"
#include "simplyv.h"

// ==== FUNZIONI PER LA GESTIONE DELLE STRUTTURE ====

// Costruttore del nodo
void init_node(Node &node, g_id_t g_id){
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

// Distruttore del nodo
/*void destroy_node(Node* node){
    free(node);
}*/



// Inizializzatore del messaggio: Da rivedere 
void create_multicast_msg(multicast_msg_t &msg, payload_t payload, int id, const g_id_t *dstgrp, int dst_count){
    msg->type = MULTICAST;
    msg->msg_id=id;
    strcpy(msg->payload, payload);
    for (int i = 0;i<dst_count;i++){
        msg->dstgrp[i] = dstgrp[i];
    }
    msg->dst_count=dst_count;
}
// Distruttore del messaggio
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
}


void create_propose_msg(propose_msg_t &msg, int id, g_id_t g_id, ts_t lts){
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
}

uint8_t timestamp_cmp(ts_t a, ts_t b){
    return a.clock == b.clock && a.g_id == b.g_id;
}

ts_t timestamp_max(ts_t a, ts_t b){
    if (a.clock > b.clock) return a;
    else return b;
}





// Stub per consegna restituisce -1 se fallisce, 1 se l'invio ha successo
int deliver(Node* node, multicast_msg_t* msg){
    if(msg != NULL){
        printf("PID:%d Messaggio con timestamp: g_id %d, clock:%d consegnato\n",getpid(),node->gts[msg->msg_id].g_id,node->gts[msg->msg_id].clock);
        printf("CONTENUTO DEL MESSAGGIO: %s\n",msg->payload);
        return 1;
    }
    else 
        return -1;
}
// ==== FUNZIONI PER IMPLEMENTAZIONE ALGORITMO ==== 
// Invio messaggio multicast: Invia il messaggio multicast 
// ad ogni destinatario indicato nel messaggio
int multicast(Node* node, multicast_msg_t* msg){
    int err;
    printf("Il processo con PID: %d invia un messaggio MULTICAST con id: %d\n",getpid(),msg->msg_id);
    for(int i = 0; i<msg->dst_count;i++){
        err = acast_send(node, msg,MULTICAST,msg->dstgrp[i]);
        if (err < 0) return -1;
    }
    return 1;
}

//  Gestione ricezione messaggi di tipo MULTICAST
//  Questa funzione si occupa solamente di aggiungere il messaggio multicast al
//  nodo e di aggiornarne i campi.
//  Dopo aver ricevuto il messaggio multicast, il processo chiamante dovrà 
//  inviare le porposte di timestamp a tutti i destinatari del messaggio 
int handle_multicast(Node* node, multicast_msg_t* msg){
    printf("Il processo con PID: %d ha ricevuto un messaggio MULTICAST con id: %d\n",getpid(),msg->msg_id); // # DEBUG
    
    node->clock++; // incrementa il clock LOCALE
    node->lts[msg->msg_id].clock = node->clock; // |
    node->lts[msg->msg_id].g_id = node->g_id;   // | salva il timestamp nel vettore dei local timestamp
    node->lts_count++; 
    node->phase[msg->msg_id] = PROPOSED;
    node->phase_count++;
    multicast_msg_cpy(msg, &node->msg_queue[msg->msg_id]); // aggiungi il messaggio al buffer 
    node->queue_count++;
    printf("PID:%d, proposed timestamp: (g_id:%d ,clock:%d )\n",getpid(),node->g_id,node->clock);
    return 1;
}
//  Gestione ricezione messaggi di tipo PROPOSE
//  Questa funzione si occupa solo di aggiornare i campi
//  del nodo alla ricezione di ogni proposta.
int handle_propose(Node* node, propose_msg_t* msg){
    printf("Il processo con PID: %d ha ricevuto un messaggio PROPOSE con id: %d\n",getpid(),msg->msg_id);
    int id = msg->msg_id;
    int count = node->propose_count[id];

    if(count >= MAX_NUMBER_OF_GROUPS) return -1;


    // Copia il messaggio in coda
    propose_msg_cpy(msg, &node->props[id][count]);
    // Incrementa il contatore delle proposte ricevute 
    node->propose_count[id]++;
    // Se abbiamo tutte le propose dai destinatari, possiamo committare
    if(node->propose_count[id] == node->msg_queue[id].dst_count){
        commit(node, &node->msg_queue[id]);
        TOrder_deliver(node);
    }

    return 1;
}

void commit(Node* node, const multicast_msg_t *msg){
    int id = msg->msg_id;
    // Calcola il global timestamp come il timestamp con clock maggiore nel vettore di proposte
    unsigned int max_clock = 0;
    int index;
    for(int i = 0; i < node->propose_count[id]; i++){
        if(max_clock < node->props[id][i].lts.clock){
            max_clock = node->props[id][i].lts.clock;
            index = i;
        }
    }
    node->gts[id].clock = node->props[id][index].lts.clock;
    node->gts[id].g_id = node->props[id][index].lts.g_id;
    node->gts_count++;
   // Aggiorna il clock
    if(node->clock<node->gts[msg->msg_id].clock) {
        node->clock = node->gts[msg->msg_id].clock;
    }
    // Committa il messaggio
    node->phase[msg->msg_id]=COMMITTED;
    printf("Il processo con PID: %d ha fatto il commit con id: %d\n",getpid(),msg->msg_id);

}

static uint8_t ts_less(ts_t a, ts_t b) {
    if (a.clock < b.clock) return true;
    if (a.clock > b.clock) return false;
    return a.g_id < b.g_id;
}

void TOrder_deliver(Node *node){
    printf("Il processo con PID: %d ha iniziato la consegna dei messaggi...\n",getpid());

    for(int i = 0; i < node->phase_count; i++){
        if(node->phase[i] == COMMITTED && node->delivered[i] == 0){

            uint8_t can_deliver = true;

            for(int j = 0; j < node->phase_count; j++){
                if(node->phase[j] == PROPOSED && ts_less(node->lts[j], node->gts[i])){
                    // esiste un messaggio ancora non committed che dovrebbe venire prima
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
    printf("Il processo con PID: %d consegna dei messaggi completata\n",getpid());
}