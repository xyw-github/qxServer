// 非阻塞IO + IO多路复用
// 统一事件源：接收新连接，处理信号时间，处理数据收发
// 子线程处理收到的数据并触发EPOLLOUT信号返回数据
// 信号事件通过管道也交由主线程处理

#include "requesthandle.h"
#include "std.h"
#include "threadpool.h"
#include "locker.h"
#include "util.h"

#define MAX_FD 65536                // 最大文件描述符
#define MAX_EVENT_NUMBER 10000      // 最大事件数

// 这三个函数在util.cpp中，负责对epoll监听树进行操作
extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);
extern int setnonblocking(int fd);

// 管道：用于信号处理
static int pipefd[2];
static int epollfd;

// 信号处理函数 
void sig_handler(int sig)
{
    // 为保证函数的可重入性，需要保存errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 添加要处理的信号
void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset((void *)&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 向客户端发送错误信息
void show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s port_number\n", basename(argv[0]));
        exit(1);
    }

    int port = atoi(argv[1]);
    int listenfd = init_listenfd(port);
    int ret = 0;

    // 忽略SIGPIPE信号
    addsig(SIGPIPE, SIG_IGN);
    // SIGTERM：This is the default signal sent by kill.
    addsig(SIGTERM, sig_handler, false);
    // 控制台CTRL + C信号
    addsig(SIGINT, sig_handler, false);

    sql_connection_pool *sql_conn_pool = sql_connection_pool::GetInstance();
    sql_conn_pool->init("tcp://127.0.0.1:3306", "xyw", "2664226","test",4);

    // 创建线程池
    threadpool<requesthandle> *pool = NULL;
    try
    {
        pool = new threadpool<requesthandle>(sql_conn_pool);
    }
    catch (...)
    {
        return 1;
    }

    // threadpool<requesthandle> *pool = new threadpool<requesthandle>;

    // 预先为每个可能的客户连接分配requesthandle对象
    requesthandle *users = new requesthandle[MAX_FD];
    assert(users);

    
    // 创建内核监听事件表
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    // 添加监听事件到epoll的内核事件表
    addfd(epollfd, listenfd, false);

    // 最多监听的返回的事件
    epoll_event events[MAX_EVENT_NUMBER];

    // requesthandle类内静态变量初始化
    requesthandle::m_epollfd = epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);

    // 写端设置为非阻塞
    setnonblocking(pipefd[1]);

    // 统一事件源，将事件加入epoll监听树
    addfd(epollfd, pipefd[0], false);

    printf("Listen on port %d\n",port);

    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure.\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新的客户连接
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                // 打印客户端信息
                printf("accept a client. fd = %d(%s:%d).\n", connfd, inet_ntoa(client_address.sin_addr), htons(client_address.sin_port));
                if (connfd < 0)
                {
                    printf("accept error.errno is: %d\n", errno);
                    continue;
                }
                if (requesthandle::m_user_count >= MAX_FD)
                {
                    printf("server busy\n");
                    show_error(connfd, "Internal server busy....");
                    continue;
                }
 
                // 初始化客户连接
                // 此处就以connfd的数字来给每个连接分配user
                users[connfd].init(connfd, client_address);
            }
            
            // 处理错误
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                users[sockfd].close_conn();
            }

            // 处理信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                char signals[1024];
                memset(signals, '\0', sizeof(signals));
                ret = recv(pipefd[0], signals, sizeof(signals) - 1, 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                        case SIGINT:
                        {
                            stop_server = true;
                        }
                        case SIGTERM:
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            }
            
            // 处理客户连接上收到的数据
            else if (events[i].events & EPOLLIN)
            {
                // 根据读数据的结果，决定是讲任务添加到线程池，还是关闭连接
                if (users[sockfd].read_once())
                {
                    // 监测到有读事件，则将事件放入请求队列
                    pool->append(users + sockfd);
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            
            // 返回给客户端数据
            else if (events[i].events & EPOLLOUT)
            {
                if (!users[sockfd].write())
                {
                    users[sockfd].close_conn();
                }
                else
                {
                    printf("send data to the client(%s).\n", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    users[sockfd].close_conn();
                    // modfd(epollfd,sockfd,EPOLLIN);
                }
            }
        }
    }

    printf("\nserver close!\n");

    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;

    delete pool;
    return 0;
}