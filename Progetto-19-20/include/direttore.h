#ifndef direttore_h
#define direttore_h

void *direttore(void *arg);

void updateCashiers(int queueSize, int index);

void openCashier();

void closeCashier();

void moveClient(int index);

void checkCashiers();

void handlerSigQuit(int sig);

void handlerSigHup(int sig);

void clientExit(int index);

#endif /* direttore_h */
