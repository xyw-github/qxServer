#ifndef _REQUESTHANDLER_H
#define _REQUESTHANDLER_H

#include "std.h"
#include "locker.h"
#include "util.h"
#include "sqlconnection.h"


class requesthandle
{
public:
    // 读缓冲区大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;
    
public:
    // 初始化新接受的连接
    void init(int sockfd,const sockaddr_in& addr);
    // 关闭连接
    void close_conn(bool read_close = true);
    // 处理客户请求
    void process();
    // 非阻塞读操作
    bool read_once();
    // 非阻塞写操作
    bool write();
    // 返回客户端地址
    sockaddr_in *get_address()
    {
        return &m_address;
    }
private:
    // 初始化连接
    void init();
    // 解析请求报文
    int process_read();
    // 填充应答
    int process_write();

public:    
    // 所有的socket上的事件注册到同一个epoll内核事件表
    static int m_epollfd;
    // 统计用户数量
    static int m_user_count;
private:
    // 该连接的socket和对方的socket地址
    int m_sockfd;
    sockaddr_in m_address;
    // 读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    // 写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    // 写缓冲区中待发送的字节数
    int m_write_idx;
    // 读到数据的总的字节
    int m_read_idx;
    
public:
    sql::Connection *con;
};


#endif