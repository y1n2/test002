# ADIF Simulator - Aircraft Data Interface Function Simulator
# Based on ARINC 834-1 ADBP Protocol
#
# 这是一个独立的模拟器程序，用于模拟飞机数据接口功能
# 为 MAGIC 系统提供飞机状态数据
#
# 独立编译方式:
#   cd adif_simulator && mkdir build && cd build
#   cmake .. && make
#
# 作为 freeDiameter 子项目编译:
#   在 freeDiameter/build 目录下 cmake .. && make adif_simulator

CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT("ADIF Simulator" C)

# 查找 readline 库（用于 CLI 界面）
FIND_PATH(READLINE_INCLUDE_DIR readline/readline.h
    /usr/include
    /usr/local/include
)

FIND_LIBRARY(READLINE_LIBRARY readline
    /usr/lib
    /usr/local/lib
    /usr/lib/x86_64-linux-gnu
)

IF(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARY)
    MESSAGE(STATUS "readline library not found, ADIF simulator will not be built")
    RETURN()
ENDIF()

MESSAGE(STATUS "Found readline: ${READLINE_LIBRARY}")

# 添加可执行文件
ADD_EXECUTABLE(adif_simulator adif_simulator.c)

# 设置包含目录
TARGET_INCLUDE_DIRECTORIES(adif_simulator PRIVATE
    ${READLINE_INCLUDE_DIR}
)

# 链接库
TARGET_LINK_LIBRARIES(adif_simulator
    ${READLINE_LIBRARY}
    pthread
)

# 安装目标
INSTALL(TARGETS adif_simulator
    RUNTIME DESTINATION bin
)
