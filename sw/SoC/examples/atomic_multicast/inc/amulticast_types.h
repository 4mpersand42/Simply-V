#ifndef _AMCAST_TYPES_H_
#define _AMCAST_TYPES_H_
#include <stdlib.h>
#include <stdbool.h>

#define MAXPAYLOAD_LEN 20
#define MAX_NUMBER_OF_GROUPS 8 //Per ora suppongo che ogni gruppo sia formato da un unico processo reliable che quindi non può fallire 
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


// Struttura del messaggio multicast: 
// Questo messaggio deve includere un campo id per poterlo identificare nella coda 
// il campo payload che contiene il messaggio effettivo da consegnare
// forse per renderlo più compatibile, il campo payload 
// in futuro potrà essere un puntatore a void
// il vettore di destinazioni con il relativo campo per il contatore
typedef struct {
    msgtype_t type;
    int msg_id;
    payload_t payload;
    g_id_t dstgrp[MAX_NUMBER_OF_GROUPS];
    int dst_count;
}multicast_msg_t;


// Struttura del messaggio propose:
// questo tipo di messaggio viene usato per inviare 
// la porposta per il timestamp da assegnare ad un 
// messaggio di cui è stato precedentemente fatto il multicast
// deve quindi contenere il msg_id del messaggio mandato in broadcast
// il group id del gruppo che ha fatto la proposta
typedef struct {
    msgtype_t type;
    int msg_id;
    g_id_t g_id;
    ts_t lts;
}propose_msg_t;

// Struttura NODE: Questa struct servirà ad astrarre tutte quelle che sono le variabili 
// locali ad un processo per evitare l'utilizzo di variabili globali.
// Per poter implementare l'algoritmo di Skeen, ogni nodo dovrà avere: 
// - un id 
// - un vettore lts dove conservare i local timestamps dei messaggi ricevuti
// - un vettore gts contenente i global timestamps dei messaggi ricevuti
// - un vettore indicizzato dagli id dei messaggi che tiene traccia 
//   delle diverse fasi in cui si trovano.
// - Una coda di messaggi in cui vengono messi i messaggi
//   in attesa di essere consegnati.
// - il clock locale 
// - variabili per tenere sotto controllo i boundaries dei vettori lts e gts: lts_count gts_count
typedef struct {
    g_id_t g_id;
    clk_t clock;
    ts_t lts[MAX_MESSAGES];
    ts_t gts[MAX_MESSAGES];
    phase_t phase[MAX_MESSAGES];
    multicast_msg_t msg_queue[MAX_MESSAGES];
    bool delivered[MAX_MESSAGES];    
    // PROPOSE per-messaggio (necessario per all-to-all)
    propose_msg_t props[MAX_MESSAGES][MAX_NUMBER_OF_GROUPS];
    int propose_count[MAX_MESSAGES];
    
    int delivered_count;
    int lts_count;
    int gts_count; 
    int phase_count; 
    int queue_count;
}Node;


// ==== FUNZIONI PER LA GESTIONE DELLE STRUTTURE ====

// Costruttore del nodo
Node *init_node(g_id_t g_id);
// Distruttore del nodo
void destroy_node(Node* node);

// Funzione per la creazione di un messaggio multicast: 
// per costruire questo messaggio è necessario fornire il payload,
// l'id del messaggio e la lista di destinatari
multicast_msg_t *create_multicast_msg(payload_t payload, int id, const g_id_t *dstgrp, int dst_count);
// Distruttore del messaggio multicast
void destroy_multicast_msg(multicast_msg_t *msg);

// Costruttore del messaggio propose
propose_msg_t *create_propose_msg(int id, g_id_t g_id, ts_t lts);
// Distruttore del messaggio propose
void destroy_propose_msg(propose_msg_t *msg);

// Funzione che restituisce true se due timestamp hanno lo stesso clock
// e lo stesso g_id
bool timestamp_cmp(ts_t a, ts_t b);

// Funzione che riceve in ingresso due timestamps 
// e restituisce quello con il clock più grande
ts_t timestamp_max(ts_t a, ts_t b);

// funzione per la copia di messaggi di tipo multicast
void multicast_msg_cpy(const multicast_msg_t *src, multicast_msg_t *dst);

// funzione per la copia di messaggi di tipo propose
void propose_msg_cpy(const propose_msg_t *src, propose_msg_t *dst);



// Stub per consegna restituisce -1 se fallisce, 0 se l'invio ha successo
int deliver(Node* node, multicast_msg_t* msg);

// ==== FUNZIONI PER IMPLEMENTAZIONE ALGORITMO ==== 
// Invio messaggio multicast 
int multicast(Node* node, multicast_msg_t* msg);

//  Gestione ricezione messaggi di tipo MULTICAST
int handle_multicast(Node* node, multicast_msg_t* msg);

//  Gestione ricezione messaggi di tipo PROPOSE
int handle_propose(Node* node, propose_msg_t* msg);

//  Funzione per il commit dei messaggi 
void commit(Node *node, const multicast_msg_t *msg);

//  Questa funzione si occupa di fare il deliver dei messaggi 
//  seguendo il total order: cerca tutti i messaggi che sono stati 
//  COMMITTED ma non ancora delivered, per ognuno di questi, verifica 
//  che non vi siano messaggi non ancora COMMITTED che abbiano timestamp minore 
//  del global timestamp (in tal caso significa che questi messaggi dovranno essere delivered prima)
void TOrder_deliver(Node *node);

#endif 
