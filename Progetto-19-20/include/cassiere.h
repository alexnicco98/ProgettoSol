#ifndef cassiere_h
#define cassiere_h

#include "utility.h"

// initialize cashier variable
void cassiere_create(threadCassiere *tc, int tprod, int intTime, int index);

// insert client in the tail of queue
void insertClient(queue *head, queue *tail, queue *client);

// life cycle cashier
void *cassiere_work(void *arg);

#endif /* cassiere_h */
