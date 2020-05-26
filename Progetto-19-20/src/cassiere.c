//#define _POSIX_C_SOURCE  200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include "direttore.h"
#include "cliente.h"
#include "cassiere.h"
#include "utility.h"

//volatile sig_atomic_t notFinish = 1;

void printClient(queue head, int indexCas){
    printf("Cassiere %d\n", indexCas+1);
    if(head == NULL)
        printf("non ci sono clienti in fila\n");
    while(head != NULL){
        printf("cliente servito: %d\n", head->served);
        head = head->next;
    }
}

// insert the client in the tail of queue
void insertClient(queue *head, queue *tail, queue *client){
    if(*head == NULL){
        *head = *client;
        //(*head)->next = NULL;
        *tail = *head;
    }
    else{
        (*tail)->next = *client;
        (*tail) = *client;
        //(*tail)->next = NULL;
    }
    //printf("client buyProducts: %d\n", (*client)->buyProducts);
}

// return the products buy by client in the head of queue
int clientAtCashier(queue head){
    if(head == NULL)
        return -1;
    return head->buyProducts;
}

// remove the client in the head of queue
queue removeClient(queue *head, queue *tail){
    if(*head == NULL){
        perror("Error rimosso cliente da lista vuota");
        exit(EXIT_FAILURE);
    }

    if(*head == *tail){
        (*head)->served = 1;
        pthread_cond_signal(&((*head)->myTurn));
        //free(*head);
        queue aux = *head;
        *head = NULL;
        *tail = NULL;
        return aux;
    }

    //queue aux = malloc(sizeof(nodoC));
    queue aux = *head;
    //aux->next = NULL;
    (*head)->served = 1;
    pthread_cond_signal(&((*head)->myTurn));
    *head = (*head)->next;
    return aux;
}

// function for the support thread of cashier,
//  notify the manager with the cleints in the queue
void *waitTimer(void *arg){
    threadCassiere *tc = arg;
    struct timespec t1;
    t1.tv_sec = 0;
    t1.tv_nsec = tc->intervalTime;
    //pthread_detach(pthread_self());
    while(1){
        nanosleep(&t1, NULL);
                // Qui la lock la devo mettere?
        //pthread_mutex_lock(&(tc->lock));
        updateCashiers(tc->queueSize, tc->index);
        //pthread_mutex_unlock(&(tc->lock));
        pthread_mutex_lock(&(tc->lock));
        //nF = tc->notFinish;
        if(tc->notFinish == 0){
            printf("Sto terminando, sono il support\n");
            pthread_mutex_unlock(&(tc->lock));
            //pthread_exit(NULL);
            return NULL;
        }
        pthread_mutex_unlock(&(tc->lock));
        //printf("Allarme cassiere %ld\n", id);
    }
    printf("Support terminato\n");
    //pthread_exit(NULL);
    return NULL;
}

void deleteNotifyTimer(threadCassiere *tc){
    pthread_mutex_lock(&(tc->lock));
    tc->notFinish = 0;
    pthread_cond_signal(&(tc->notEmpty));
    pthread_mutex_unlock(&(tc->lock));
    int status;
    //pthread_detach(tc->sup);
    pthread_join(tc->sup, (void*) &status);
    /*if( pthread_join(tc->sup, (void*) &status) != 0){
        perror("pthread join support");
        exit(EXIT_FAILURE);
    }*/
    printf("terminato support\n");
    /*if( status != 0){
        perror("Error pthread join support");
        exit(EXIT_FAILURE);
    }*/
    //printf("Support terminato\n");
}

void *cassiere_work(void *arg){
    threadCassiere *tc = arg;
    struct timespec t1;
    queue client;
    pthread_t notifyTimer;
    int tService, nProducts;
    pthread_detach(pthread_self());

    pthread_mutex_lock(&(tc->lock));
    tc->reg = pthread_self();
    srand(time(NULL));
    //printf("IL cassiere %ld, è entrato in servizio\n", pthread_self());

    // create the support thread for cashier
    if( pthread_create(&notifyTimer, NULL, &waitTimer, (void*) tc) != 0 ){
        perror("Error create support thread reg");
        exit(EXIT_FAILURE);
    }
    pthread_detach(notifyTimer);
    tc->sup = notifyTimer;
    //nF = tc->notFinish;
    pthread_mutex_unlock(&(tc->lock));

    while(1){
        pthread_mutex_lock(&(tc->lock));
        if(tc->notFinish == 0){
            printf("Sto terminando, sono il cassiere\n");
            pthread_mutex_unlock(&(tc->lock));
            return NULL;
        }
        while(tc->head == NULL && (tc->notFinish == 1)){ // or || tc->notFinish == 0
            pthread_cond_wait(&(tc->notEmpty), &(tc->lock));
        }
        if(tc->notFinish == 0){
            printf("Sto terminando, sono il cassiere\n");
            pthread_mutex_unlock(&(tc->lock));
            return NULL;
        }

        if ( (nProducts =  clientAtCashier(tc->head))== -1){
            perror("Error clientAtCashier ");
            exit(EXIT_FAILURE);
        }

        //printf("Ho un nuovo cliente da servire, sono il cassiere : %ld\n", pthread_self());
        // time for serve the client

        tService = (tc->fixedTime + tc->productTime * nProducts) * 1000000;
        if(tService >= 1000000000 ){
            t1.tv_sec = tService / 1000000000;
            t1.tv_nsec = (tService % 1000000000);
        }
        else{
            t1.tv_sec = 0;
            t1.tv_nsec = tService;
        }
        //printf("            Aspetto %d ms\n", client->serviceTime);
        nanosleep(&t1, NULL);

        client = removeClient(&(tc->head), &(tc->tail));

        pthread_mutex_lock(&(client->lock));
        client->serviceTime = (int)t1.tv_nsec;
        client->supermarketTime = client->serviceTime + client->buyTime + client->queueTime;
        pthread_mutex_unlock(&(client->lock));
        // change remove function for return nodoC
        //removeClient(&(tc->head), &(tc->tail));
        tc->queueSize--;
        //printf("        tempo di attesa in coda del client: %d\n", client->queueTime);

        pthread_mutex_unlock(&(tc->lock));
        //printf("Ho servito %d clienti, sono il cassiere : %ld\n", count, pthread_self());
    }
    printf("Sto terminando, sono il cassiere\n");
    free(client);
    //free(tc);
    //pthread_exit(NULL);
    return NULL;
}

// initialize register variable
void cassiere_create(threadCassiere *tc, int tprod, int intTime, int index){
    tc->queueSize = 0;
    tc->fixedTime = rand()% ((80 +1) -20) +20;
    tc->productTime = tprod;
    tc->intervalTime = intTime;
    tc->notFinish = 1;

    // info for log file
    tc->clientServed = 0;
    tc->productsBuy = 0;
    tc->openTime = 0;

    if( pthread_mutex_init(&(tc->lock), NULL) != 0){
        perror("Error initialize lock");
        exit(EXIT_FAILURE);
    }
    if( pthread_cond_init(&(tc->notEmpty), NULL) != 0){
        perror("Error initialize var cond");
        exit(EXIT_FAILURE);
    }
    tc->head = NULL;
    tc->tail = NULL;
    tc->index = index;
}
