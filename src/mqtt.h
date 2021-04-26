#ifndef __MQTT_H__
#define __MQTT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "queue.h"
#include <pthread.h>

#define MQTT_PUB 1
#define MQTT_SUB 2

#define CONNECT_RUI_SERVICE 1
#define WAKEUP_RUI_ONCE 2
#define SPEECH_RUI_ONCE 3
#define MUSIC_MUTE 4
#define MUSIC_UNMUTE 5

enum mqtt_status { MQTT_RUN = 0, MQTT_IDLE };

struct mqtt_part {
  int status;
  int mqtt_type;
  char *content;
};

struct mqtt_operate {
  int type;
  struct mqtt_part *info;
};

struct mqtt {
  enum mqtt_status status;
  linkqueue *lq;
  pthread_t pid;
  int running;
};

struct mqtt *mqtt_init(struct ipspeaker *ex);
int mqtt_run(struct mqtt *mosmqtt);
int mqtt_exit(struct mqtt *mosmqtt);
int mqtt_delete(struct mqtt *mosmqtt);
int mqtt_delete_operate(void *arg);

#ifdef __cplusplus
}
#endif

#endif