#ifndef _SQL_CONNECTION_H
#define _SQL_CONNECTION_H

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>


#include <string>
#include <mutex>
#include <list>
#include <pthread.h>
#include <semaphore.h>

using namespace std;


class sql_connection_pool
{

public:
    sql::Connection *GetConnection();               // 获取一条连接
    bool ReleaseConnection(sql::Connection *conn);  // 释放这个连接，即将连接加入连接队列
    int  GetFreeConn();                             // 获取空闲连接个数
    void DestoryPool();                             // 销毁连接池
    
    
    static sql_connection_pool *GetInstance();      // 单例模式，确保只有这一个连接池

    void init(string url,string m_user,string password,string dbname,unsigned int maxconn);
    
    sql_connection_pool();       // 连接池的初始化 初始化多条连接 放入连接队列
    ~sql_connection_pool();
    // getConnection();          // 获取一条空闲的连接

private:
    unsigned int m_maxconn;      // 总的连接数量
    unsigned int m_curconn;      // 当前已使用连接数量
    unsigned int m_freeconn;     // 当前空闲连接数量

private:
    std::mutex m_mutex;                        // 使用C++11 mutex
    std::list<sql::Connection *> m_connlist;   // 连接池链表
    // std::list<sql::Driver *> m_driver;         // 
    sem_t m_sem;                               // 使用POSIX信号量

private:
    string m_url;          // 主机地址:主机端口
    string m_user;         // 数据库用户名
    string m_password;     // 登录密码
    string m_dbname;       // 数据库库名
};

class connectionRAII
{
public:
    connectionRAII(sql::Connection **con,sql_connection_pool *pool);
    ~connectionRAII();

private:
    sql::Connection *connRAII;
    sql_connection_pool *poolRAII;
};



#endif