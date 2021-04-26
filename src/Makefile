Target = smartspeaker
LOCAL_CFLAGS += -O0 -g
LOCAL_CFLAGS += -I. -I..
LOCAL_LDFLAGS += -L. -lauth -lduilite -ldds -lasound -lpthread -ldl -lrt -lmosquitto
LOCAL_LDFLAGS += -Wl,-rpath,.
INCLUDES +=	-I$(TOPDIR)/lichee/linux-3.4/include \
			-I$(TOPDIR)/lichee/linux-3.4/arch/arm/mach-sunxi/include \
			-I$(TOPDIR)/lichee/linux-3.4/arch/arm/include \
			-I$(TOPDIR)/lichee/linux-3.4/arch/sh/include

$(Target):main.c auth.c base64.c cJSON.c connect.c common.c ddsasr.c minlni.c ipspeaker.c gpioin.c export.c mqtt.c play.c queue.c record.c renti.c vad.c wakeup.c
	$(CC) -o $@ $^ $(LOCAL_CFLAGS) $(INCLUDES) $(LOCAL_LDFLAGS)