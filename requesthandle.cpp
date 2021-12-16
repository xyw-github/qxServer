#include "std.h"
#include "requesthandle.h"

// 静态变量初始化
int requesthandle::m_epollfd = -1;
int requesthandle::m_user_count = 0;

void requesthandle::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    // 非阻塞ET工作模式下，需要一次性将数据读完
    addfd(m_epollfd, sockfd, true);
    m_user_count++;

    init();
}

void requesthandle::init()
{
    // 初始化读缓冲区
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    // 初始化写缓冲区
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    // 初始化写缓冲区中待发送的字节数
    m_write_idx = 0;
    // 初始化读到数据的总的字节
    m_read_idx = 0;
}

void requesthandle::close_conn(bool read_close)
{
    printf("close fd: %d\n", m_sockfd);
    if (read_close && m_sockfd != -1)
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

/*循环读取数据，直到无数据可读或对方关闭连接*/
//循环读取客户数据，直到无数据可读或对方关闭连接
bool requesthandle::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            if (errno == EINTR)      // 信号打断了
                continue;
            
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

// 处理读到的数据
int requesthandle::process_read()
{
    // 先拿到数据库连接
    // -- 先查询obtname 对应的obtid
    // -- 根据对应的obtid 和 日期查询 当前的天气  select
    // -- 将查询结果进行计算得到  气温（取最高，最低）
    // -- 计算降雨量（均值） 能见度（均值）
    // -- 相对湿度  大气压（均值）
    // sql::Statement *stmt;
    // connectionRAII(&con,)

    sql::ResultSet *res;
    sql::PreparedStatement *pstmt;

    pstmt = con->prepareStatement("select id from test order by id asc");
    res = pstmt->executeQuery();

    res->afterLast();
    while (res->previous())
    {
        cout << "\t... MySQL counts: " << res->getInt("id") << endl;
    }
    delete res;

    delete pstmt;

    return 1;
}

ssize_t writen(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    char *ptr = (char *)buff;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    nwritten = 0;
                    continue;
                }
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return writeSum;
}

// 向客户端写
bool requesthandle::write()
{
    int ret = writen(m_sockfd, m_write_buf, m_write_idx);
    if (ret == m_write_idx)
        return true;

    return false;
}

// 准备要写的数据
int requesthandle::process_write()
{
    // 调用writen,将要写的数据发回去
    sprintf(m_write_buf, "%s", "nihao");
    m_write_idx = strlen(m_write_buf);

    return 1;
}

// 由线程池中的工作线程调用
// 这是处理客户请求的入口函数

void requesthandle::process()
{
    printf("thread id = %d process the request\n", gettid());
    int read_ret = process_read();
    if (read_ret == -1)
    {
        // 使用此操作直接触发一次EPOLLIN，进而读取数据
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    int write_ret = process_write();
    if (!write_ret)
    {
        close_conn();
    }
    /*直接调用epoll_ctl()重新设置一下event就可以了,
     event跟原来的设置一模一样都行(但必须包含EPOLLOUT)，关键是重新设置，就会马上触发一次EPOLLOUT事件。*/
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}