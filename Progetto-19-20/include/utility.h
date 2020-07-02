#ifndef utily_h
#define utily_h

#include <pthread.h>

struct vcm{
    int numClient;                            // number of clients in the supermarket
    int openCashiers;                         // number of open cashiers
    volatile sig_atomic_t finish;             // 1 if the supermarket is closing
    volatile sig_atomic_t sigquitReceived;    // 1 if the manager received the SIGQUIT signal
    pthread_mutex_t mtx;
    pthread_cond_t NotFullClient;
    pthread_cond_t Empty;
};

typedef struct vcm var_struct;

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
    int buyProducts;         // number of products buy
    int served;              // served client = 1, otherwise 0
    int changedQueue;        // numbers of changed queue
    float buyTime;           // time for buy products
    float serviceTime;       // time of service
    float supermarketTime;   // time permanence of the client in the supermarket
    float queueTime;         // time in queue
    pthread_mutex_t lock;
    pthread_cond_t myTurn;
    pthread_cond_t modified;
    struct nodo *next;
};

typedef struct nodo nodoC;
typedef nodoC *queue;

// struct for save info of client after terminate
struct node{
    pthread_t idClient;     // id client
    int buyProducts;        // number of products buy
    int changedQueue;       // numbers of changed queue
    float supermarketTime;  // time permanence of the client in the supermarket
    float queueTime;        // time in queue
    struct node *next;
};

typedef struct node nodoInfoClient;
typedef nodoInfoClient *infoQueueClient;

// struct for save info of cashier after terminate
struct node1{
    int idCashier;              // id cashier
    int processedProducts;      // number of processed products
    int clientServed;           // number of clients
    int numberClosures;         // number of closures
    float openTime;             // open time cashier
    float averangeTimeService;  // averange time of service
};

typedef struct node1 nodoInfoCashier;
typedef nodoInfoCashier *infoQueueCashier;

struct tC{
    int index;                        // index of cashier
    int queueSize;                    // queue size
    int fixedTime;                    // fixed time
    int productTime;                  // product time
    int intervalTime;                 // interval time
    volatile sig_atomic_t notFinish;  // 1 if supermarket is open, 0 otherwise
    pthread_t reg;                    // cashier thread
    pthread_t sup;                    // support thread for notify the manager
    pthread_mutex_t lock;
    pthread_cond_t notEmpty;
    queue head;                      // queue of clients
    queue tail;                      // queue of clients
};

typedef struct tC threadCassiere;

// struct info flow between clients and cashiers
struct var_cashier_client{
    int index;
    var_struct *vcm;
    threadCassiere *regs;
};

// struct info flow to manager
struct var_manager{
    var_struct *vcm;
    infoQueueClient iqClientHead;
    infoQueueClient iqClientTail;
    infoQueueCashier *iqCashier;
};

#endif /* utily_h */
