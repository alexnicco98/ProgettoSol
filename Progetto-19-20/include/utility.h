#ifndef utily_h
#define utily_h

#include <pthread.h>

struct vcm{
    int numClient;                  // number of clients in the supermarket
    int openCashiers;               // number of open cashiers
    volatile sig_atomic_t finish;   // 1 if the supermarket is closing
    pthread_mutex_t mtx;
    pthread_cond_t NotFullClient;   // if number of clients <= C - E
    pthread_cond_t Empty;           // if queue cashier is empty
};

// variable that not change
extern int productTime;
extern int intervalTime;
extern int C;
extern int E;
extern int K;
extern int P;
extern int T;
extern int S1;
extern int S2;

struct nodo{
    int buyProducts;        // number of products buy
    int served;             // served client = 1, otherwise 0
    int buyTime;            // time for buy products
    int serviceTime;        // time of service
    int supermarketTime;    // time permanence of the client in the supermarket
    int queueTime;          // time in queue
    int changedQueue;       // numbers of changed queue
    pthread_mutex_t lock;
    pthread_cond_t myTurn;
    struct nodo *next;
};

typedef struct nodo nodoC;
typedef nodoC *queue;

struct tC{
    int queueSize;          // clienti in coda
    int fixedTime;          // tempo fisso del cassiere
    int productTime;        // tempo per un singolo prodotto
    int intervalTime;       // intervallo regolare per informare il manager
    int clientServed;       // numero di clienti serviti
    int productsBuy;         // numero totale di prodotti acquistati dai clienti
    int openTime;            // tempo in cui è stato aperto
    int index;              // indice relativo al thread cassiere
    volatile sig_atomic_t notFinish;
    pthread_t reg;          // thread cassiere
    pthread_t sup;          // thread di supporto per invio notifiche
    pthread_mutex_t lock;
    pthread_cond_t notEmpty;
    queue head;
    queue tail;
};

typedef struct vcm var_client_manager;

typedef struct tC threadCassiere;

struct vcmAndCashiers{
    int index;
    var_client_manager *vcm;
    threadCassiere *regs;
};

#endif /* utily_h */
