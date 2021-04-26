#include "common.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

char *genRandomString(int length) {
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

int get_local_ip(char *ip_list) {
  struct ifaddrs *ifAddrStruct;
  void *tmpAddrPtr;
  char ip[INET_ADDRSTRLEN];
  int n = 0;
  getifaddrs(&ifAddrStruct);
  while (ifAddrStruct != NULL && n != 1) {
    if (ifAddrStruct->ifa_addr->sa_family == AF_INET) {
      tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
      inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
      if (strcmp(ip, "127.0.0.1") != 0) {
        if (n == 0) {
          memcpy(ip_list, ip, INET_ADDRSTRLEN);
        } else {
          memcpy(ip_list + INET_ADDRSTRLEN, ip, INET_ADDRSTRLEN);
        }
        n++;
      }
    }
    ifAddrStruct = ifAddrStruct->ifa_next;
  }
  // free ifaddrs
  // freeifaddrs(ifAddrStruct);
  return n;
}