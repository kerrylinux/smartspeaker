#include "auth.h"
#include "duilite.h"
#include <stdio.h>
#include <string.h>

static char *auth_tag = "m-auth-v0.1.1";

int auth_init() {
  char auth_cfg[512];
  const char *productIdValue = "{\"productId\":\"";
  extern char productId[50];
  const char *productKeyValue = "\", \"productKey\":\"";
  extern char productKey[50];
  const char *productSecretValue = "\", \"productSecret\":\"";
  extern char productSecret[50];
  const char *savedProfileValue = "\", \"savedProfile\":\"";
  extern char savedProfile[50];
  const char *deviceNameValue = "\", \"devInfo\":{\"deviceName\":\"";
  extern char deviceName[50];
  const char *platformValue = "\", \"platform\":\"";
  extern char auth_platform[50];

  strcpy(auth_cfg, productIdValue);
  strcat(auth_cfg, productId);
  strcat(auth_cfg, productKeyValue);
  strcat(auth_cfg, productKey);
  strcat(auth_cfg, productSecretValue);
  strcat(auth_cfg, productSecret);
  strcat(auth_cfg, savedProfileValue);
  strcat(auth_cfg, savedProfile);
  strcat(auth_cfg, deviceNameValue);
  strcat(auth_cfg, deviceName);
  strcat(auth_cfg, platformValue);
  strcat(auth_cfg, auth_platform);
  strcat(auth_cfg, "\"}}");

  printf("%s auth_init, auth_cfg: %s\n", auth_tag, auth_cfg);

  int ret = duilite_library_load(auth_cfg);
  printf("%s init, ret: %d\n", auth_tag, ret);
  return ret;
}

int auth_delete() {
  printf("%s delete\n", auth_tag);
  duilite_library_release();
  return 0;
}