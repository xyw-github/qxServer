#ifndef _UTIL_H
#define _UTIL_H
#include "std.h"

void addfd(int epollfd,int fd,bool one_shot);
void removefd(int epollfd,int fd);
int setnonblocking(int fd);
void modfd(int epollfd,int fd,int ev);


int init_listenfd(int port);


#endif