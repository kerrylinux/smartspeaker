#include "renti.h"
#include "gpioin.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static char *tag = "renti-v0.1";

struct renti *renti_init(struct export *ex) {
  printf("%s init begin!\n", tag);
  struct renti *rui_renti = (struct renti *)malloc(sizeof(struct renti));
  rui_renti->running = 1;
  printf("%s init end!\n", tag);
  return rui_renti;
}

int renti_run(struct renti *mrenti) {
  printf("%s run begin!\n", tag);
  pthread_create(&mrenti->pid, NULL, _renti_run, (void *)mrenti);
  printf("%s run end!\n", tag);
  return 0;
}

int renti_exit(struct renti *mrenti) {
  mrenti->running = 0;
  pthread_join(mrenti->pid, NULL);
  return 0;
}

#define sizearray(a) (sizeof(a) / sizeof((a)[0]))

static char *tag = "renti-v1.1.10";
const char inifile[] = "config.ini";
// const char inifile[] = "./test.ini";

long period;

char host[50];
long port;
char pub[50];
char username[50];
char password[50];
char deviceID[50];
bool session = false;

char onestart[10], oneend[10];
char twostart[10], twoend[10];
char threestart[10], threeend[10];
char fourstart[10], fourend[10];
char fivestart[10], fiveend[10];
int onestart_Hour, onestart_Min, oneend_Hour, oneend_Min, onestart_Time,
    oneend_Time;
int twostart_Hour, twostart_Min, twoend_Hour, twoend_Min, twostart_Time,
    twoend_Time;
int threestart_Hour, threestart_Min, threeend_Hour, threeend_Min,
    threestart_Time, threeend_Time;
int fourstart_Hour, fourstart_Min, fourend_Hour, fourend_Min, fourstart_Time,
    fourend_Time;
int fivestart_Hour, fivestart_Min, fiveend_Hour, fiveend_Min, fivestart_Time,
    fiveend_Time;

int ini_init() {
  period = ini_getl("renti", "period", 30, inifile);
  ini_gets("mqtt", "host", "", host, sizearray(host), inifile);
  port = ini_getl("mqtt", "port", 1883, inifile);
  ini_gets("mqtt", "pub", "", pub, sizearray(pub), inifile);
  ini_gets("mqtt", "username", "", username, sizearray(username), inifile);
  ini_gets("mqtt", "password", "", password, sizearray(password), inifile);
  ini_gets("system", "deviceID", "", deviceID, sizearray(deviceID), inifile);

  ini_gets("playtime", "onestart", "", onestart, sizearray(onestart), inifile);
  ini_gets("playtime", "oneend", "", oneend, sizearray(oneend), inifile);
  ini_gets("playtime", "twostart", "", twostart, sizearray(twostart), inifile);
  ini_gets("playtime", "twoend", "", twoend, sizearray(twoend), inifile);
  ini_gets("playtime", "threestart", "", threestart, sizearray(threestart),
           inifile);
  ini_gets("playtime", "threeend", "", threeend, sizearray(threeend), inifile);
  ini_gets("playtime", "fourstart", "", fourstart, sizearray(fourstart),
           inifile);
  ini_gets("playtime", "fourend", "", fourend, sizearray(fourend), inifile);
  ini_gets("playtime", "fivestart", "", fivestart, sizearray(fivestart),
           inifile);
  ini_gets("playtime", "fiveend", "", fiveend, sizearray(fiveend), inifile);

  onestart_Hour = atoi(strtok(onestart, "-"));
  onestart_Min = atoi(strtok(NULL, "-"));
  onestart_Time = onestart_Hour * 60 + onestart_Min;
  oneend_Hour = atoi(strtok(oneend, "-"));
  oneend_Min = atoi(strtok(NULL, "-"));
  oneend_Time = oneend_Hour * 60 + oneend_Min;

  twostart_Hour = atoi(strtok(twostart, "-"));
  twostart_Min = atoi(strtok(NULL, "-"));
  twoend_Hour = atoi(strtok(twoend, "-"));
  twoend_Min = atoi(strtok(NULL, "-"));
  twostart_Time = twostart_Hour * 60 + twostart_Min;
  twoend_Time = twoend_Hour * 60 + twoend_Min;

  threestart_Hour = atoi(strtok(threestart, "-"));
  threestart_Min = atoi(strtok(NULL, "-"));
  threeend_Hour = atoi(strtok(threeend, "-"));
  threeend_Min = atoi(strtok(NULL, "-"));
  threestart_Time = threestart_Hour * 60 + threestart_Min;
  threeend_Time = threeend_Hour * 60 + threeend_Min;

  fourstart_Hour = atoi(strtok(fourstart, "-"));
  fourstart_Min = atoi(strtok(NULL, "-"));
  fourend_Hour = atoi(strtok(fourend, "-"));
  fourend_Min = atoi(strtok(NULL, "-"));
  fourstart_Time = fourstart_Hour * 60 + fourstart_Min;
  fourend_Time = fourend_Hour * 60 + fourend_Min;

  fivestart_Hour = atoi(strtok(fivestart, "-"));
  fivestart_Min = atoi(strtok(NULL, "-"));
  fiveend_Hour = atoi(strtok(fiveend, "-"));
  fiveend_Min = atoi(strtok(NULL, "-"));
  fivestart_Time = fivestart_Hour * 60 + fivestart_Min;
  fiveend_Time = fiveend_Hour * 60 + fiveend_Min;

  return 0;
}

char *genRandomString1(int length) {
  int flag, i;
  char *string;
  srand((unsigned)time(NULL));
  if ((string = (char *)malloc(length)) == NULL) {
    return NULL;
  }

  for (i = 0; i < length - 1; i++) {
    flag = rand() % 3;
    switch (flag) {
    case 0:
      string[i] = 'A' + rand() % 26;
      break;
    case 1:
      string[i] = 'a' + rand() % 26;
      break;
    case 2:
      string[i] = '0' + rand() % 10;
      break;
    default:
      string[i] = 'x';
      break;
    }
  }
  string[length - 1] = '\0';
  return string;
}

int cmd_status = -1;

void pub304(int kind, char *mqttContent) {
// 创建进程发送数据
  char tmp[512];
  int system_status;
  char *id = genRandomString1(16);
  sprintf(tmp, "mosquitto_pub -h '%s' -p %d -u '%s' -P '%s' -t '%s' -m "
                 "'{\"id\":\"%s\",\"deviceID\":"
                "\"%s\",\"status\":10,\"type\":304,\"kind\":\"%d\",\"content\":\"%s\"}' ",
          host, port,  username, password, pub, id, deviceID, kind, mqttContent);
  system_status = system(tmp);
  if (system_status != 0) {
    printf("pub304 fail!\n");
  }else{
    printf("pub304 success!\n");
  }
}

static void *_renti_run(void *arg) {
  printf("%s _renti_run begin!\n", tag);
  // extern int gpio_value;
  // extern char *voice1_value;
  // extern char *voice2_value;
  // extern char *AmplifierOn_value;
  gpio_t *gpio_renti;
  bool renti_value;
  // unsigned int renti_input = gpio_value;
  unsigned int renti_input = 203;
  gpio_renti = gpio_new();
  gpio_open_sysfs(gpio_renti, renti_input, GPIO_DIR_IN);
  gpio_set_direction(gpio_renti, GPIO_DIR_IN);
  int i = -1;
  int is_play = 0;
  int noboday_time = 0;
  time_t rawtime;
  struct tm *info;
  int timeMin;
  int timeHour;
  ini_init();
  printf("%s _renti_run begin!\n", tag);
  int noboday_time = 0;
  time_t rawtime;
  struct tm *info;
  int timeMin;
  int timeHour;
  int timeNow;
  FILE *fp;
  char gpio_value[2];
  if (access("/sys/class/gpio/gpio203", F_OK) != 0) {
    system("echo \"203\" > /sys/class/gpio/export");
  }
  system("echo \"in\" > /sys/class/gpio/gpio203/direction");
  fp = fopen("/sys/class/gpio/gpio203/value", "r");
	if (fp == NULL) {
    printf("cat: can't open gpio203 ! \n");
  }

  struct renti *rui_renti = (struct renti *)arg;
  while (rui_renti->running) {
    fp = fopen("/sys/class/gpio/gpio203/value", "r");
	  if (fp == NULL) {
      printf("cat: can't open gpio203 ! \n");
    }
    fgets(gpio_value,2,fp);
    printf("gpio_value %c\n", gpio_value[0]);
    fclose(fp);
    // 没人
    if ('0' == gpio_value[0]) {
      usleep(1000000);
      // printf("no body !\n");
      noboday_time++;
    }

    if (noboday_time > period) {
      if (noboday_time > 3600) {
        noboday_time = period+1;
      }

      if ('1' == gpio_value[0]) {
        noboday_time = 0;
        time(&rawtime);
        /* 获取 GMT 时间 */
        // 转换成分钟判断
        info = gmtime(&rawtime);
        timeHour = (info->tm_hour + 8) % 24;
        timeMin = info->tm_min;
        timeNow = timeHour * 60 + timeMin;
        char *mqtt304;

        // 第一段时间
        if (onestart_Time <= timeNow && timeNow <= oneend_Time) {
          printf("onestart \n");
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 1.wav");
          if (cmd_status != 0) {
            pub304(1, mqtt304);
            continue;
          }
          pub304(3, mqtt304);
          continue;
        }

        // 第二段时间
        if (twostart_Time <= timeNow && timeNow <= twoend_Time) {
          printf("twostart \n");
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 2.wav");
          if (cmd_status != 0) {
            pub304(1, mqtt304);
            continue;
          }
          pub304(3, mqtt304);
          continue;
        }

        // 第三段时间
        if (threestart_Time <= timeNow && timeNow <= threeend_Time) {
          printf("threestart \n");
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 3.wav");
          if (cmd_status != 0) {
            pub304(1, mqtt304);
            continue;
          }
          pub304(3, mqtt304);
          continue;
        }

        // 第四段时间
        if (fourstart_Time <= timeNow && timeNow <= fourend_Time) {
          printf("fourstart \n");
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 4.wav");
          if (cmd_status != 0) {
            pub304(1, mqtt304);
            continue;
          }
          pub304(3, mqtt304);
          continue;
        }

        // 第五段时间
        if (fivestart_Time <= timeNow && timeNow <= fiveend_Time) {
          printf("fivestart \n");
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 5.wav");
          if (cmd_status != 0) {
            pub304(1, mqtt304);
            continue;
          }
          pub304(3, mqtt304);
          continue;
        }
      }
    }

    // 有人
    if ('1' == gpio_value[0]) {
      noboday_time = 0;
    }
  }
  return 0
}
