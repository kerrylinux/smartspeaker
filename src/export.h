#ifndef __EXPORT_H__
#define __EXPORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define EXPORT_VERSION "0.1"

struct wakeup;
struct ddsasr;
struct vad;
struct play;
struct record;
struct mqtt;
struct renti;
struct connect;

struct export {
  struct mqtt *mqtt;
  struct wakeup *wakeup;
  struct ddsasr *ddsasr;
  struct vad *vad;
  struct play *play;
  struct record *record;
  struct renti *renti;
  struct connect *connect;
  int auth_result;
};

typedef int (*export_ev_callback)(void *userdata, int type, char *msg, int len);
struct export *export_init(char *cfg, export_ev_callback ex_callback,
                           void *userdata);
int export_run(struct export *ex);
int export_enable_wakeup(struct export *ex);
int export_disable_wakeup(struct export *ex);
int export_enable_record(struct export *ex);
int ini_init();
int export_feed_data(struct export *ex, char *data, int len);
int export_exit(struct export *ex);
int export_delete(struct export *ex);

#ifdef __cplusplus
}
#endif

#endif