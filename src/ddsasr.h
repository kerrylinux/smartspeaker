#ifndef __ASR_H__
#define __ASR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include "dds.h"
#include "export.h"
#include "queue.h"

#define MINDARY_DUILITE_ASR_START 1
#define MINDARY_DUILITE_ASR_STOP 2
#define MINDARY_DUILITE_ASR_FEED 3
#define MINDARY_DUILITE_ASR_CANCEL 4

#define MINDARY_DUILITE_ASR_FLAG_NODE 0x00
#define MINDARY_DUILITE_ASR_FLAG_NEEDING 0x01

// asr status
enum ddsasr_status {
  DDS_STATUS_NONE = 0,
  DDS_STATUS_IDLE,
  DDS_STATUS_LISTENING,
  DDS_STATUS_UNDERSTANDING
};

enum ddsasr_status dds_status;

// data block
struct ddsasr_part {
  int len;
  char* audiodata;
};

struct ddsasr_operate {
  int type;
  struct ddsasr_part* audio;
};

// ddsasr
struct ddsasr {
  enum ddsasr_status status;
  linkqueue* lq;
  pthread_t pid;
  int running;
  int flag;
};

struct ddsasr* ddsasr_init(struct export* ex);
int ddsasr_run(struct ddsasr* masr);
int ddsasr_exit(struct ddsasr* masr);
int ddsasr_delete(struct ddsasr* masr);

#ifdef __cplusplus
}
#endif

#endif