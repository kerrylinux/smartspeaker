#include "ddsasr.h"
#include "base64.h"
#include "cJSON.h"
#include "duilite.h"
#include "mqtt.h"
#include "play.h"
#include "record.h"
#include "vad.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *ddsasr_tag = "m-ddsasr-v0.1.1";
static int is_get_asr_result = 0;
static int is_get_tts_url = 0;
static int is_dui_response = 0;
static int devicename_status = 0;
int cinfo_count = 0;
extern char city[50];
extern char aliasKey[50];
extern char productId[50];
extern char productKey[50];
extern char productSecret[50];
extern char savedProfile[50];
extern char deviceName[50];

int ddsasr_delete_operate(void *arg) {
  struct ddsasr_operate *data = (struct ddsasr_operate *)arg;
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

static int dds_ev_cb(void *user_data, struct dds_msg *msg) {
  int type;
  struct export *ex = (struct export *)user_data;
  if (!dds_msg_get_type(msg, &type)) {
    // printf("type=%d\n", type);
    switch (type) {
    case DDS_EV_OUT_STATUS: {
      char *value;
      if (!dds_msg_get_string(msg, "status", &value)) {
        if (!strcmp(value, "idle")) {
          dds_status = DDS_STATUS_IDLE;
        } else if (!strcmp(value, "listening")) {
          dds_status = DDS_STATUS_LISTENING;
        } else if (!strcmp(value, "understanding")) {
          dds_status = DDS_STATUS_UNDERSTANDING;
        }
      }
      break;
    }
    case DDS_EV_OUT_DUI_LOGIN: {
      // printf("\n");
      char *value;
      if (!dds_msg_get_string(msg, "error", &value)) {
        printf("ERROR: %s\n", value);
      } else if (!dds_msg_get_string(msg, "deviceName", &value)) {
        printf("deviceName: %s\n", value);
        devicename_status = 1;
      }

      break;
    }
    case DDS_EV_OUT_DUI_DEVICENAME: {
      char *value;
      if (!dds_msg_get_string(msg, "deviceName", &value)) {
        printf("DDS_EV_OUT_DUI_DEVICENAME: %s\n", value);
      }
      break;
    }
    case DDS_EV_OUT_ASR_RESULT: {
      char *value;
      if (!dds_msg_get_string(msg, "var", &value)) {
        printf("var: %s\n", value);
      }
      if (!dds_msg_get_string(msg, "text", &value)) {
        printf("text: %s\n", value);
        is_get_asr_result = 1;
      }

      break;
    }
    case DDS_EV_OUT_CINFO_V2_RESULT: {
      char *value;
      if (!dds_msg_get_string(msg, "result", &value)) {
        printf("CINFO V2 result: %s\n", value);
      }

      break;
    }
    case DDS_EV_OUT_TTS: {
      char *value;
      size_t num;
      if (!dds_msg_get_string(msg, "speakUrl", &value)) {
        // printf("speakUrl: %s\n", value);
        is_get_tts_url = 1;
        // play tts music
        struct play_operate *play_data =
            (struct play_operate *)malloc(sizeof(struct play_operate));
        assert(play_data);
        play_data->type = PLAY_DDS_MUSIC;
        num = strlen(value);
        play_data->play_path = (char *)malloc(num);
        memcpy(play_data->play_path, value, num);
        inQueue(ex->play->lq, (void *)play_data);
        // close record
        struct record_operate *record_data =
            (struct record_operate *)malloc(sizeof(struct record_operate));
        record_data->type = RUI_RECORD_STOP;
        inQueue(ex->record->lq, (void *)record_data);
      }
      break;
    }
    case DDS_EV_OUT_DUI_RESPONSE: {
      char *resp = NULL;
      if (!dds_msg_get_string(msg, "response", &resp)) {
        // printf("dui response: %s\n", resp);
        cJSON *content = cJSON_CreateObject();
        cJSON_AddItemToObject(content, "type", cJSON_CreateNumber(2));
        cJSON *details = cJSON_CreateObject();
        cJSON_AddItemToObject(content, "details", details);
        cJSON_AddItemToObject(details, "question", cJSON_CreateString(""));
        cJSON_AddItemToObject(details, "askerName", cJSON_CreateString(""));
        cJSON_AddItemToObject(details, "answer", cJSON_CreateString(resp));
        unsigned char *contentbase64 =
            base64_encode((unsigned char *)cJSON_Print(content));
        // printf("contentbase64: %s\n", contentbase64);
        struct mqtt_operate *mqtt_data =
            (struct mqtt_operate *)malloc(sizeof(struct mqtt_operate));
        assert(mqtt_data);
        mqtt_data->type = SPEECH_RUI_ONCE;
        inQueue(ex->mqtt->lq, (void *)mqtt_data);
        cJSON_Delete(content);
        printf("cJSON_Delete\n");
      }
      is_dui_response = 1;
      break;
    }
    case DDS_EV_OUT_ERROR: {
      char *value;
      if (!dds_msg_get_string(msg, "error", &value)) {
        printf("DDS_EV_OUT_ERROR: %s\n", value);
      }
      is_dui_response = 1;
      break;
    }
    default:
      break;
    }
    return 0;
  }
}

static void *_ddsasr_run(void *arg) {
  struct ddsasr *mindary_asr = (struct ddsasr *)arg;
  int value = 0;

  while (mindary_asr->running) {
    struct ddsasr_operate *data =
        (struct ddsasr_operate *)outQueue(mindary_asr->lq);
    if (data) {
      // printf("mindary_asr->statusg _ddsasr_run : %d\n", mindary_asr->status);
      switch (data->type) {
      case MINDARY_DUILITE_ASR_START: {
        if (mindary_asr->status != DDS_STATUS_LISTENING) {
          struct dds_msg *msgstart = NULL;
          msgstart = dds_msg_new();
          dds_msg_set_type(msgstart, DDS_EV_IN_SPEECH);
          dds_msg_set_string(msgstart, "action", "start");
          dds_send(msgstart);
          dds_msg_delete(msgstart);
          msgstart = NULL;
          mindary_asr->status = DDS_STATUS_LISTENING;
          printf("%s start\n", ddsasr_tag);
        }

        if (mindary_asr->status == DDS_STATUS_LISTENING) {
          mindary_asr->flag |= MINDARY_DUILITE_ASR_FLAG_NEEDING;
        }
        break;
      }
      case MINDARY_DUILITE_ASR_STOP: {
        if (mindary_asr->status == DDS_STATUS_LISTENING) {
          struct dds_msg *msgend = NULL;
          msgend = dds_msg_new();
          dds_msg_set_type(msgend, DDS_EV_IN_SPEECH);
          dds_msg_set_string(msgend, "action", "end");
          dds_send(msgend);
          dds_msg_delete(msgend);
          msgend = NULL;
          mindary_asr->status = DDS_STATUS_IDLE;
        }
        while (1) {
          if (is_dui_response) {
            printf("%s stop\n", ddsasr_tag);
            is_dui_response = 0;
            break;
          }
          usleep(10000);
        }
        break;
      }
      case MINDARY_DUILITE_ASR_FEED: {
        if (mindary_asr->status == DDS_STATUS_LISTENING) {
          struct dds_msg *m;
          m = dds_msg_new();
          dds_msg_set_type(m, DDS_EV_IN_AUDIO_STREAM);
          dds_msg_set_bin(m, "audio", data->audio->audiodata, data->audio->len);
          dds_send(m);
          dds_msg_delete(m);
        }
        break;
      }
      case MINDARY_DUILITE_ASR_CANCEL: {
        struct dds_msg *msgcancel;
        msgcancel = dds_msg_new();
        dds_msg_set_type(msgcancel, DDS_EV_IN_RESET);
        dds_send(msgcancel);
        dds_msg_delete(msgcancel);
        msgcancel = NULL;
        mindary_asr->status = DDS_STATUS_IDLE;
        printf("%s cancel\n", ddsasr_tag);
        break;
      }
      default:
        break;
      }
      ddsasr_delete_operate((void *)data);
    }
    if (devicename_status && cinfo_count == 0) {
      printf("set DDS_EV_IN_SYSTEM_SETTINGS \n");
      struct dds_msg *msgsystem = NULL;
      printf("settings city\n");
      msgsystem = dds_msg_new();
      dds_msg_set_type(msgsystem, DDS_EV_IN_SYSTEM_SETTINGS);
      char tmp[256];
      sprintf(tmp,
              "[{\"key\":\"location\",\"value\":{\"longitude\":\"\","
              "\"latitude\":\"\","
              "\"address\":\"\",\"city\":\"%s\",\"time\":\"\"}}]",
              city);
      dds_msg_set_string(msgsystem, "settings", tmp);
      dds_send(msgsystem);
      dds_msg_delete(msgsystem);
      msgsystem = NULL;
      cinfo_count = 1;
    }
    usleep(10000);
  }
  return NULL;
}

void *_run(void *arg) {
  struct dds_msg *msg = dds_msg_new();
  msg = dds_msg_new();
  dds_msg_set_string(msg, "productId", productId);
  dds_msg_set_string(msg, "aliasKey", "test");
  dds_msg_set_string(msg, "productKey", productKey);
  dds_msg_set_string(msg, "productSecret", productSecret);

  int flag = -1;
  flag = access(savedProfile, 0);
  if (flag != 0) {
    dds_msg_set_string(msg, "savedProfile", savedProfile);
  }
  char tmp[256];
  sprintf(tmp,
          "{\"deviceName\":\"%s\",\"platform\":\"Linux\","
          "\"instructionSet\":\"armv7\",\"chipModel\":\"xxxx\"}",
          deviceName);
  dds_msg_set_string(msg, "devInfo", tmp);
  struct dds_opt opt;
  opt._handler = dds_ev_cb;
  opt.userdata = arg;
  dds_start(msg, &opt);
  dds_msg_delete(msg);

  return NULL;
}

struct ddsasr *ddsasr_init(struct export *ex) {
  struct ddsasr *mindary_asr = (struct ddsasr *)malloc(sizeof(struct ddsasr));
  mindary_asr->status = DDS_STATUS_NONE;
  mindary_asr->lq = newQueue();
  mindary_asr->running = 1;
  mindary_asr->flag = 0;

  pthread_t tiddds;
  pthread_create(&tiddds, NULL, _run, (void *)ex);
  return mindary_asr;
}

int ddsasr_run(struct ddsasr *masr) {
  pthread_create(&masr->pid, NULL, _ddsasr_run, (void *)masr);
  return 0;
}

int ddsasr_exit(struct ddsasr *masr) {
  masr->running = 0;
  pthread_join(masr->pid, NULL);
  return 0;
}

int ddsasr_delete(struct ddsasr *masr) {
  if (masr) {
    if (masr->lq) {
      deleteQueue(masr->lq, ddsasr_delete_operate);
      masr->lq = NULL;
    }
    masr->status = DDS_STATUS_NONE;
    free(masr);
    masr = NULL;
  }

  struct dds_msg *msg = dds_msg_new();
  dds_msg_set_type(msg, DDS_EV_IN_EXIT);
  dds_send(msg);
  dds_msg_delete(msg);
  return 0;
}
