#ifndef __RENTI_H__
#define __RENTI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

// renti
struct renti {
  pthread_t pid;
  int running;
};

struct renti *renti_init(struct ipspeaker *ex);
int renti_run(struct renti *mrenti);
int renti_exit(struct renti *mrenti);

#ifdef __cplusplus
}
#endif

#endif