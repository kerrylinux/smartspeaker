
#ifndef __QUEUE_H__
#define __QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*delete_callback)(void *data);
typedef struct link linkqueue;

linkqueue * newQueue();
void inQueue(linkqueue *q, void * data);
void * outQueue(linkqueue *q);
void deleteQueue(linkqueue *q, delete_callback callback);

#ifdef __cplusplus
}
#endif

#endif