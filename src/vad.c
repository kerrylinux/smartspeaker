#include "vad.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "ddsasr.h"
#include "duilite.h"

static char *vad_cfg =
    "{\"resBinPath\":\"/speech/vad/"
    "vad.bin\",\"pauseTime\": 1200,\"fullMode\": 0}";
static char *vad_gram = "{\"pauseTime\": 1200}";
static char *vad_tag = "m-vad-v0.1.1";

int vad_delete_operate(void *arg) {
  struct vad_operate *data = (struct vad_operate *)arg;
  if (data) {
    if (data->audio) {
      if (data->audio->audiodata) {
        free(data->audio->audiodata);
        data->audio->audiodata = NULL;
      }
      free(data->audio);
      data->audio = NULL;
    }
    free(data);
    data = NULL;
  }
  return 0;
}

static int vad_callback(void *user_data, int type, char *msg, int len) {
  struct export *ex = (struct export *)user_data;
  if (type == DUILITE_MSG_TYPE_JSON) {
    printf("%.*s\n", len, msg);
    cJSON *msg_js = cJSON_Parse(msg);
    int status = cJSON_GetObjectItem(msg_js, "status")->valueint;
    switch (status) {
    case 1: {
      // ex->vad->flag = 1;
      printf("ddsasr start\n");
      struct ddsasr_operate *data =
          (struct ddsasr_operate *)malloc(sizeof(struct ddsasr_operate));
      assert(data);
      data->type = MINDARY_DUILITE_ASR_START;
      data->audio = NULL;
      inQueue(ex->ddsasr->lq, (void *)data);
      break;
    }
    case 2: {
      ex->vad->flag = 0;
      printf("ddsasr end\n");
      struct ddsasr_operate *data =
          (struct ddsasr_operate *)malloc(sizeof(struct ddsasr_operate));
      data->type = MINDARY_DUILITE_ASR_STOP;
      data->audio = NULL;
      inQueue(ex->ddsasr->lq, (void *)data);

      struct vad_operate *end_data =
          (struct vad_operate *)malloc(sizeof(struct vad_operate));
      end_data->type = MINDARY_DUILITE_VAD_STOP;
      end_data->audio = NULL;
      inQueue(ex->vad->lq, (void *)end_data);
      // printf("\n");
      break;
    }
    default:
      break;
    }
    cJSON_Delete(msg_js);
    msg_js = NULL;
  } else {
    struct ddsasr_operate *data =
        (struct ddsasr_operate *)malloc(sizeof(struct ddsasr_operate));
    data->type = MINDARY_DUILITE_ASR_FEED;
    data->audio = (struct ddsasr_part *)malloc(sizeof(struct ddsasr_part));
    data->audio->len = len;
    data->audio->audiodata = (char *)malloc(len);
    memcpy(data->audio->audiodata, msg, len);
    inQueue(ex->ddsasr->lq, (void *)data);
  }
  return 0;
}

static void *_vad_run(void *arg) {
  struct vad *mindary_vad = (struct vad *)arg;
  while (mindary_vad->running) {
    struct vad_operate *data = (struct vad_operate *)outQueue(mindary_vad->lq);
    if (data) {
      switch (data->type) {
      case MINDARY_DUILITE_VAD_START: {
        if (mindary_vad->status != VAD_STATUS_BUSY) {
          duilite_vad_start(mindary_vad->vad, vad_gram);
          mindary_vad->status = VAD_STATUS_BUSY;
          printf("%s start\n", vad_tag);
        }

        if (mindary_vad->status == VAD_STATUS_BUSY) {
          mindary_vad->flag = 1;
        }
        break;
      }
      case MINDARY_DUILITE_VAD_STOP: {
        if (mindary_vad->status == VAD_STATUS_BUSY) {
          mindary_vad->status = VAD_STATUS_WAIT;
          duilite_vad_stop(mindary_vad->vad);
          printf("%s stop\n", vad_tag);
        }

        if (mindary_vad->status == VAD_STATUS_WAIT) {
          mindary_vad->flag = 0;
        }
        break;
      }
      case MINDARY_DUILITE_VAD_FEED: {
        if (mindary_vad->status == VAD_STATUS_BUSY) {
          duilite_vad_feed(mindary_vad->vad, data->audio->audiodata,
                           data->audio->len);
        }
        break;
      }
      default:
        break;
      }
      vad_delete_operate((void *)data);
    }
    usleep(10000);
  }
  return NULL;
}

struct vad *vad_init(struct export *ex) {
  struct vad *mindary_vad = (struct vad *)malloc(sizeof(struct vad));
  mindary_vad->status = VAD_STATUS_IDLE;
  mindary_vad->lq = newQueue();
  mindary_vad->running = 1;
  mindary_vad->flag = 0;
  mindary_vad->vad = duilite_vad_new(vad_cfg, vad_callback, (void *)ex);
  assert(mindary_vad->vad != NULL);
  return mindary_vad;
}

int vad_run(struct vad *mvad) {
  pthread_create(&mvad->pid, NULL, _vad_run, (void *)mvad);
  return 0;
}

int vad_exit(struct vad *mvad) {
  mvad->running = 0;
  pthread_join(mvad->pid, NULL);
  return 0;
}

int vad_delete(struct vad *mvad) {
  if (mvad) {
    if (mvad->lq) {
      deleteQueue(mvad->lq, vad_delete_operate);
      mvad->lq = NULL;
    }

    if (mvad->vad) {
      if (mvad->status != VAD_STATUS_WAIT) {
        duilite_vad_stop(mvad->vad);
        mvad->status = VAD_STATUS_WAIT;
      }
      duilite_vad_delete(mvad->vad);
      mvad->vad = NULL;
    }
    free(mvad);
    mvad = NULL;
  }
  return 0;
}
