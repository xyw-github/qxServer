#include "util.h"

int setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd,bool one_shot)
{
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET  | EPOLLRDHUP;
    ev.data.fd = fd;
    if(one_shot)
    {
        ev.events |=  EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
    setnonblocking(fd);
}

void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

void modfd(int epollfd,int fd,int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLIN | EPOLLET  | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

int init_listenfd(const int port)
{
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    int ret = bind(listenfd,(struct sockaddr *)&address,sizeof(address));
    assert(ret >= 0);
    
    ret = listen(listenfd,5);
    assert(ret >= 0);

    return listenfd;
}