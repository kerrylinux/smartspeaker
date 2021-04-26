#include "export.h"
#include "auth.h"
#include "ddsasr.h"
#include "export.h"
#include "minIni.h"
#include "mqtt.h"
#include "play.h"
#include "record.h"
#include "vad.h"
#include "wakeup.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define sizearray(a) (sizeof(a) / sizeof((a)[0]))

static char *export_tag = "m-export-v0.1.1";
const char inifile[] = "config.ini";

char productId[50];
char productKey[50];
char productSecret[50];
char savedProfile[50];
char deviceName[50];
char auth_platform[50];
char city[50];
char aliasKey[50];
char dds_platform[50];
char instructionSet[50];
char chipModel[50];
char resBinPath[100];
int pauseTime;
int fullMode;
char env[100];
char wakeupBinPath[100];
char aecBinPath[100];
char beamformingBinPath[100];
char wakeupVoice[50];
char deviceID[50];
char AmplifierGPIO[50];
char AmplifierExport[50];
char AmplifierDirection[50];
char AmplifierOn[50];
char AmplifierOff[50];
char connectVoice[50];
char arch[50];
char mac[50];
char softwareVersion[50];
char hardwareVersion[50];
char osVersion[50];
char host[50];
long port;
int keepalive;
char sub[50];
char pub[50];
char username[50];
char password[50];
int rate;
int size;
int channels;
int rsize;
char recordPath[50];
char recordname[50];
// int gpio;
// char voice1[50];
// char voice2[50];

int ini_init() {
  ini_gets("auth", "productId", "", productId, sizearray(productId), inifile);
  ini_gets("auth", "productKey", "", productKey, sizearray(productKey),
           inifile);
  ini_gets("auth", "productSecret", "", productSecret, sizearray(productSecret),
           inifile);
  ini_gets("auth", "savedProfile", "", savedProfile, sizearray(savedProfile),
           inifile);
  ini_gets("auth", "deviceName", "", deviceName, sizearray(deviceName),
           inifile);
  ini_gets("auth", "platform", "", auth_platform, sizearray(auth_platform),
           inifile);

  ini_gets("dds", "city", "", city, sizearray(city), inifile);
  ini_gets("dds", "aliasKey", "", aliasKey, sizearray(aliasKey), inifile);
  ini_gets("dds", "platform", "", dds_platform, sizearray(dds_platform),
           inifile);
  ini_gets("dds", "instructionSet", "", instructionSet,
           sizearray(instructionSet), inifile);
  ini_gets("dds", "chipModel", "", chipModel, sizearray(chipModel), inifile);
  ini_gets("vad", "resBinPath", "", resBinPath, sizearray(resBinPath), inifile);
  pauseTime = ini_getl("vad", "pauseTime", 1200, inifile);
  fullMode = ini_getl("vad", "fullMode", 0, inifile);

  ini_gets("wakeup", "env", "", env, sizearray(env), inifile);
  ini_gets("wakeup", "wakeupBinPath", "", wakeupBinPath,
           sizearray(wakeupBinPath), inifile);
  ini_gets("wakeup", "aecBinPath", "", aecBinPath, sizearray(aecBinPath),
           inifile);
  ini_gets("wakeup", "beamformingBinPath", "", beamformingBinPath,
           sizearray(beamformingBinPath), inifile);
  ini_gets("wakeup", "wakeupVoice", "", wakeupVoice, sizearray(wakeupVoice),
           inifile);

  ini_gets("system", "deviceID", "", deviceID, sizearray(deviceID), inifile);
  ini_gets("system", "AmplifierGPIO", "", AmplifierGPIO,
           sizearray(AmplifierGPIO), inifile);
  ini_gets("system", "AmplifierExport", "", AmplifierExport,
           sizearray(AmplifierExport), inifile);
  ini_gets("system", "AmplifierDirection", "", AmplifierDirection,
           sizearray(AmplifierDirection), inifile);
  ini_gets("system", "AmplifierOn", "", AmplifierOn, sizearray(AmplifierOn),
           inifile);
  ini_gets("system", "AmplifierOff", "", AmplifierOff, sizearray(AmplifierOff),
           inifile);
  ini_gets("system", "connectVoice", "", connectVoice, sizearray(connectVoice),
           inifile);
  ini_gets("system", "arch", "", arch, sizearray(arch), inifile);
  ini_gets("system", "mac", "", mac, sizearray(mac), inifile);
  ini_gets("system", "softwareVersion", "", softwareVersion,
           sizearray(softwareVersion), inifile);
  ini_gets("system", "hardwareVersion", "", hardwareVersion,
           sizearray(hardwareVersion), inifile);
  ini_gets("system", "osVersion", "", osVersion, sizearray(osVersion), inifile);

  ini_gets("mqtt", "host", "", host, sizearray(host), inifile);
  port = ini_getl("mqtt", "port", 1883, inifile);
  keepalive = ini_getl("mqtt", "keepalive", 60, inifile);
  ini_gets("mqtt", "sub", "", sub, sizearray(sub), inifile);
  ini_gets("mqtt", "pub", "", pub, sizearray(pub), inifile);
  ini_gets("mqtt", "username", "", username, sizearray(username), inifile);
  ini_gets("mqtt", "password", "", password, sizearray(password), inifile);

  rate = ini_getl("record", "rate", 16000, inifile);
  size = ini_getl("record", "size", 16, inifile);
  keepalive = ini_getl("record", "keepalive", 60, inifile);
  ini_gets("record", "recordPath", "", recordPath, sizearray(recordPath),
           inifile);
  ini_gets("record", "recordname", "", recordname, sizearray(recordname),
           inifile);

  // gpio = ini_getl("renti", "gpio", 60, inifile);
  // ini_gets("renti", "voice1", "", voice1, sizearray(voice1), inifile);
  // ini_gets("renti", "voice2", "", voice2, sizearray(voice2), inifile);

  if (access(AmplifierGPIO, F_OK) != 0) {
    system(AmplifierExport);
  }
  system(AmplifierDirection);
  system(AmplifierOn);
  // if (access("/sys/class/gpio/gpio68", F_OK) != 0) {
  //   system("echo \"68\" > /sys/class/gpio/export");
  // }
  // system("echo \"out\" > /sys/class/gpio/gpio68/direction");
  // system("echo \"0\" > /sys/class/gpio/gpio68/value");

  return 0;
}

struct export *export_init(char *cfg, export_ev_callback ex_callback,
                           void *userdata) {
  printf("%s version: %s\n", export_tag, EXPORT_VERSION);
  struct export *ex = (struct export *)malloc(sizeof(struct export));
  assert(ex);
  int ret = auth_init();
  ex->mqtt = mqtt_init(ex);
  ex->record = record_init(ex);
  if (!ret) {
    ex->ddsasr = ddsasr_init(ex);
    ex->vad = vad_init(ex);
    ex->wakeup = wakeup_init(ex);
    ex->play = play_init(ex);
  } else {
    printf("%s auth failed, ret: %d\n", export_tag, ret);
  }
  ex->renti = renti_init(ex);
  ex->connect = connect_init(ex);
  ex->auth_result = ret;
  return ex;
}

int export_run(struct export *ex) {
  if (ex) {
    mqtt_run(ex->mqtt);
    ddsasr_run(ex->ddsasr);
    vad_run(ex->vad);
    wakeup_run(ex->wakeup);
    play_run(ex);
    record_run(ex->record);
    renti_run(ex->renti);
    connect_run(ex);
  } else {
    printf("%s init failed, due to null ex\n", export_tag);
  }
  return 0;
}

int export_enable_wakeup(struct export *ex) {
  struct wakeup_operate *start_data =
      (struct wakeup_operate *)malloc(sizeof(struct wakeup_operate));
  start_data->type = MINDARY_DUILITE_WAKEUP_START;
  start_data->audio = NULL;
  inQueue(ex->wakeup->lq, (void *)start_data);
  return 0;
}

int export_disable_wakeup(struct export *ex) {
  struct wakeup_operate *end_data =
      (struct wakeup_operate *)malloc(sizeof(struct wakeup_operate));
  end_data->type = MINDARY_DUILITE_WAKEUP_STOP;
  end_data->audio = NULL;
  inQueue(ex->wakeup->lq, (void *)end_data);
  return 0;
}

int export_enable_record(struct export *ex) {
  struct record_operate *record_data =
      (struct record_operate *)malloc(sizeof(struct record_operate));
  record_data->type = RUI_RECORD_START;
  inQueue(ex->record->lq, (void *)record_data);
  return 0;
}

int export_feed_data(struct export *ex, char *data, int len) {
  struct wakeup_operate *wakeup_data =
      (struct wakeup_operate *)malloc(sizeof(struct wakeup_operate));
  wakeup_data->type = MINDARY_DUILITE_WAKEUP_FEED;
  wakeup_data->audio = (struct wakeup_part *)malloc(sizeof(struct wakeup_part));
  wakeup_data->audio->len = len;
  wakeup_data->audio->audiodata = (char *)malloc(len);
  memcpy(wakeup_data->audio->audiodata, data, len);
  inQueue(ex->wakeup->lq, (void *)wakeup_data);
}

int export_exit(struct export *ex) {
  if (ex) {
    record_exit(ex->record);
    vad_exit(ex->vad);
    ddsasr_exit(ex->ddsasr);
    wakeup_exit(ex->wakeup);
    play_exit(ex->play);
    connect_exit(ex->connect);
    mqtt_exit(ex->mqtt);
    renti_exit(ex->renti);
  }
  return 0;
}

int export_delete(struct export *ex) {
  if (ex) {
    if (ex->vad) {
      vad_delete(ex->vad);
      ex->vad = NULL;
    }
    if (ex->ddsasr) {
      ddsasr_delete(ex->ddsasr);
      ex->ddsasr = NULL;
    }
    if (ex->wakeup) {
      wakeup_delete(ex->wakeup);
      ex->wakeup = NULL;
    }
    if (ex->play) {
      play_delete(ex->play);
      ex->play = NULL;
    }
    if (ex->connect) {
      connect_delete(ex->connect);
      ex->connect = NULL;
    }
    auth_delete();
    free(ex);
    ex = NULL;
  }
  return 0;
}