#ifndef __WAKEUP_H__
#define __WAKEUP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "export.h"
#include "queue.h"
#include <pthread.h>

#define MINDARY_DUILITE_WAKEUP_START 1
#define MINDARY_DUILITE_WAKEUP_STOP 2
#define MINDARY_DUILITE_WAKEUP_FEED 3

// wakeup status
enum wakeup_status {
  WAKEUP_STATUS_IDLE,
  WAKEUP_STATUS_BUSY,
  WAKEUP_STATUS_WAIT
};

// data block
struct wakeup_part {
  int len;
  char *audiodata;
};

struct wakeup_operate {
  int type;
  struct wakeup_part *audio;
};

// wakeup
struct wakeup {
  enum wakeup_status status;
  struct duilite_fespl *wakeup;
  linkqueue *lq;
  pthread_t pid;
  int running;
};

struct wakeup *wakeup_init(struct export *ex);
int wakeup_run(struct wakeup *mwakeup);
int wakeup_exit(struct wakeup *mwakeup);
int wakeup_delete(struct wakeup *mwakeup);

#ifdef __cplusplus
}
#endif

#endif