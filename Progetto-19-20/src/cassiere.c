#define _POSIX_C_SOURCE  200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include "direttore.h"
#include "cassiere.h"
#include "utility.h"

_Thread_local unsigned int seed1;           // seed thread local for rand_r

// insert client in the tail of queue
void insertClient(queue *head, queue *tail, queue *client){
    if(*head == NULL){
        *head = *client;
        *tail = *head;
    }
    else{
        (*tail)->next = *client;
        (*tail) = *client;
    }
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
        perror("        Error rimosso cliente da lista vuota");
        exit(EXIT_FAILURE);
    }

    if(*head == *tail){
        pthread_mutex_lock(&(*head)->lock);
        (*head)->served = 1;
        pthread_cond_signal(&((*head)->myTurn));
        pthread_mutex_unlock(&(*head)->lock);
        queue aux = *head;
        *head = NULL;
        *tail = NULL;
        return aux;
    }

    queue aux = *head;
    pthread_mutex_lock(&(*head)->lock);
    (*head)->served = 1;
    pthread_cond_signal(&((*head)->myTurn));
    pthread_mutex_unlock(&(*head)->lock);
    *head = (*head)->next;
    return aux;
}

// function for support thread of cashier, notify the manager
void *waitTimer(void *arg){
    threadCassiere *tc = arg;
    struct timespec t1;
    int oldType;
    pthread_detach(pthread_self());
    if( pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldType) != 0){
        perror("Error setcanceltype");
        exit(EXIT_FAILURE);
    }
    // block signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGQUIT);
    if( pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
        perror("Error pthread sigmask");
        exit(EXIT_FAILURE);
    }

    t1.tv_sec = 0;
    t1.tv_nsec = tc->intervalTime*100000000;
    // dalay for inizializate cashier
    nanosleep(&t1, NULL);
    t1.tv_nsec = tc->intervalTime*1000000;

    //notify manager at regular interval time
    while(1){
        nanosleep(&t1, NULL);
        updateCashiers(tc->queueSize, tc->index);
    }
    return NULL;
}

// life cycle cashier
void *cassiere_work(void *arg){
    threadCassiere *tc = arg;
    struct timespec t1;
    struct timespec openTime;
    struct timespec closeTime;
    queue client;
    float diff, tService, tServiceTotal = 0;
    int nProducts,clientServed = 0, totalProducts = 0, oldType;

    if( pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldType) != 0){
        perror("        Error setcanceltype");
        exit(EXIT_FAILURE);
    }

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGQUIT);
    if( pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
        perror("        Error pthread sigmask");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&(tc->lock));
    tc->reg = pthread_self();

    // create the support thread for cashier
    if( pthread_create(&(tc->sup), NULL, &waitTimer, (void*) tc) != 0 ){
        perror("        Error create support thread reg");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&(tc->lock));
    // take start time of cashier
    clock_gettime(CLOCK_MONOTONIC, &openTime);

    while(1){
        pthread_mutex_lock(&(tc->lock));
        // wait clients, or to terminate
        while(tc->head == NULL && tc->notFinish == 1){
            pthread_cond_wait(&(tc->notEmpty), &(tc->lock));
        }
        if(tc->notFinish == 0){
            #if defined(DEBUG)
            printf("        [Cassiere %d] sto terminando\n", tc->index+1);
            #endif
            // take end time of cashier
            clock_gettime(CLOCK_MONOTONIC, &closeTime);
            diff = (closeTime.tv_sec - openTime.tv_sec) ;
            if( (closeTime.tv_nsec -openTime.tv_nsec) < 0)
                diff += ((float)( closeTime.tv_nsec -openTime.tv_nsec) *(-1) / 1000000000);
            else
                diff += ((float)( closeTime.tv_nsec -openTime.tv_nsec) / 1000000000);
            updateInfoCashiers(tc->index, totalProducts, clientServed, diff, (float)tServiceTotal/1000000000);
            pthread_mutex_unlock(&(tc->lock));
            return NULL;
        }

        if ( (nProducts =  clientAtCashier(tc->head))== -1){
            perror("        Error clientAtCashier ");
            exit(EXIT_FAILURE);
        }
        totalProducts += nProducts;

        // calculate service time
        tService = (tc->fixedTime + tc->productTime * nProducts) * 1000000;
        if(tService >= 1000000000 ){
            t1.tv_sec = tService / 1000000000;
            t1.tv_nsec = ((int)tService % 1000000000);
        }
        else{
            t1.tv_sec = 0;
            t1.tv_nsec = tService;
        }

        // simulate to serve a client
        nanosleep(&t1, NULL);

        // save statistics of work
        clientServed++;
        tServiceTotal += tService;

        client = removeClient(&(tc->head), &(tc->tail));

        #if defined(DEBUG)
        printf("        [Cassiere %d] sto servendo un nuovo cliente\n", tc->index+1);
        #endif
        pthread_mutex_lock(&(client->lock));
        // served client and now wait client that calculate his queue time
        while( client->queueTime == -1)
            pthread_cond_wait(&(client->modified), &(client->lock));
        client->serviceTime = (float)tService / 1000000000;
        client->supermarketTime = client->serviceTime + client->buyTime + client->queueTime;

        pthread_cond_signal(&(client->modified));
        pthread_mutex_unlock(&(client->lock));
        tc->queueSize--;

        pthread_mutex_unlock(&(tc->lock));
        fflush(stdout);
    }
    return NULL;
}

// initialize cashier variable
void cassiere_create(threadCassiere *tc, int tprod, int intTime, int index){
    seed1 = (unsigned int) pthread_self()*(index+1)*10000;
    srand(seed1);
    tc->queueSize = 0;
    tc->fixedTime = rand_r(&seed1)% ((80 +1) -20) +20;
    tc->productTime = tprod;
    tc->intervalTime = intTime;
    tc->notFinish = 1;

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
