#ifndef __connect_H__
#define __connect_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ipspeaker.h"
#include "queue.h"
#include <pthread.h>

#define RUI_CONNECT_STATUS 1
#define RUI_DISCONNECT_STATUS 2

enum connect_status { CONNECT_STATUS = 0, DISCONNECT_STATUS = 1 };

struct connect_operate {
  int type;
};

struct connect {
  enum connect_status status;
  linkqueue *lq;
  pthread_t pid;
  int running;
};

struct connect *connect_init(struct ipspeaker *ex);
int connect_run(struct ipspeaker *ex);
int connect_exit(struct connect *mconnect);
int connect_delete(struct connect *mconnect);
int connect_delete_operate(void *arg);

#ifdef __cplusplus
}
#endif

#endif