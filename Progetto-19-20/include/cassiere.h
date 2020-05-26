#ifndef cassiere_h
#define cassiere_h

#include "utility.h"

// create new cashier
void cassiere_create(threadCassiere *tc, int tprod, int intTime, int index);

// return the products buy by client in the head of queue
int clientAtCashier(queue head);

void printClient(queue head, int indexCas);

// insert client on tail in the queue of cashier
void insertClient(queue *head, queue *tail, queue *client);

// remove and return the client on head in the queue of register
queue removeClient(queue *head, queue *tail);

// register serves the client in the queue one at time
void *cassiere_work(void *arg);

void deleteNotifyTimer(threadCassiere *tc);

void *waitTimer(void *arg);

#endif /* cassiere_h */
