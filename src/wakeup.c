#include "wakeup.h"
#include "cJSON.h"
#include "ddsasr.h"
#include "duilite.h"
#include "mqtt.h"
#include "play.h"
#include "record.h"
#include "vad.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *wakeup_tag = "m-wakeup-v0.1.1";
static char *wakeup_gram = NULL;
static int total_len = 0;

// delete data block
int wakeup_delete_operate(void *arg) {
  struct wakeup_operate *data = (struct wakeup_operate *)arg;
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

static int wakeup_callback(void *user_data, int type, char *msg, int len) {
  struct export *ex = (struct export *)user_data;
  printf("%s result, %.*s\n", wakeup_tag, len, msg);
  total_len = 0;
  printf("wakeup total_len : %d\n", total_len);
  // cancle ASR
  struct ddsasr_operate *data =
      (struct ddsasr_operate *)malloc(sizeof(struct ddsasr_operate));
  data->type = MINDARY_DUILITE_ASR_CANCEL;
  data->audio = NULL;
  inQueue(ex->ddsasr->lq, (void *)data);
  // close VAD
  struct vad_operate *end_data =
      (struct vad_operate *)malloc(sizeof(struct vad_operate));
  end_data->type = MINDARY_DUILITE_VAD_STOP;
  end_data->audio = NULL;
  inQueue(ex->vad->lq, (void *)end_data);
  // close record
  struct record_operate *record_data =
      (struct record_operate *)malloc(sizeof(struct record_operate));
  record_data->type = RUI_RECORD_STOP;
  inQueue(ex->record->lq, (void *)record_data);
  // play wakeup music
  struct play_operate *play_data =
      (struct play_operate *)malloc(sizeof(struct play_operate));
  assert(play_data);
  play_data->type = PLAY_WAKEUP;
  play_data->play_path = NULL;
  inQueue(ex->play->lq, (void *)play_data);
  // send wakeup mqtt
  printf("send wakeup mqtt !!!\n");
  struct mqtt_operate *mqtt_data =
      (struct mqtt_operate *)malloc(sizeof(struct mqtt_operate));
  assert(mqtt_data);
  mqtt_data->type = WAKEUP_RUI_ONCE;
  inQueue(ex->mqtt->lq, (void *)mqtt_data);
  return 0;
}

static int doa_callback(void *user_data, int type, char *msg, int len) {
  struct export *ex = (struct export *)user_data;
  printf("%.*s\n", len, msg);
}

static int beamforming_callback(void *user_data, int type, char *msg, int len) {
  struct export *ex = (struct export *)user_data;
  if (type == DUILITE_MSG_TYPE_JSON) {
    printf("%.*s\n", len, msg);
  } else {
    // FILE *audiobf = fopen("/root/bf.pcm", "a+");
    // if (audiobf) {
    //   fwrite(msg, 1, len, audiobf);
    //   fclose(audiobf);
    // }
    // paly wakeup music after

    if (ex->vad->flag == 1) {
      // 15 second quit
      total_len += len;
      // printf("total_len : %d\n", total_len);
      if (total_len > (32000 * 15)) {
        printf("total_len : %d\n", total_len);
        total_len = 0;
        // cancle ASR
        struct ddsasr_operate *data =
            (struct ddsasr_operate *)malloc(sizeof(struct ddsasr_operate));
        data->type = MINDARY_DUILITE_ASR_CANCEL;
        data->audio = NULL;
        inQueue(ex->ddsasr->lq, (void *)data);

        // close VAD
        struct vad_operate *end_data =
            (struct vad_operate *)malloc(sizeof(struct vad_operate));
        end_data->type = MINDARY_DUILITE_VAD_STOP;
        end_data->audio = NULL;
        inQueue(ex->vad->lq, (void *)end_data);
      }
      // send data to VAD
      struct vad_operate *data =
          (struct vad_operate *)malloc(sizeof(struct vad_operate));
      data->type = MINDARY_DUILITE_VAD_FEED;
      data->audio = (struct vad_part *)malloc(sizeof(struct vad_part));
      data->audio->len = len;
      data->audio->audiodata = (char *)malloc(len);
      memcpy(data->audio->audiodata, msg, len);
      inQueue(ex->vad->lq, (void *)data);
    }
  }
}

static void *_wakeup_run(void *arg) {
  struct wakeup *mindary_wakeup = (struct wakeup *)arg;
  while (mindary_wakeup->running) {
    struct wakeup_operate *data =
        (struct wakeup_operate *)outQueue(mindary_wakeup->lq);
    if (data) {
      switch (data->type) {
      case MINDARY_DUILITE_WAKEUP_START: {
        if (mindary_wakeup->status != WAKEUP_STATUS_BUSY) {
          printf("%s start\n", wakeup_tag);
          duilite_fespl_start(mindary_wakeup->wakeup, NULL);
          mindary_wakeup->status = WAKEUP_STATUS_BUSY;
        }
        break;
      }
      case MINDARY_DUILITE_WAKEUP_STOP: {
        if (mindary_wakeup->status != WAKEUP_STATUS_WAIT) {
          printf("%s stop\n", wakeup_tag);
          duilite_fespl_stop(mindary_wakeup->wakeup);
          mindary_wakeup->status = WAKEUP_STATUS_WAIT;
        }
        break;
      }
      case MINDARY_DUILITE_WAKEUP_FEED: {
        if (mindary_wakeup->status == WAKEUP_STATUS_BUSY) {
          duilite_fespl_feed(mindary_wakeup->wakeup, data->audio->audiodata,
                             data->audio->len);
        }
        break;
      }
      default:
        break;
      }
      wakeup_delete_operate((void *)data);
    }
    usleep(10000);
  }
  return NULL;
}

struct wakeup *wakeup_init(struct export *ex) {
  struct wakeup *mindary_wakeup =
      (struct wakeup *)malloc(sizeof(struct wakeup));
  mindary_wakeup->status = WAKEUP_STATUS_IDLE;
  mindary_wakeup->lq = newQueue();
  mindary_wakeup->running = 1;
  char fespl_cfg[1024];
  char *aec = "{\"aecBinPath\":\"";
  char *env = ",\"env\":\"";
  char *wakeupbin = ",\"wakeupBinPath\":\"";
  char *bfbin = ",\"beamformingBinPath\":\"";
  char *wakeup_last = "wakeup.bin";
  char *env_last = "words=ni hao;thresh=0.2;major=0;";
  char *aec_last = "AEC.bin";
  char *bf_last = "ULA.bin";

  strcpy(fespl_cfg, aec);
  strcat(fespl_cfg, aec_last);
  strcat(fespl_cfg, "\"");
  strcat(fespl_cfg, wakeupbin);
  strcat(fespl_cfg, wakeup_last);
  strcat(fespl_cfg, "\"");
  strcat(fespl_cfg, bfbin);
  strcat(fespl_cfg, bf_last);
  strcat(fespl_cfg, "\"");
  strcat(fespl_cfg, env);
  strcat(fespl_cfg, env_last);
  strcat(fespl_cfg, "\"}");
  printf("fespl_cfg = %s\n", fespl_cfg);
  mindary_wakeup->wakeup = duilite_fespl_new(fespl_cfg);
  duilite_fespl_register(mindary_wakeup->wakeup, DUILITE_CALLBACK_FESPL_WAKEUP,
                         wakeup_callback, (void *)ex);
  duilite_fespl_register(mindary_wakeup->wakeup, DUILITE_CALLBACK_FESPL_DOA,
                         doa_callback, (void *)ex);
  duilite_fespl_register(mindary_wakeup->wakeup,
                         DUILITE_CALLBACK_FESPL_BEAMFORMING,
                         beamforming_callback, (void *)ex);
  // duilite_fespl_start(mindary_wakeup->wakeup, NULL);
  assert(mindary_wakeup->wakeup != NULL);
  return mindary_wakeup;
}

int wakeup_run(struct wakeup *mwakeup) {
  pthread_create(&mwakeup->pid, NULL, _wakeup_run, (void *)mwakeup);
  return 0;
}

int wakeup_exit(struct wakeup *mwakeup) {
  mwakeup->running = 0;
  pthread_join(mwakeup->pid, NULL);
  return 0;
}

int wakeup_delete(struct wakeup *mwakeup) {
  if (mwakeup) {
    if (mwakeup->lq) {
      deleteQueue(mwakeup->lq, wakeup_delete_operate);
      mwakeup->lq = NULL;
    }

    if (mwakeup->wakeup) {
      if (mwakeup->status != WAKEUP_STATUS_WAIT) {
        duilite_fespl_stop(mwakeup->wakeup);
        mwakeup->status = WAKEUP_STATUS_WAIT;
      }
      duilite_fespl_delete(mwakeup->wakeup);
      mwakeup->wakeup = NULL;
    }
    free(mwakeup);
    mwakeup = NULL;
  }
  return 0;
}
