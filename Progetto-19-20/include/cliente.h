#ifndef cliente_h
#define cliente_h

#include "utility.h"

// life cycle of client
void *client(void *arg);

// insert the client in cashier's tail of that index
void insertInCashiersQueue(queue *client, int index, var_struct *vcm, threadCassiere *registers);

#endif /* cliente_h */
