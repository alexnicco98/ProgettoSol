#ifndef direttore_h
#define direttore_h

// life cycle of manager
void *direttore(void *arg);

// this function is used by cashier to notify the manager
void updateCashiers(int queueSize, int index);

// clean pthread_t of client with this index
void clientExit(int index);

void updateInfoClients(pthread_t idClient, int buyProducts, int changedQueue, float supermarketTime, float queueTime);

void updateInfoCashiers(int index, int processedProducts, int clientServed, float openTime, float averangeTimeService);

#endif /* direttore_h */
