#define ALSA_PCM_NEW_HW_PARAMS_API

#include "record.h"
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static char *tag = "m-record-v0.1.1";

int rc;
int size;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
unsigned int val;
int dir;
snd_pcm_uframes_t frames;
char *buffer;

int record_delete_operate(void *arg) {
  struct record_operate *record_data = (struct record_operate *)arg;
  if (record_data) {
    free(record_data);
    record_data = NULL;
  }
  return 0;
}

static void *_record_run(void *arg) {
  printf("%s _record_run begin!\n", tag);
  int is_record = 0;
  if ((access("/root/speech8", F_OK)) != 0) {
    mkfifo("/root/speech8", 0777);
  }
  printf("%s open speech8 begin!\n", tag);
  FILE *audio = fopen("/root/speech8", "wb+");
  struct record *rui_record = (struct record *)arg;
  while (rui_record->running) {
    struct record_operate *record_data =
        (struct record_operate *)outQueue(rui_record->lq);
    if (record_data) {
      is_record = record_data->type;
    }
    // printf("record_run : %d!\n", is_record);
    if (1 == is_record) {
      size = frames * (16 / 8) * 8;
      // size = 3200 * 8;
      buffer = (char *)malloc(size);
      rc = snd_pcm_readi(handle, buffer, frames);
      if (rc == -EPIPE) {
        // EPIPE means overrun
        fprintf(stderr, "overrun occurred/n");
        snd_pcm_prepare(handle);
      } else if (rc < 0) {
        fprintf(stderr, "error from read: %s/n", snd_strerror(rc));
      } else if (rc != (int)frames) {
        fprintf(stderr, "short read, read %d frames/n", rc);
      }
      fwrite(buffer, size, 1, audio);
    }
  }
  fclose(audio);
  return NULL;
}

struct record *record_init(struct export *ex) {
  printf("%s init begin!\n", tag);
  rc = snd_pcm_open(&handle, "hw:M8,0", SND_PCM_STREAM_CAPTURE, 0);
  if (rc < 0) {
    fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
    return NULL;
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(handle, params, 8);
  val = 16000;
  snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
  frames = 64; // why?
  snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
  rc = snd_pcm_hw_params(handle, params);
  if (rc < 0) {
    fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
    return NULL;
  }
  snd_pcm_hw_params_get_period_size(params, &frames,
                                    &dir); // get real frames!

  printf("frames=%d\n", frames);

  struct record *rui_record = (struct record *)malloc(sizeof(struct record));
  rui_record->lq = newQueue();
  rui_record->running = 1;
  printf("%s init end!\n", tag);
  return rui_record;
}

int record_run(struct record *mrecord) {
  printf("%s run begin!\n", tag);
  pthread_create(&mrecord->pid, NULL, _record_run, (void *)mrecord);
  printf("%s run end!\n", tag);
  return 0;
}

int record_exit(struct record *mrecord) {
  mrecord->running = 0;
  pthread_join(mrecord->pid, NULL);
  return 0;
}

int record_delete(struct record *mrecord) {
  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer);
  if (mrecord) {
    if (mrecord->lq) {
      deleteQueue(mrecord->lq, record_delete_operate);
      mrecord->lq = NULL;
    }

    free(mrecord);
    mrecord = NULL;
  }
  return 0;
}
