#include "sqlconnection.h"

sql_connection_pool *sql_connection_pool::GetInstance()
{
    static sql_connection_pool connPool;
    return &connPool;
}

sql_connection_pool::sql_connection_pool()
{
    this->m_curconn = 0;
    this->m_freeconn = 0;
}

sql_connection_pool::~sql_connection_pool()
{
    DestoryPool();
}

void sql_connection_pool::init(string url, string user, string password, string dbname, unsigned int maxconn)
{
    this->m_url = url;
    this->m_user = user;
    this->m_password = password;
    this->m_dbname = dbname;

    sql::Driver *driver;
    driver = get_driver_instance();

    std::lock_guard<std::mutex> guard(m_mutex);
    for (int i = 0; i < maxconn; i++)
    {
        try
        {
            // m_driver.push_back(driver);
            sql::Connection *con = driver->connect(url, user, password);
            con->setSchema(dbname);
            m_connlist.push_back(con);
            ++m_freeconn;
            cout << "create a sql connection\n";
        }
        catch (sql::SQLException &e)
        {
            cout << "# ERR: SQLException in " << __FILE__;
            cout << "(" << __FUNCTION__ << ") on line "
                 << __LINE__ << endl;
            cout << "# ERR: " << e.what();
            cout << " (MySQL error code: " << e.getErrorCode();
            cout << ", SQLState: " << e.getSQLState() << " )" << endl;
        }
    }

    // 对信号量初始化
    sem_init(&m_sem, 0, m_freeconn);
    this->m_maxconn = m_freeconn;
}

sql::Connection *sql_connection_pool::GetConnection()
{
    sql::Connection *con = nullptr;

    if (m_connlist.size() == 0)
        return nullptr;

    sem_wait(&m_sem);
    std::lock_guard<std::mutex> guard(m_mutex);

    con = m_connlist.front();
    m_connlist.pop_front();

    m_freeconn--;
    ++m_curconn;

    return con;
}

bool sql_connection_pool::ReleaseConnection(sql::Connection *conn)
{
    if (conn == nullptr)
        return false;
    std::lock_guard<std::mutex> guard(m_mutex);
    m_connlist.push_back(conn);
    ++m_freeconn;
    --m_curconn;

    sem_post(&m_sem);
    return true;
}

int sql_connection_pool::GetFreeConn()
{
    return this->m_freeconn;
}

void sql_connection_pool::DestoryPool()
{
    cout << "call DestoryPool()" << endl;
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_connlist.size() != 0)
    {
        for (auto &conn : m_connlist)
        {
            conn->close();
            cout << "close 1 sql_conn" << endl;
            delete conn;
        }
        m_connlist.clear();
    }
}

connectionRAII::connectionRAII(sql::Connection **con, sql_connection_pool *pool)
{
    *con = pool->GetConnection();
    cout << "call connectionRAII\n";
    connRAII = *con;
    poolRAII = pool;
}

connectionRAII::~connectionRAII()
{
    cout << "call ~connectionRAII" << endl;
    poolRAII->ReleaseConnection(connRAII);
}