#include "base64.h"
#include "cJSON.h"
#include "minIni.h"
#include <fcntl.h>
#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ifaddrs.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>

#define sizearray(a) (sizeof(a) / sizeof((a)[0]))

static char *tag = "mqttsub-v0.1.6";
const char inifile[] = "config.ini";

char host[50];
long port;
int keepalive;
char sub[50];
char pub[50];
char username[50];
char password[50];
char deviceID[50];

long volume_value;
static int smixer_level = 0;
static struct snd_mixer_selem_regopt smixer_options;
static char card[64] = "default";

int ini_init() {
  ini_gets("mqtt", "host", "", host, sizearray(host), inifile);
  port = ini_getl("mqtt", "port", 1883, inifile);
  keepalive = ini_getl("mqtt", "keepalive", 60, inifile);
  ini_gets("mqtt", "sub", "", sub, sizearray(sub), inifile);
  ini_gets("mqtt", "pub", "", pub, sizearray(pub), inifile);
  ini_gets("mqtt", "username", "", username, sizearray(username), inifile);
  ini_gets("mqtt", "password", "", password, sizearray(password), inifile);
  ini_gets("system", "deviceID", "", deviceID, sizearray(deviceID), inifile);

  return 0;
}

int go_ping(char *svrip) {
  pid_t pid;
  int stat;

  if ((pid = vfork()) < 0) {
    printf("vfork error");
  }
  if (pid == 0) {
    if (execlp("ping", "ping", "-c", "1", svrip, (char *)0) < 0) {
      printf("execlp error\n");
    }
  }

  waitpid(pid, &stat, 0);
  if (stat == 0) {
    return 0;
  }

  return -1;
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

/*
    将Json写入文件中
*/
int writeflietocon() {
  cJSON *root = cJSON_CreateObject();
  char *id = genRandomString1(16);
  cJSON_AddItemToObject(root, "id", cJSON_CreateString(id));
  cJSON_AddItemToObject(root, "deviceID", cJSON_CreateString(deviceID));
  cJSON_AddItemToObject(root, "status", cJSON_CreateNumber(10));
  cJSON_AddItemToObject(root, "type", cJSON_CreateNumber(305));
  cJSON_AddItemToObject(root, "kind", cJSON_CreateNumber(100));
  cJSON *content = cJSON_CreateObject();
  cJSON *host = cJSON_CreateObject();
  cJSON_AddItemToObject(content, "host", host);
  cJSON_AddItemToObject(host, "arch", cJSON_CreateString("Cortex-A7"));
  cJSON_AddItemToObject(host, "ip", cJSON_CreateString("127.0.0.1"));
  cJSON_AddItemToObject(host, "mac", cJSON_CreateString("9c:b6:d0:00:00:02"));
  cJSON_AddItemToObject(host, "software", cJSON_CreateString("V1.0"));
  cJSON_AddItemToObject(host, "hardware", cJSON_CreateString("V1.0"));
  cJSON_AddItemToObject(host, "os", cJSON_CreateString("TinaV2.2"));
  unsigned char *contentbase64 =
      base64_encode((unsigned char *)cJSON_Print(content));
  cJSON_AddItemToObject(root, "content",
                        cJSON_CreateString((char *)contentbase64));

  if ((access("/root/connet.json", F_OK)) != -1) {
    system("rm /root/connet.json");
  }
  FILE *fpcon = fopen("/root/connet.json", "w");
  fwrite(cJSON_Print(root), strlen(cJSON_Print(root)), 1, fpcon);
  fclose(fpcon);
  cJSON_Delete(root);

  return 0;
}

int mospub_connect() {
  char tmp[512];
  sprintf(tmp, "mosquitto_pub -h '%s' -p %d -u '%s' -P '%s' -t '%s' -f '/root/connet.json' ",
          host, port,  username, password, pub);
  system(tmp);
  return 0;
}

static void *_connect_run(void *arg) {
  printf("_connect_run begin!\n");
  writeflietocon();
  while (1) {
    mospub_connect();
    sleep(3600 * 12);
  }
  return NULL;
}

int connect_run() {
  pthread_t c;
  pthread_create(&c,NULL,_connect_run,NULL);
  return 0;
}

static void error(const char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "volume_value: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

static int (* const get_raw_range[2])(snd_mixer_elem_t *, long *, long *) = {
	snd_mixer_selem_get_playback_volume_range,
};
static int (* const get_raw[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *) = {
	snd_mixer_selem_get_playback_volume,
};
static int (* const set_raw[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long) = {
	snd_mixer_selem_set_playback_volume,
};

struct volume_ops {
	int (*get_range)(snd_mixer_elem_t *elem, long *min, long *max);
	int (*get)(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t c,
		   long *value);
	int (*set)(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t c,
		   long value, int dir);
};

struct volume_ops_set {
	int (*has_volume)(snd_mixer_elem_t *elem);
	struct volume_ops v[3];
};

static int set_playback_raw_volume(snd_mixer_elem_t *elem,
				   snd_mixer_selem_channel_id_t c,
				   long value, int dir)
{
	return snd_mixer_selem_set_playback_volume(elem, c, value);
}

static const struct volume_ops_set vol_ops[2] = {
	{
		.has_volume = snd_mixer_selem_has_playback_volume,
		.v = {{ snd_mixer_selem_get_playback_volume_range,
			snd_mixer_selem_get_playback_volume,
			set_playback_raw_volume },
		},
	},
};

static void show_selem_volume(snd_mixer_elem_t *elem, 
			      snd_mixer_selem_channel_id_t chn, int dir,
			      long min, long max)
{
	long raw, val;
	vol_ops[0].v[0].get(elem, chn, &raw);
	printf(" %li", raw);
	volume_value = raw;
}

static int show_selem(snd_mixer_t *handle, snd_mixer_selem_id_t *id, const char *space)
{
	snd_mixer_selem_channel_id_t chn;
	long pmin = 0, pmax = 0;
	long cmin = 0, cmax = 0;
	int psw, csw;
	int pmono, cmono, mono_ok = 0;
	snd_mixer_elem_t *elem;
	
	elem = snd_mixer_find_selem(handle, id);
	if (!elem) {
		error("Mixer %s simple element not found", card);
		return -ENOENT;
	}

	if (1) {
		pmono = snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO) &&
		        (snd_mixer_selem_is_playback_mono(elem) || 
			 (!snd_mixer_selem_has_playback_volume(elem) &&
			  !snd_mixer_selem_has_playback_switch(elem)));
		cmono = snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO) &&
		        (snd_mixer_selem_is_capture_mono(elem) || 
			 (!snd_mixer_selem_has_capture_volume(elem) &&
			  !snd_mixer_selem_has_capture_switch(elem)));

		if (pmono || cmono) {
			if (!mono_ok) {
				printf("%s%s:", space, "Mono");
				mono_ok = 1;
			}
			if (snd_mixer_selem_has_common_volume(elem)) {
				show_selem_volume(elem, SND_MIXER_SCHN_MONO, 0, pmin, pmax);
			}
		}
		if (pmono && snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO)) {
			int title = 0;
			if (!mono_ok) {
				printf("%s%s:", space, "Mono");
				mono_ok = 1;
			}
			if (!snd_mixer_selem_has_common_volume(elem)) {
				if (snd_mixer_selem_has_playback_volume(elem)) {
					printf(" Playback");
					title = 1;
					show_selem_volume(elem, SND_MIXER_SCHN_MONO, 0, pmin, pmax);
				}
			}
		}
		if (pmono || cmono)
			printf("\n");
		if (!pmono || !cmono) {
			for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
				if ((pmono || !snd_mixer_selem_has_playback_channel(elem, chn)) &&
				    (cmono || !snd_mixer_selem_has_capture_channel(elem, chn)))
					continue;
				printf("%s%s:", space, snd_mixer_selem_channel_name(chn));
				if (!pmono && !cmono && snd_mixer_selem_has_common_volume(elem)) {
					show_selem_volume(elem, chn, 0, pmin, pmax);
				}
				if (!pmono && snd_mixer_selem_has_playback_channel(elem, chn)) {
					int title = 0;
					if (!snd_mixer_selem_has_common_volume(elem)) {
						if (snd_mixer_selem_has_playback_volume(elem)) {
							printf(" Playback");
							title = 1;
							show_selem_volume(elem, chn, 0, pmin, pmax);
						}
					}
				}
				printf("\n");
			}
		}
	}
	return 0;
}

static int parse_simple_id(const char *str, snd_mixer_selem_id_t *sid)
{
	int c, size;
	char buf[128];
	char *ptr = buf;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	size = 1;	/* for '\0' */
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
		c = *str++;
		while (*str && *str != c) {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
		if (*str == c)
			str++;
	}
	if (*str == '\0') {
		snd_mixer_selem_id_set_index(sid, 0);
		*ptr = 0;
		goto _set;
	}
	if (*str != ',')
		return -EINVAL;
	*ptr = 0;	/* terminate the string */
	str++;
	if (!isdigit(*str))
		return -EINVAL;
	snd_mixer_selem_id_set_index(sid, atoi(str));
       _set:
	snd_mixer_selem_id_set_name(sid, buf);
	return 0;
}

static int sset()
{
	int err = 0;
	static snd_mixer_t *handle = NULL;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);

	if (parse_simple_id("headphone volume", sid)) {
		return 1;
	}
	// if (parse_simple_id("Master", sid)) {
	// 	return 1;
	// }
	if ((err = snd_mixer_open(&handle, 0)) < 0) {
			error("Mixer %s open error: %s\n", card, snd_strerror(err));
			return err;
	}
	if (smixer_level == 0 && (err = snd_mixer_attach(handle, card)) < 0) {
			error("Mixer attach %s error: %s", card, snd_strerror(err));
			snd_mixer_close(handle);
			handle = NULL;
			return err;
	}
	if ((err = snd_mixer_selem_register(handle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
			error("Mixer register error: %s", snd_strerror(err));
			snd_mixer_close(handle);
			handle = NULL;
			return err;
	}
	err = snd_mixer_load(handle);
	if (err < 0) {
			error("Mixer %s load error: %s", card, snd_strerror(err));
			snd_mixer_close(handle);
			handle = NULL;
			return err;
	}

	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		if (0)
			return 0;
		error("Unable to find simple control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		snd_mixer_close(handle);
		handle = NULL;
		return -ENOENT;
	}

	show_selem(handle, sid, "  ");

 done:
	snd_mixer_close(handle);
	handle = NULL;
	return err < 0 ? 1 : 0;
}

int mospub_headphone_volume() {
  char tmp[512];
  int system_status;
  char *id = genRandomString1(16);
  sprintf(tmp, "mosquitto_pub -h '%s' -p %d -u '%s' -P '%s' -t '%s' -m "
                 "'{\"id\":\"%s\",\"deviceID\":"
                "\"%s\",\"status\":10,\"type\":303,\"kind\":\"%li\"}' ",
          host, port,  username, password, pub, id, deviceID, volume_value);
  system_status = system(tmp);
  printf("_headphone_volume_run system_status %d!\n",system_status);
  printf("_headphone_volume_run tmp %s!\n",tmp);
  return 0;
}

static void *_headphone_volume_run(void *arg) {
  printf("_headphone_volume_run begin!\n");
  int level = 0;
	smixer_options.device = card;
  while (1) {
	  sset();
    mospub_headphone_volume();
    sleep(3600);
  }
  return NULL;
}

int headphone_volume_run(){
  pthread_t h;
  pthread_create(&h,NULL,_headphone_volume_run,NULL);
  return 0;
}

bool session = false;
int cmd_status = -1;
int adjust_device_volume = 0;
int play_lajifenlei_music = 0;
int alter_lajifenlei_time = 0;
int restart_speech_once = 0;
unsigned char *contentbase64;

struct playmusicstring
{
  char mqtt_content_string[100];
  char music_name_string[100];
} PlayString;  

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
  printf("pub304 system_status %d!\n",system_status);
  // printf("pub304 tmp %s!\n",tmp);
}

static void *_play_music(void *arg) {
  char music[128];
  printf("_play_music begin!\n");
  sprintf(music, "aplay -D hw:sndcodec,0 -f S16_LE -r 16000 -c 2 /voice/%s", PlayString.music_name_string);
  cmd_status = system(music);
  if (cmd_status != 0) {
    // 上传播放失败
    pub304(1, PlayString.mqtt_content_string);
    return NULL;
  }
  // 上传播放结束
  pub304(3, PlayString.mqtt_content_string);
  return NULL;
}

void my_message_callback(struct mosquitto *mosq, void *userdata,
                         const struct mosquitto_message *message) {
  if (message->payloadlen) {

    printf("\n %s %s \n", message->topic, message->payload);
    cJSON *message_payload, *mqtt_content, *mqtt_id, *mqtt_type, *mqtt_kind;
    cJSON *mqtt_content_base64_decode, *time_start, *time_end, *music_name;
    message_payload = cJSON_Parse(message->payload);

    if (!message_payload) {
      printf("JSON格式错误:%s\n\n", cJSON_GetErrorPtr()); //输出json格式错误信息
    }

    if (message_payload) {
      printf("JSON格式正确:\n%s\n\n", cJSON_Print(message_payload));
      mqtt_id = cJSON_GetObjectItem(message_payload, "id");
      mqtt_type = cJSON_GetObjectItem(message_payload, "type");
      mqtt_kind = cJSON_GetObjectItem(message_payload, "kind");

      printf("mqtt_id: %s \r\n", mqtt_id->valuestring);
      printf("mqtt_type: %d \r\n", mqtt_type->valueint);
      printf("mqtt_kind: %d \r\n", mqtt_kind->valueint);

      switch (mqtt_type->valueint) {
      case 201: // adjust device volume
        printf("adjust device volume\r\n");
        char tmp[128];
        sprintf(tmp,
                "amixer cset numid=1,iface=MIXER,name='headphone volume' 0%d",
                mqtt_kind->valueint);
        printf("system tmp: %s \r\n", tmp);
        system(tmp);
        system("alsactl -f /etc/asound.state  store");
        system("alsactl -f /etc/asound.state restore");
        break;
      case 202: // play lajifenlei music
        printf("play lajifenlei music\r\n");
        // 上传开始播放
        char *mqtt304;
        if (1 == mqtt_kind->valueint) {
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 1.wav");
        }
        if (2 == mqtt_kind->valueint) {
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 2.wav");
        }
        if (3 == mqtt_kind->valueint) {
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 3.wav");
        }
        if (4 == mqtt_kind->valueint) {
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 4.wav");
        }
        if (5 == mqtt_kind->valueint) {
          mqtt304 = "=";
          pub304(2, mqtt304);
          cmd_status = system("aplay -D hw:sndcodec,0 5.wav");
        }
        if (cmd_status != 0) {
          // 播放失败
          pub304(1, mqtt304);
          break;
        }
        // 上传播放结束
        pub304(3, mqtt304);
        break;
      case 203: // alter lajifenlei time
        printf("alter lajifenlei time\r\n");
        mqtt_content = cJSON_GetObjectItem(message_payload, "content");
        contentbase64 =
            base64_decode((unsigned char *)(mqtt_content->valuestring));
        printf("contentbase64 %s \r\n", contentbase64);

        mqtt_content_base64_decode = cJSON_Parse(contentbase64);
        time_start = cJSON_GetObjectItem(mqtt_content_base64_decode, "start");
        printf("time_start %s \r\n", time_start->valuestring);
        time_end = cJSON_GetObjectItem(mqtt_content_base64_decode, "end");
        printf("time_end %s \r\n", time_end->valuestring);

        if (1 == mqtt_kind->valueint) {
          ini_puts("playtime", "onestart", time_start->valuestring, inifile);
          ini_puts("playtime", "oneend", time_end->valuestring, inifile);
        }
        if (2 == mqtt_kind->valueint) {
          ini_puts("playtime", "twostart", time_start->valuestring, inifile);
          ini_puts("playtime", "twoend", time_end->valuestring, inifile);
        }
        if (3 == mqtt_kind->valueint) {
          ini_puts("playtime", "threestart", time_start->valuestring, inifile);
          ini_puts("playtime", "threeend", time_end->valuestring, inifile);
        }
        if (4 == mqtt_kind->valueint) {
          ini_puts("playtime", "fourstart", time_start->valuestring, inifile);
          ini_puts("playtime", "fourend", time_end->valuestring, inifile);
        }
        if (5 == mqtt_kind->valueint) {
          ini_puts("playtime", "fivestart", time_start->valuestring, inifile);
          ini_puts("playtime", "fiveend", time_end->valuestring, inifile);
        }
        break;
      case 204:
        break;

      case 205: // play music
        printf("play music\r\n");
        mqtt_content = cJSON_GetObjectItem(message_payload, "content");
        contentbase64 =
            base64_decode((unsigned char *)(mqtt_content->valuestring));
        printf("contentbase64 %s \r\n", contentbase64);
        mqtt_content_base64_decode = cJSON_Parse(contentbase64);
        music_name = cJSON_GetObjectItem(mqtt_content_base64_decode, "name");
        // 上传开始播放
        pub304(2, mqtt_content->valuestring);
        strcpy(PlayString.mqtt_content_string, mqtt_content->valuestring);
        strcpy(PlayString.music_name_string, music_name->valuestring);
        pthread_t p;
        pthread_create(&p,NULL,_play_music,NULL);
        break;

      case 206: // kill aplay
        printf("kill aplay\r\n");
        system("ps w|grep aplay|grep -v grep|awk '{print $1}'|xargs kill "
               "-9");
        break;

      default:
        break;
      }
    }

    cJSON_Delete(message_payload); //释放内存
  }
  // printf("%s (null)\n", message->topic);
  // fflush(stdout);
}

void my_connect_callback(struct mosquitto *mosq, void *userdata, int result) {
  if (!result) {
    /* Subscribe to broker information topics on successful connect. */
    mosquitto_subscribe(mosq, NULL, sub, 2);
  } else {
    fprintf(stderr, "Connect failed\n");
  }
}

void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid,
                           int qos_count, const int *granted_qos) {
  int i;
  printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
  for (i = 1; i < qos_count; i++) {
    printf(", %d", granted_qos[i]);
  }
  printf("\n");
}

void my_log_callback(struct mosquitto *mosq, void *userdata, int level,
                     const char *str) {
  /* Pring all log messages regardless of level. */
  printf("%s\n", str);
}

int main() {
  ini_init();
  printf("%s _mqttsub_run begin!\n", tag);
  int ret = -1;
  do {
    ret = go_ping("www.baidu.com");
    printf("+++++++++++++ret:%d\n", ret);
    sleep(3);
  } while (ret != 0);

  struct mosquitto *mosq = NULL;
  // libmosquitto 库初始化
  mosquitto_lib_init();
  // 创建 mosquitto 客户端
  mosq = mosquitto_new(deviceID, session, NULL);
  if (!mosq) {
    printf("create client failed..\n");
    mosquitto_lib_cleanup();
    return 1;
  }
  mosquitto_username_pw_set(mosq, username, password);

  // 设置回调函数，需要时可使用
  mosquitto_log_callback_set(mosq, my_log_callback);
  mosquitto_connect_callback_set(mosq, my_connect_callback);
  mosquitto_message_callback_set(mosq, my_message_callback);
  mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

  // 客户端连接服务器
  if (mosquitto_connect(mosq, host, port, keepalive)) {
    printf("Unable to connect.\n");
    return 1;
  }

  // 303 上传当前音量
  headphone_volume_run();

  // 305 上传设备属性信息
  connect_run();

  // 循环处理网络消息
  mosquitto_loop_forever(mosq, -1, 1);

  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();

  return 0;
}