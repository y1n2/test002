cmake_minimum_required(VERSION 3.5)  
project(magic_client C)  
  
# freeDiameter 安装前缀  
set(FD_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/../install" CACHE PATH "freeDiameter install prefix")  
  
# 查找 freeDiameter 头文件目录  
find_path(FD_INCLUDE_DIR  
  NAMES freeDiameter/libfdcore.h freeDiameter/libfdproto.h  
  PATHS  
    "${FD_INSTALL_PREFIX}/include"  
    "${CMAKE_SOURCE_DIR}/../include"  
)  
if (NOT FD_INCLUDE_DIR)  
  message(FATAL_ERROR "未找到 freeDiameter 头文件")  
endif()  
  
# 添加包含路径  
include_directories("${FD_INCLUDE_DIR}")           # freeDiameter 头文件  
include_directories(${CMAKE_CURRENT_SOURCE_DIR})   # 当前目录的头文件  
  
# 查找库文件  
find_library(FDCORE_LIB NAMES fdcore  
  PATHS  
    "${FD_INSTALL_PREFIX}/lib"  
    "${CMAKE_SOURCE_DIR}/../build/libfdcore"  
)  
find_library(FDPROTO_LIB NAMES fdproto  
  PATHS  
    "${FD_INSTALL_PREFIX}/lib"  
    "${CMAKE_SOURCE_DIR}/../build/libfdproto"  
)  
if (NOT FDCORE_LIB OR NOT FDPROTO_LIB)  
  message(FATAL_ERROR "未找到 fdcore/fdproto 库")  
endif()  
  
# 源文件列表
set(MAGIC_CLIENT_SOURCES
    magic_client.c
    config.c
    magic_dict_handles.c
    cli_interface.c
    magic_commands.c
    magic_group_avp_add.c
    session_manager.c
    udp_test.c
    magic_dataplane.c                              # 数据平面路由模块 (ARINC 839 合规)
    ../extensions/dict_magic_839/dict_magic_codes.c  # 错误码字符串转换
)

# 可执行文件  
add_executable(magic_client ${MAGIC_CLIENT_SOURCES})  
  
# 链接库（添加 readline 支持）
target_link_libraries(magic_client 
    "${FDCORE_LIB}" 
    "${FDPROTO_LIB}"
    readline
    pthread
)  
  
# 设置 RPATH  
set_target_properties(magic_client PROPERTIES  
  INSTALL_RPATH "${FD_INSTALL_PREFIX}/lib"  
  BUILD_RPATH "${FDCORE_LIB_DIR};${FDPROTO_LIB_DIR}"  
)