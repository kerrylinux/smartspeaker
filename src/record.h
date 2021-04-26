#ifndef __RECORD_H__
#define __RECORD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "export.h"
#include "queue.h"
#include <pthread.h>

#define RUI_RECORD_START 1
#define RUI_RECORD_STOP 2

struct record_operate {
  int type;
};

struct record {
  linkqueue *lq;
  pthread_t pid;
  int running;
};

struct record *record_init(struct export *ex);
int record_run(struct record *mrecord);
int record_exit(struct record *mrecord);
int record_delete(struct record *mrecord);
int record_delete_operate(void *arg);

#ifdef __cplusplus
}
#endif

#endif