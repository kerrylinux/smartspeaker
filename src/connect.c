#include "connect.h"
#include "base64.h"
#include "cJSON.h"
#include "common.h"
#include "mqtt.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *tag = "connect-v0.1";

char *connert_Content;
/*
    将Json写入文件中
*/
int writeflietocon() {
  cJSON *root = cJSON_CreateObject();
  char *id = genRandomString(16);
  cJSON_AddItemToObject(root, "id", cJSON_CreateString(id));
  cJSON_AddItemToObject(root, "deviceID",
                        cJSON_CreateString("9999"));
  cJSON_AddItemToObject(root, "status", cJSON_CreateNumber(10));
  cJSON_AddItemToObject(root, "type", cJSON_CreateNumber(301));
  cJSON_AddItemToObject(root, "kind", cJSON_CreateNumber(0));
  cJSON *content = cJSON_CreateObject();
  cJSON *host = cJSON_CreateObject();
  cJSON_AddItemToObject(content, "host", host);
  cJSON_AddItemToObject(host, "arch", cJSON_CreateString("Cortex-A7"));
    char ip[1][INET_ADDRSTRLEN];
    memset(ip, 0, sizeof(ip));
    int n;
    char *ipbuf;
    n = get_local_ip(*ip);
    printf("ip account = %d\n", n);
  for(n; n>0; n--) {
      printf("ip = %s\n",ip[n-1]);
  }
  sprintf(ipbuf, "%s", ip[n-1]);

  if (n == 0) {
  cJSON_AddItemToObject(host, "ip", cJSON_CreateString("127.0.0.1"));
  } else {
   cJSON_AddItemToObject(host, "ip", cJSON_CreateString(&ip[0]));
  }
  cJSON_AddItemToObject(host, "mac",
  cJSON_CreateString("9c:b6:d0:00:00:02")); cJSON_AddItemToObject(host,
  "software", cJSON_CreateString("V1.0")); cJSON_AddItemToObject(host,
  "hardware", cJSON_CreateString("V1.0")); cJSON_AddItemToObject(host, "os",
  cJSON_CreateString("TinaV2.2"));
  // printf("%s\n\n", cJSON_Print(content));
  unsigned char *contentbase64 =
      base64_encode((unsigned char *)cJSON_Print(content));
  // printf("%s\n\n", contentbase64);
  cJSON_AddItemToObject(root, "content",
                        cJSON_CreateString((char *)contentbase64));

  FILE *fpcon = fopen("/tmp/connect.json", "w");
  fwrite(cJSON_Print(root), strlen(cJSON_Print(root)), 1, fpcon);
  fclose(fpcon); //关闭文件

  //free(id);
  cJSON_Delete(root); //释放资源

  return 0;
}

int connertContent() {
  cJSON *content = cJSON_CreateObject();
  cJSON *host = cJSON_CreateObject();
  cJSON_AddItemToObject(content, "host", host);
  cJSON_AddItemToObject(host, "arch", cJSON_CreateString("Cortex-A7"));
    char ip[1][INET_ADDRSTRLEN];
    memset(ip, 0, sizeof(ip));
    int n;
    char *ipbuf;
    n = get_local_ip(*ip);
    printf("ip account = %d\n", n);
  for(n; n>0; n--) {
      printf("ip = %s\n",ip[n-1]);
  }
  sprintf(ipbuf, "%s", ip[n-1]);

  if (n == 0) {
  cJSON_AddItemToObject(host, "ip", cJSON_CreateString("127.0.0.1"));
  } else {
   cJSON_AddItemToObject(host, "ip", cJSON_CreateString(&ip[0]));
  }
  cJSON_AddItemToObject(host, "mac", cJSON_CreateString("9c:b6:d0:00:00:02"));
  cJSON_AddItemToObject(host, "software", cJSON_CreateString("V1.0"));
  cJSON_AddItemToObject(host, "hardware", cJSON_CreateString("V1.0"));
  cJSON_AddItemToObject(host, "os", cJSON_CreateString("TinaV2.2"));
  // printf("%s\n\n", cJSON_Print(content));
  unsigned char *contentbase64 =
      base64_encode((unsigned char *)cJSON_Print(content));
  connert_Content = (char *)contentbase64;
  printf("connert_Content : %s\n\n", connert_Content);
  cJSON_Delete(content);
  return 0;
}

int connect_delete_operate(void *arg) { return 0; }

static void *_connect_run(void *arg) {
  printf("%s _connect_run begin!\n", tag);
  extern int connect_status;
  // extern char *AmplifierOn_value;
  // extern char *connectVoice_value;
  struct ipspeaker *ex = (struct ipspeaker *)arg;
  printf("%s ex->connect->running %d!\n", tag, ex->connect->running);
  while (ex->connect->running) {
    if (connect_status) {
      if (DISCONNECT_STATUS == ex->connect->status) {
        // system(AmplifierOn_value);
        // system(connectVoice_value);
        system("echo \"0\" > /sys/class/gpio/gpio68/value");
        system("aplay -D hw:sndcodec,0 /voice/connectsuccese.wav");
        ex->connect->status = CONNECT_STATUS;
      }
      //心跳时间1小时
      sleep(3600);
      mospub();
      struct mqtt_operate *mqtt_data =
          (struct mqtt_operate *)malloc(sizeof(struct mqtt_operate));
      assert(mqtt_data);
      mqtt_data->type = CONNECT_RUI_SERVICE;
      mqtt_data->info = (struct mqtt_part *)malloc(sizeof(struct mqtt_part));
      mqtt_data->info->content = connert_Content;
      mqtt_data->info->mqtt_type = 301;
      mqtt_data->info->status = 10;
      inQueue(ex->mqtt->lq, (void *)mqtt_data);
    } else {
      ex->connect->status = DISCONNECT_STATUS;
      //沒有連接到服務器30秒之後從新連接
      sleep(30);
      writeflietocon();
      mospub();
      struct mqtt_operate *mqtt_data =
          (struct mqtt_operate *)malloc(sizeof(struct mqtt_operate));
      assert(mqtt_data);
      mqtt_data->type = CONNECT_RUI_SERVICE;
      mqtt_data->info = (struct mqtt_part *)malloc(sizeof(struct mqtt_part));
      mqtt_data->info->content = connert_Content;
      mqtt_data->info->mqtt_type = 301;
      mqtt_data->info->status = 10;
      inQueue(ex->mqtt->lq, (void *)mqtt_data);
    }
  }
  return NULL;
}

struct connect *connect_init(struct ipspeaker *ex) {
  printf("%s init begin!\n", tag);
  connertContent();
  struct connect *rui_connect =
      (struct connect *)malloc(sizeof(struct connect));
  rui_connect->status = DISCONNECT_STATUS;
  rui_connect->lq = newQueue();
  rui_connect->running = 1;
  printf("%s init end!\n", tag);
  return rui_connect;
}

int connect_run(struct ipspeaker *ex) {
  printf("%s run begin!\n", tag);
  pthread_create(&ex->connect->pid, NULL, _connect_run, (void *)ex);
  printf("%s run end!\n", tag);
  return 0;
}

int connect_exit(struct connect *mconnect) {
  mconnect->running = 0;
  pthread_join(mconnect->pid, NULL);
  return 0;
}

int connect_delete(struct connect *mconnect) {
  if (mconnect) {
    if (mconnect->lq) {
      deleteQueue(mconnect->lq, connect_delete_operate);
      mconnect->lq = NULL;
    }

    if (mconnect->status != DISCONNECT_STATUS) {
      mconnect->status = DISCONNECT_STATUS;
    }

    free(mconnect);
    mconnect = NULL;
  }
  return 0;
}
