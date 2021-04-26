#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

char *genRandomString(int length);
int get_local_ip(char *ip_list);

#ifdef __cplusplus
}
#endif

#endif