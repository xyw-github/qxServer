cmake_minimum_required(VERSION 3.0)

project(QXSERVER)

add_executable(server main.cpp  requesthandle.cpp util.cpp sqlconnection.cpp)

# 定义SRC变量，其值为当前目录下所有的源代码文件
# aux_source_directory(. SRC)
# 编译SRC变量所代表的源代码文件，生成main可执行文件
# add_executable(server ${SRC})

target_link_libraries(server pthread mysqlcppconn)