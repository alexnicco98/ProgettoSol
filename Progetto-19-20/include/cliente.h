#ifndef cliente_h
#define cliente_h

#include "utility.h"

// create new client
void *client(void *arg);

// change the register to the client, and put the
// client on cashier's tail
void insertInCashiersQueue(queue *client, var_client_manager *vcm, threadCassiere *registers);

#endif /* cliente_h */
