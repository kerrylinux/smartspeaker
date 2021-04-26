#ifndef __PLAY_H__
#define __PLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "export.h"
#include "queue.h"

#define PLAY_WAKEUP 1
#define PLAY_DDS_MUSIC 2

enum play_status { PLAY_RUN = 0, PLAY_IDLE };

struct play_operate {
  int type;
  char* play_path;
};

struct play {
  enum play_status status;
  linkqueue* lq;
  pthread_t pid;
  int running;
};

struct play* play_init(struct export* ex);
int play_run(struct export* ex);
int play_exit(struct play* mplay);
int play_delete(struct play* mplay);
int play_delete_operate(void* arg);

#ifdef __cplusplus
}
#endif

#endif