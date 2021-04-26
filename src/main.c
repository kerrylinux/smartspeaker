#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ddsasr.h"
#include "queue.h"

static char *smartspeaker_tag = "m-smartspeaker-v0.1.1";

int main() {
  ini_init();
  printf("%s _smartspeaker_run begin!\n", smartspeaker_tag);
  if (access("/root/speech8", F_OK) != 0) {
    mkfifo("/root/speech8", 0777);
  }
  system("alsactl -f /etc/asound.state restore");
  sleep(1);

  struct export *ex = export_init(NULL, NULL, NULL);
  export_run(ex);
  export_enable_wakeup(ex);

  FILE *audio = fopen("/root/speech8", "r");
  // FILE *audioinput = fopen("/root/input.pcm", "a+");
  assert(audio != NULL);
  // assert(audioinput != NULL);
  while (1) {
    if (dds_status == DDS_STATUS_IDLE)
      break;
    usleep(10000);
  }
  export_enable_record(ex);
  fseek(audio, 0, SEEK_SET);
  char buf[3200 * 8];
  int len;
  while (1) {
    len = fread(buf, 1, sizeof(buf), audio);
    if (0 == len) {
      continue;
    }
    // fwrite(buf, 1, len, audioinput);
    export_feed_data(ex, buf, len);
    usleep(100000);
  }

  fclose(audio);
  // fclose(audioinput);
  export_disable_wakeup(ex);
  export_exit(ex);
  export_delete(ex);
  return 0;
}