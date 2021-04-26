#include "mqtt.h"
#include "cJSON.h"
#include "common.h"
#include <mosquitto.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *tag = "mqtt-v0.1";
bool session = true;
int connect = -1;
int connect_status = 0;
int is_music_begin = 0;
int music_begin_speaker = 1;
int restart_snapclient = 0;
char *connect_id = NULL;
extern const char *host_value;
extern const int port_value;
extern const int keepalive_value;
extern const char *sub_value;
extern const char *pub_value;

int mqtt_delete_operate(void *arg) {
  struct mqtt_operate *data = (struct mqtt_operate *)arg;
  if (data) {
    free(data);
    data = NULL;
  }
  return 0;
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result) {
  if (!result) {
    mosquitto_subscribe(mosq, NULL, "12345", 2);

  } else {
    fprintf(stderr, "Connect failed\n");
  }
  return;
}

void my_message_callback(struct mosquitto *mosq, void *userdata,
                         const struct mosquitto_message *message) {
  int is_cancel_mute = -1;
  int music_begin = -1;
  int music_end = -1;
  int paly_music = -1;

  if (message->payloadlen) {
    printf("\n %s %s \n", message->topic, message->payload);
    cJSON *message_payload, *mqtt_content, *mqtt_id, *mqtt_type, *mqtt_status;
    message_payload = cJSON_Parse(message->payload);

    if (!message_payload) {
      printf("JSON格式错误:%s\n\n", cJSON_GetErrorPtr());
    }

    if (message_payload) {
      printf("JSON格式正确:\n%s\n\n", cJSON_Print(message_payload));

      mqtt_id = cJSON_GetObjectItem(message_payload, "id");

      if (mqtt_id->type == cJSON_String) {
        printf("mqtt_id:%s\r\n", mqtt_id->valuestring);
        connect = strcmp(connect_id, mqtt_id->valuestring);
        if (0 == connect) {
          mqtt_status = cJSON_GetObjectItem(message_payload, "status");
          if (mqtt_status->type == cJSON_Number) {
            printf("mqtt_status:%d\r\n", mqtt_status->valueint);
            if (30 == mqtt_status->valueint) {
              connect_status = 1;
              printf("message_payload is_connect:%d\r\n", connect_status);
            }
          }
        }
      }
    }

    cJSON_Delete(message_payload); //释放内存
  }
  return;
}

static void *_mqtt_run(void *arg) {
  printf("%s _mqtt_run begin!\n", tag);
  int connect_flag = 1;
  struct mosquitto *mosq = NULL;
  struct mqtt *rui_mqtt = (struct mqtt *)arg;
  char *id;
  // extern char *deviceID_value;
  char *deviceID_value = "12345";
  while (rui_mqtt->running) {
    struct mqtt_operate *data = (struct mqtt_operate *)outQueue(rui_mqtt->lq);
    if (data) {
      if (connect_flag) {
        // libmosquitto 库初始化
        mosquitto_lib_init();
        //创建mosquitto客户端
        mosq = mosquitto_new(NULL, session, NULL);
        if (!mosq) {
          printf("Create mosquitto client failed..\n");
          mosquitto_destroy(mosq);
          mosquitto_lib_cleanup();
          connect_flag = 1;
        }
        printf("Create mosquitto client sucessfully!\n");
        //设置回调函数，需要时可使用
        mosquitto_log_callback_set(mosq, my_log_callback);
        mosquitto_connect_callback_set(mosq, my_connect_callback);
        mosquitto_message_callback_set(mosq, my_message_callback);
        mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
        //客户端连接服务器
        if (mosquitto_connect(mosq, "116.62.162.253", 1883, 60) !=
            MOSQ_ERR_SUCCESS) {
          printf("Mosquitto connect failed..\n");
          mosquitto_destroy(mosq);
          mosquitto_lib_cleanup();
          connect_flag = 1;
        }
        printf("Mosquitto connect %s:%d sucessfully!\n", "116.62.162.253",
               1883);

        //循环处理网络消息
        int loop = mosquitto_loop_start(mosq);
        if (loop != MOSQ_ERR_SUCCESS) {
          printf("mosquitto loop error\n");
          mosquitto_destroy(mosq);
          mosquitto_lib_cleanup();
          connect_flag = 1;
        }
        connect_flag = 0;
      }
      switch (data->type) {
      case CONNECT_RUI_SERVICE: {
        cJSON *root = cJSON_CreateObject();
        connect_id = genRandomString(16);
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(connect_id));
        cJSON_AddItemToObject(root, "deviceID",
                              cJSON_CreateString(deviceID_value));
        cJSON_AddItemToObject(root, "status",
                              cJSON_CreateNumber(data->info->status));
        cJSON_AddItemToObject(root, "type",
                              cJSON_CreateNumber(data->info->mqtt_type));
        cJSON_AddItemToObject(root, "kind", cJSON_CreateNumber(0));
        cJSON_AddItemToObject(root, "content",
                              cJSON_CreateString((char *)data->info->content));

        // if (mosquitto_publish(mosq, NULL, pub_value,
        //                       strlen(cJSON_Print(root)) + 1,
        //                       cJSON_Print(root), 0, 0) != MOSQ_ERR_SUCCESS) {
        if (mosquitto_publish(mosq, NULL, "gateway",
                              strlen(cJSON_Print(root)) + 1, cJSON_Print(root),
                              0, 0) != MOSQ_ERR_SUCCESS) {
          printf("MQTT publish connect_RUI_SERVICE error");
          mosquitto_destroy(mosq);
          mosquitto_lib_cleanup();
          connect_flag = 1;
        } else {
          printf("MQTT publish connect_RUI_SERVICE Ok!\n");
        }
        cJSON_Delete(root);
        break;
      }
      case WAKEUP_RUI_ONCE: {
        cJSON *root = cJSON_CreateObject();
        id = genRandomString(16);
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(id));
        cJSON_AddItemToObject(root, "deviceID",
                              cJSON_CreateString(deviceID_value));
        cJSON_AddItemToObject(root, "status",
                              cJSON_CreateNumber(data->info->status));
        cJSON_AddItemToObject(root, "type",
                              cJSON_CreateNumber(data->info->mqtt_type));
        cJSON_AddItemToObject(root, "kind", cJSON_CreateNumber(0));
        cJSON_AddItemToObject(root, "content",
                              cJSON_CreateString((char *)data->info->content));
        // if (mosquitto_publish(mosq, NULL, pub_value,
        //                       strlen(cJSON_Print(root)) + 1,
        //                       cJSON_Print(root), 0, 0) != MOSQ_ERR_SUCCESS) {
        if (mosquitto_publish(mosq, NULL, "gateway",
                              strlen(cJSON_Print(root)) + 1, cJSON_Print(root),
                              0, 0) != MOSQ_ERR_SUCCESS) {
          printf("MQTT publish WAKEUP_RUI_ONCE error");
          mosquitto_destroy(mosq);
          mosquitto_lib_cleanup();
          connect_flag = 1;
        } else {
          printf("MQTT publish WAKEUP_RUI_ONCE Ok!\n");
        }
        cJSON_Delete(root);
        break;
      }
      case SPEECH_RUI_ONCE: {
        cJSON *root = cJSON_CreateObject();
        id = genRandomString(16);
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(id));
        cJSON_AddItemToObject(root, "deviceID",
                              cJSON_CreateString(deviceID_value));
        cJSON_AddItemToObject(root, "status",
                              cJSON_CreateNumber(data->info->status));
        cJSON_AddItemToObject(root, "type",
                              cJSON_CreateNumber(data->info->mqtt_type));
        cJSON_AddItemToObject(root, "kind", cJSON_CreateNumber(0));
        cJSON_AddItemToObject(root, "content",
                              cJSON_CreateString((char *)data->info->content));

        // if (mosquitto_publish(mosq, NULL, pub_value,
        //                       strlen(cJSON_Print(root)) + 1,
        //                       cJSON_Print(root), 0, 0) != MOSQ_ERR_SUCCESS) {
        if (mosquitto_publish(mosq, NULL, "gateway",
                              strlen(cJSON_Print(root)) + 1, cJSON_Print(root),
                              0, 0) != MOSQ_ERR_SUCCESS) {
          printf("MQTT publish SPEECH_RUI_ONCE error");
          mosquitto_destroy(mosq);
          mosquitto_lib_cleanup();
          connect_flag = 1;
        } else {
          printf("MQTT publish SPEECH_RUI_ONCE Ok!\n");
        }
        cJSON_Delete(root);
        break;
      }
      case MUSIC_MUTE: {
        break;
      }
      case MUSIC_UNMUTE: {
        break;
      }
      default:
        break;
      }
      // vad_delete_operate((void *)data);
    }
    usleep(10000);
  }
  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();
  return NULL;
}

struct mqtt *mqtt_init(struct ipspeaker *ex) {
  printf("%s init begin!\n", tag);
  struct mqtt *rui_mqtt = (struct mqtt *)malloc(sizeof(struct mqtt));
  rui_mqtt->status = MQTT_IDLE;
  rui_mqtt->lq = newQueue();
  rui_mqtt->running = 1;
  printf("%s init end!\n", tag);
  return rui_mqtt;
}

int mqtt_run(struct mqtt *mosmqtt) {
  printf("%s run begin!\n", tag);
  pthread_create(&mosmqtt->pid, NULL, _mqtt_run, (void *)mosmqtt);
  printf("%s run end!\n", tag);
  return 0;
}

int mqtt_exit(struct mqtt *mmqtt) {
  mmqtt->running = 0;
  pthread_join(mmqtt->pid, NULL);
  return 0;
}

int mqtt_delete(struct mqtt *mmqtt) {
  if (mmqtt) {
    if (mmqtt->lq) {
      deleteQueue(mmqtt->lq, mqtt_delete_operate);
      mmqtt->lq = NULL;
    }
    free(mmqtt);
    mmqtt = NULL;
  }
  return 0;
}