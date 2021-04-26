#include "gpioin.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

struct gpio_ops {
  int (*read)(gpio_t *gpio, bool *value);
  int (*write)(gpio_t *gpio, bool value);
  int (*read_event)(gpio_t *gpio, gpio_edge_t *edge, uint64_t *timestamp);
  int (*poll)(gpio_t *gpio, int timeout_ms);
  int (*close)(gpio_t *gpio);
  int (*get_direction)(gpio_t *gpio, gpio_direction_t *direction);
  int (*get_edge)(gpio_t *gpio, gpio_edge_t *edge);
  int (*set_direction)(gpio_t *gpio, gpio_direction_t direction);
  int (*set_edge)(gpio_t *gpio, gpio_edge_t edge);
  unsigned int (*line)(gpio_t *gpio);
  int (*fd)(gpio_t *gpio);
  int (*name)(gpio_t *gpio, char *str, size_t len);
  int (*chip_fd)(gpio_t *gpio);
  int (*chip_name)(gpio_t *gpio, char *str, size_t len);
  int (*chip_label)(gpio_t *gpio, char *str, size_t len);
  int (*tostring)(gpio_t *gpio, char *str, size_t len);
};

struct gpio_handle {
  const struct gpio_ops *ops;

  /* gpio-sysfs and gpio-cdev state */
  unsigned int line;
  int line_fd;

  /* gpio-cdev state */
  int chip_fd;
  gpio_direction_t direction;
  gpio_edge_t edge;

  /* error state */
  struct {
    int c_errno;
    char errmsg[96];
  } error;
};

static const struct gpio_ops gpio_sysfs_ops;
static const struct gpio_ops gpio_cdev_ops;

gpio_t *gpio_new(void) {
  gpio_t *gpio = calloc(1, sizeof(gpio_t));
  if (gpio == NULL)
    return NULL;

  gpio->ops = &gpio_cdev_ops;
  gpio->line = -1;
  gpio->line_fd = -1;
  gpio->chip_fd = -1;

  return gpio;
}

void gpio_freein(gpio_t *gpio) { free(gpio); }

const char *gpio_errmsg(gpio_t *gpio) { return gpio->error.errmsg; }

int gpio_errno(gpio_t *gpio) { return gpio->error.c_errno; }

int gpio_read(gpio_t *gpio, bool *value) {
  return gpio->ops->read(gpio, value);
}

int gpio_write(gpio_t *gpio, bool value) {
  return gpio->ops->write(gpio, value);
}

int gpio_read_event(gpio_t *gpio, gpio_edge_t *edge, uint64_t *timestamp) {
  return gpio->ops->read_event(gpio, edge, timestamp);
}

int gpio_poll(gpio_t *gpio, int timeout_ms) {
  return gpio->ops->poll(gpio, timeout_ms);
}

int gpio_close(gpio_t *gpio) { return gpio->ops->close(gpio); }

int gpio_get_direction(gpio_t *gpio, gpio_direction_t *direction) {
  return gpio->ops->get_direction(gpio, direction);
}

int gpio_get_edge(gpio_t *gpio, gpio_edge_t *edge) {
  return gpio->ops->get_edge(gpio, edge);
}

int gpio_set_direction(gpio_t *gpio, gpio_direction_t direction) {
  return gpio->ops->set_direction(gpio, direction);
}

int gpio_set_edge(gpio_t *gpio, gpio_edge_t edge) {
  return gpio->ops->set_edge(gpio, edge);
}

unsigned int gpio_line(gpio_t *gpio) { return gpio->ops->line(gpio); }

int gpio_fd(gpio_t *gpio) { return gpio->ops->fd(gpio); }

int gpio_name(gpio_t *gpio, char *str, size_t len) {
  return gpio->ops->name(gpio, str, len);
}

int gpio_chip_fd(gpio_t *gpio) { return gpio->ops->chip_fd(gpio); }

int gpio_chip_name(gpio_t *gpio, char *str, size_t len) {
  return gpio->ops->chip_name(gpio, str, len);
}

int gpio_chip_label(gpio_t *gpio, char *str, size_t len) {
  return gpio->ops->chip_label(gpio, str, len);
}

int gpio_tostring(gpio_t *gpio, char *str, size_t len) {
  return gpio->ops->tostring(gpio, str, len);
}

static int _gpio_error(gpio_t *gpio, int code, int c_errno, const char *fmt,
                       ...) {
  va_list ap;

  gpio->error.c_errno = c_errno;

  va_start(ap, fmt);
  vsnprintf(gpio->error.errmsg, sizeof(gpio->error.errmsg), fmt, ap);
  va_end(ap);

  /* Tack on strerror() and errno */
  if (c_errno) {
    char buf[64];
    strerror_r(c_errno, buf, sizeof(buf));
    snprintf(gpio->error.errmsg + strlen(gpio->error.errmsg),
             sizeof(gpio->error.errmsg) - strlen(gpio->error.errmsg),
             ": %s [errno %d]", buf, c_errno);
  }

  return code;
}

#define P_PATH_MAX 256

/* Delay between checks for successful GPIO ipspeaker (100ms) */
#define GPIO_SYSFS_ipspeaker_STAT_DELAY 100000
/* Number of retries to check for successful GPIO ipspeakers */
#define GPIO_SYSFS_ipspeaker_STAT_RETRIES 10

static const char *gpio_sysfs_direction_to_string[] = {
    [GPIO_DIR_IN] = "in",
    [GPIO_DIR_OUT] = "out",
    [GPIO_DIR_OUT_LOW] = "low",
    [GPIO_DIR_OUT_HIGH] = "high",
};

static const char *gpio_sysfs_edge_to_string[] = {
    [GPIO_EDGE_NONE] = "none",
    [GPIO_EDGE_RISING] = "rising",
    [GPIO_EDGE_FALLING] = "falling",
    [GPIO_EDGE_BOTH] = "both",
};

static int gpio_sysfs_close(gpio_t *gpio) {
  char buf[16];
  int fd;

  if (gpio->line_fd < 0)
    return 0;

  /* Close fd */
  if (close(gpio->line_fd) < 0)
    return _gpio_error(gpio, GPIO_ERROR_CLOSE, errno, "Closing GPIO 'value'");

  gpio->line_fd = -1;

  /* Unipspeaker the GPIO */
  snprintf(buf, sizeof(buf), "%d", gpio->line);
  if ((fd = open("/sys/class/gpio/unipspeaker", O_WRONLY)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_CLOSE, errno,
                       "Closing GPIO: opening 'unipspeaker'");
  if (write(fd, buf, strlen(buf) + 1) < 0) {
    int errsv = errno;
    close(fd);
    return _gpio_error(gpio, GPIO_ERROR_CLOSE, errsv,
                       "Closing GPIO: writing 'unipspeaker'");
  }
  if (close(fd) < 0)
    return _gpio_error(gpio, GPIO_ERROR_CLOSE, errno,
                       "Closing GPIO: closing 'unipspeaker'");

  return 0;
}

static int gpio_sysfs_read(gpio_t *gpio, bool *value) {
  char buf[2];

  /* Read fd */
  if (read(gpio->line_fd, buf, 2) < 0)
    return _gpio_error(gpio, GPIO_ERROR_IO, errno, "Reading GPIO 'value'");

  /* Rewind */
  if (lseek(gpio->line_fd, 0, SEEK_SET) < 0)
    return _gpio_error(gpio, GPIO_ERROR_IO, errno, "Rewinding GPIO 'value'");

  if (buf[0] == '0')
    *value = false;
  else if (buf[0] == '1')
    *value = true;
  else
    return _gpio_error(gpio, GPIO_ERROR_IO, 0, "Unknown GPIO value");

  return 0;
}

static int gpio_sysfs_write(gpio_t *gpio, bool value) {
  char value_str[][2] = {"0", "1"};

  /* Write fd */
  if (write(gpio->line_fd, value_str[value], 2) < 0)
    return _gpio_error(gpio, GPIO_ERROR_IO, errno, "Writing GPIO 'value'");

  /* Rewind */
  if (lseek(gpio->line_fd, 0, SEEK_SET) < 0)
    return _gpio_error(gpio, GPIO_ERROR_IO, errno, "Rewinding GPIO 'value'");

  return 0;
}

static int gpio_sysfs_read_event(gpio_t *gpio, gpio_edge_t *edge,
                                 uint64_t *timestamp) {
  return _gpio_error(gpio, GPIO_ERROR_UNSUPPORTED, 0,
                     "GPIO of type sysfs does not support read event");
}

static int gpio_sysfs_poll(gpio_t *gpio, int timeout_ms) {
  struct pollfd fds[1];
  int ret;

  /* Poll */
  fds[0].fd = gpio->line_fd;
  fds[0].events = POLLPRI | POLLERR;
  if ((ret = poll(fds, 1, timeout_ms)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_IO, errno, "Polling GPIO 'value'");

  /* GPIO edge interrupt occurred */
  if (ret) {
    /* Rewind */
    if (lseek(gpio->line_fd, 0, SEEK_SET) < 0)
      return _gpio_error(gpio, GPIO_ERROR_IO, errno, "Rewinding GPIO 'value'");

    return 1;
  }

  /* Timed out */
  return 0;
}

static int gpio_sysfs_set_direction(gpio_t *gpio, gpio_direction_t direction) {
  char gpio_path[P_PATH_MAX];
  int fd;

  if (direction != GPIO_DIR_IN && direction != GPIO_DIR_OUT &&
      direction != GPIO_DIR_OUT_LOW && direction != GPIO_DIR_OUT_HIGH)
    return _gpio_error(gpio, GPIO_ERROR_ARG, 0,
                       "Invalid GPIO direction (can be in, out, low, high)");

  /* Write direction */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/direction",
           gpio->line);
  if ((fd = open(gpio_path, O_WRONLY)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_CONFIGURE, errno,
                       "Opening GPIO 'direction'");
  if (write(fd, gpio_sysfs_direction_to_string[direction],
            strlen(gpio_sysfs_direction_to_string[direction]) + 1) < 0) {
    int errsv = errno;
    close(fd);
    return _gpio_error(gpio, GPIO_ERROR_CONFIGURE, errsv,
                       "Writing GPIO 'direction'");
  }
  if (close(fd) < 0)
    return _gpio_error(gpio, GPIO_ERROR_CONFIGURE, errno,
                       "Closing GPIO 'direction'");

  return 0;
}

static int gpio_sysfs_get_direction(gpio_t *gpio, gpio_direction_t *direction) {
  char gpio_path[P_PATH_MAX];
  char buf[8];
  int fd, ret;

  /* Read direction */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/direction",
           gpio->line);
  if ((fd = open(gpio_path, O_RDONLY)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errno,
                       "Opening GPIO 'direction'");
  if ((ret = read(fd, buf, sizeof(buf))) < 0) {
    int errsv = errno;
    close(fd);
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errsv,
                       "Reading GPIO 'direction'");
  }
  if (close(fd) < 0)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errno,
                       "Closing GPIO 'direction'");

  buf[ret] = '\0';

  if (strcmp(buf, "in\n") == 0)
    *direction = GPIO_DIR_IN;
  else if (strcmp(buf, "out\n") == 0)
    *direction = GPIO_DIR_OUT;
  else
    return _gpio_error(gpio, GPIO_ERROR_QUERY, 0, "Unknown GPIO direction");

  return 0;
}

static int gpio_sysfs_set_edge(gpio_t *gpio, gpio_edge_t edge) {
  char gpio_path[P_PATH_MAX];
  int fd;

  if (edge != GPIO_EDGE_NONE && edge != GPIO_EDGE_RISING &&
      edge != GPIO_EDGE_FALLING && edge != GPIO_EDGE_BOTH)
    return _gpio_error(
        gpio, GPIO_ERROR_ARG, 0,
        "Invalid GPIO interrupt edge (can be none, rising, falling, both)");

  /* Write edge */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/edge",
           gpio->line);
  if ((fd = open(gpio_path, O_WRONLY)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_CONFIGURE, errno,
                       "Opening GPIO 'edge'");
  if (write(fd, gpio_sysfs_edge_to_string[edge],
            strlen(gpio_sysfs_edge_to_string[edge]) + 1) < 0) {
    int errsv = errno;
    close(fd);
    return _gpio_error(gpio, GPIO_ERROR_CONFIGURE, errsv,
                       "Writing GPIO 'edge'");
  }
  if (close(fd) < 0)
    return _gpio_error(gpio, GPIO_ERROR_CONFIGURE, errno,
                       "Closing GPIO 'edge'");

  return 0;
}

static int gpio_sysfs_get_edge(gpio_t *gpio, gpio_edge_t *edge) {
  char gpio_path[P_PATH_MAX];
  char buf[16];
  int fd, ret;

  /* Read edge */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/edge",
           gpio->line);
  if ((fd = open(gpio_path, O_RDONLY)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errno, "Opening GPIO 'edge'");
  if ((ret = read(fd, buf, sizeof(buf))) < 0) {
    int errsv = errno;
    close(fd);
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errsv, "Reading GPIO 'edge'");
  }
  if (close(fd) < 0)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errno, "Closing GPIO 'edge'");

  buf[ret] = '\0';

  if (strcmp(buf, "none\n") == 0)
    *edge = GPIO_EDGE_NONE;
  else if (strcmp(buf, "rising\n") == 0)
    *edge = GPIO_EDGE_RISING;
  else if (strcmp(buf, "falling\n") == 0)
    *edge = GPIO_EDGE_FALLING;
  else if (strcmp(buf, "both\n") == 0)
    *edge = GPIO_EDGE_BOTH;
  else
    return _gpio_error(gpio, GPIO_ERROR_QUERY, 0, "Unknown GPIO edge");

  return 0;
}

static unsigned int gpio_sysfs_line(gpio_t *gpio) { return gpio->line; }

static int gpio_sysfs_fd(gpio_t *gpio) { return gpio->line_fd; }

static int gpio_sysfs_name(gpio_t *gpio, char *str, size_t len) {
  strncpy(str, "", len);
  return 0;
}

static int gpio_sysfs_chip_fd(gpio_t *gpio) {
  return _gpio_error(gpio, GPIO_ERROR_UNSUPPORTED, 0,
                     "GPIO of type sysfs has no chip fd");
}

static int gpio_sysfs_chip_name(gpio_t *gpio, char *str, size_t len) {
  int ret;
  char gpio_path[P_PATH_MAX];
  char gpiochip_path[P_PATH_MAX];

  /* Form path to device */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/device",
           gpio->line);

  /* Resolve symlink to gpiochip */
  if ((ret = readlink(gpio_path, gpiochip_path, sizeof(gpiochip_path))) < 0)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errno,
                       "Reading GPIO chip symlink");

  /* Null-terminate symlink path */
  gpiochip_path[(ret < P_PATH_MAX) ? ret : (P_PATH_MAX - 1)] = '\0';

  /* Find last / in symlink path */
  const char *sep = strrchr(gpiochip_path, '/');
  if (!sep)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, 0, "Invalid GPIO chip symlink");

  strncpy(str, sep + 1, len);
  str[len - 1] = '\0';

  return 0;
}

static int gpio_sysfs_chip_label(gpio_t *gpio, char *str, size_t len) {
  char gpio_path[P_PATH_MAX];
  char chip_name[32];
  int fd, ret;

  if ((ret = gpio_sysfs_chip_name(gpio, chip_name, sizeof(chip_name))) < 0)
    return ret;

  /* Read gpiochip label */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/%s/label", chip_name);

  if ((fd = open(gpio_path, O_RDONLY)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errno,
                       "Opening GPIO chip 'label'");
  if ((ret = read(fd, str, len)) < 0) {
    int errsv = errno;
    close(fd);
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errsv,
                       "Reading GPIO chip 'label'");
  }
  if (close(fd) < 0)
    return _gpio_error(gpio, GPIO_ERROR_QUERY, errno, "Closing GPIO 'label'");

  str[ret - 1] = '\0';

  return 0;
}

static int gpio_sysfs_tostring(gpio_t *gpio, char *str, size_t len) {
  gpio_direction_t direction;
  const char *direction_str;
  gpio_edge_t edge;
  const char *edge_str;
  char chip_name[32];
  const char *chip_name_str;
  char chip_label[32];
  const char *chip_label_str;

  if (gpio_sysfs_get_direction(gpio, &direction) < 0)
    direction_str = "?";
  else
    direction_str = (direction == GPIO_DIR_IN)
                        ? "in"
                        : (direction == GPIO_DIR_OUT) ? "out" : "unknown";

  if (gpio_sysfs_get_edge(gpio, &edge) < 0)
    edge_str = "?";
  else
    edge_str = (edge == GPIO_EDGE_NONE)
                   ? "none"
                   : (edge == GPIO_EDGE_RISING)
                         ? "rising"
                         : (edge == GPIO_EDGE_FALLING)
                               ? "falling"
                               : (edge == GPIO_EDGE_BOTH) ? "both" : "unknown";

  if (gpio_sysfs_chip_name(gpio, chip_name, sizeof(chip_name)) < 0)
    chip_name_str = "<error>";
  else
    chip_name_str = chip_name;

  if (gpio_sysfs_chip_label(gpio, chip_label, sizeof(chip_label)) < 0)
    chip_label_str = "<error>";
  else
    chip_label_str = chip_label;

  return snprintf(str, len,
                  "GPIO %d (fd=%d, direction=%s, edge=%s, chip_name=\"%s\", "
                  "chip_label=\"%s\", type=sysfs)",
                  gpio->line, gpio->line_fd, direction_str, edge_str,
                  chip_name_str, chip_label_str);
}

static const struct gpio_ops gpio_sysfs_ops = {
    .read = gpio_sysfs_read,
    .write = gpio_sysfs_write,
    .read_event = gpio_sysfs_read_event,
    .poll = gpio_sysfs_poll,
    .close = gpio_sysfs_close,
    .get_direction = gpio_sysfs_get_direction,
    .get_edge = gpio_sysfs_get_edge,
    .set_direction = gpio_sysfs_set_direction,
    .set_edge = gpio_sysfs_set_edge,
    .line = gpio_sysfs_line,
    .fd = gpio_sysfs_fd,
    .name = gpio_sysfs_name,
    .chip_fd = gpio_sysfs_chip_fd,
    .chip_name = gpio_sysfs_chip_name,
    .chip_label = gpio_sysfs_chip_label,
    .tostring = gpio_sysfs_tostring,
};

int gpio_open_sysfs(gpio_t *gpio, unsigned int line,
                    gpio_direction_t direction) {
  char gpio_path[P_PATH_MAX];
  struct stat stat_buf;
  char buf[16];
  int ret, fd;

  if (direction != GPIO_DIR_IN && direction != GPIO_DIR_OUT &&
      direction != GPIO_DIR_OUT_LOW && direction != GPIO_DIR_OUT_HIGH)
    return _gpio_error(gpio, GPIO_ERROR_ARG, 0,
                       "Invalid GPIO direction (can be in, out, low, high)");

  /* Check if GPIO directory exists */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d", line);
  if (stat(gpio_path, &stat_buf) < 0) {
    /* Write line number to ipspeaker file */
    snprintf(buf, sizeof(buf), "%d", line);
    if ((fd = open("/sys/class/gpio/ipspeaker", O_WRONLY)) < 0)
      return _gpio_error(gpio, GPIO_ERROR_OPEN, errno,
                         "Opening GPIO: opening 'ipspeaker'");
    if (write(fd, buf, strlen(buf) + 1) < 0) {
      int errsv = errno;
      close(fd);
      return _gpio_error(gpio, GPIO_ERROR_OPEN, errsv,
                         "Opening GPIO: writing 'ipspeaker'");
    }
    if (close(fd) < 0)
      return _gpio_error(gpio, GPIO_ERROR_OPEN, errno,
                         "Opening GPIO: closing 'ipspeaker'");

    /* Wait until GPIO directory appears */
    unsigned int retry_count;
    for (retry_count = 0; retry_count < GPIO_SYSFS_ipspeaker_STAT_RETRIES;
         retry_count++) {
      int ret = stat(gpio_path, &stat_buf);
      if (ret == 0)
        break;
      else if (ret < 0 && errno != ENOENT)
        return _gpio_error(gpio, GPIO_ERROR_OPEN, errno,
                           "Opening GPIO: stat 'gpio%d/' after ipspeaker",
                           line);

      usleep(GPIO_SYSFS_ipspeaker_STAT_DELAY);
    }

    if (retry_count == GPIO_SYSFS_ipspeaker_STAT_RETRIES)
      return _gpio_error(gpio, GPIO_ERROR_OPEN, 0,
                         "Opening GPIO: waiting for 'gpio%d/' timed out", line);
  }

  /* Open value */
  snprintf(gpio_path, sizeof(gpio_path), "/sys/class/gpio/gpio%d/value", line);
  if ((fd = open(gpio_path, O_RDWR)) < 0)
    return _gpio_error(gpio, GPIO_ERROR_OPEN, errno,
                       "Opening GPIO 'gpio%d/value'", line);

  memset(gpio, 0, sizeof(gpio_t));
  gpio->line_fd = fd;
  gpio->line = line;
  gpio->ops = &gpio_sysfs_ops;

  ret = gpio_sysfs_set_direction(gpio, direction);
  if (ret < 0)
    return ret;

  return 0;
}