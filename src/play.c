#include "play.h"
#include "record.h"
#include "vad.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int play_delete_operate(void *arg) {
  struct play_operate *data = (struct play_operate *)arg;
  if (data) {
    if (data->play_path) {
      free(data->play_path);
      data->play_path = NULL;
    }
    free(data);
    data = NULL;
  }
  return 0;
}

static void *_play_run(void *arg) {
  extern char wakeupVoice[50];
  struct export *ex = (struct export *)arg;
  while (ex->play->running) {
    struct play_operate *data = (struct play_operate *)outQueue(ex->play->lq);
    if (data) {
      switch (data->type) {
      case PLAY_WAKEUP: {
        int wozaiflag = -1;
        wozaiflag = system(wakeupVoice);
        while (wozaiflag != 0) {
          usleep(1000000);
          wozaiflag = system(wakeupVoice);
        }
        // open VAD
        struct vad_operate *start_data =
            (struct vad_operate *)malloc(sizeof(struct vad_operate));
        assert(start_data);
        start_data->type = MINDARY_DUILITE_VAD_START;
        start_data->audio = NULL;
        inQueue(ex->vad->lq, (void *)start_data);
        break;
      }
      case PLAY_DDS_MUSIC: {
        char tmp[256];
        int player_state = -1;
        // printf("PLAY_DDS_MUSIC speakUrl: %s\n", data->play_path);
        // close VAD
        struct vad_operate *end_data =
            (struct vad_operate *)malloc(sizeof(struct vad_operate));
        end_data->type = MINDARY_DUILITE_VAD_STOP;
        end_data->audio = NULL;
        inQueue(ex->vad->lq, (void *)end_data);
        break;
      }
      default:
        break;
      }
      play_delete_operate((void *)data);
      // open record
      struct record_operate *record_data =
          (struct record_operate *)malloc(sizeof(struct record_operate));
      record_data->type = RUI_RECORD_START;
      inQueue(ex->record->lq, (void *)record_data);
    }
    usleep(10000);
  }
  return NULL;
}

struct play *play_init(struct export *ex) {
  struct play *mindary_play = (struct play *)malloc(sizeof(struct play));
  mindary_play->status = PLAY_IDLE;
  mindary_play->lq = newQueue();
  mindary_play->running = 1;
  return mindary_play;
}

int play_run(struct export *ex) {
  pthread_create(&ex->play->pid, NULL, _play_run, (void *)ex);
  return 0;
}

int play_exit(struct play *mplay) {
  mplay->running = 0;
  pthread_join(mplay->pid, NULL);
  return 0;
}

int play_delete(struct play *mplay) {
  if (mplay) {
    if (mplay->lq) {
      deleteQueue(mplay->lq, play_delete_operate);
      mplay->lq = NULL;
    }
    free(mplay);
    mplay = NULL;
  }
  return 0;
}