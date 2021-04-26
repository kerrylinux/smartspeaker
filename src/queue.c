#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct queuenode
{
    void * data;
    struct queuenode *next;
}Queue;

struct link
{
    Queue *fronts,*rear;
    pthread_mutex_t mutex;
};

linkqueue * newQueue(){
    linkqueue * queue = (linkqueue *)malloc(sizeof(linkqueue));
    queue->fronts = NULL;
    queue->rear = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    return queue;
}

void inQueue(linkqueue *q, void * data){
    pthread_mutex_lock(&q->mutex);
    Queue *p;
    p = (Queue *)malloc(sizeof(Queue));
    p->data = data;
    p->next = NULL;
    if(q->fronts == NULL)
        q->fronts = p;
    else
        q->rear->next = p;
    q->rear = p;
    pthread_mutex_unlock(&q->mutex);
}

void * outQueue(linkqueue *q){
    pthread_mutex_lock(&q->mutex);
    Queue *p;
    void * data = NULL;
    if(q->fronts == NULL){
    }else{
        p = q->fronts;
        data = p->data;
        q->fronts = p->next;
        if(q->fronts == NULL)
            q->rear = NULL;
        free(p);
        p = NULL;
    }
    pthread_mutex_unlock(&q->mutex);
    return data;
}

void deleteQueue(linkqueue *q, delete_callback callback){
    if(callback){
        while(1){
            void * data = outQueue(q);
            if (!data) break;
            callback(data);
        }
    }
    pthread_mutex_destroy(&q->mutex);
    free(q);
    q = NULL;
}

