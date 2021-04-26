#ifndef __VAD_H__
#define __VAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "export.h"
#include "queue.h"
#include <pthread.h>

#define MINDARY_DUILITE_VAD_START 1
#define MINDARY_DUILITE_VAD_STOP 2
#define MINDARY_DUILITE_VAD_FEED 3

#define MINDARY_DUILITE_VAD_FLAG_NODE 0x00
#define MINDARY_DUILITE_VAD_FLAG_FEEDING 0x01

// vad status
enum vad_status { VAD_STATUS_IDLE, VAD_STATUS_BUSY, VAD_STATUS_WAIT };

// data block
struct vad_part {
  int len;
  char *audiodata;
};

struct vad_operate {
  int type;
  struct vad_part *audio;
};

// vad
struct vad {
  enum vad_status status;
  struct duilite_vad *vad;
  linkqueue *lq;
  pthread_t pid;
  int running;
  int flag;
};

struct vad *vad_init(struct export *ex);
int vad_run(struct vad *mvad);
int vad_exit(struct vad *mvad);
int vad_delete(struct vad *mvad);
int vad_delete_operate(void *arg);

#ifdef __cplusplus
}
#endif

#endif