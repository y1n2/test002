# Magic Server 扩展的 CMake 构建文件
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# 设置扩展名称
SET(EXTENSION magic_server)

# 查找libconfig库
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(LIBCONFIG REQUIRED libconfig)

# 包含目录
INCLUDE_DIRECTORIES(
    ${LIBCONFIG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 源文件
SET(SOURCES
    src/magic_server.c
    src/cic/magic_cic.c
    src/cm/magic_cm.c
    src/dlm/magic_dlm.c
    src/netmgmt/magic_netmgmt.c
    src/utils/magic_config.c
)

# 构建共享库
FD_ADD_EXTENSION(${EXTENSION} ${SOURCES})

# 设置编译选项，忽略警告
SET_TARGET_PROPERTIES(${EXTENSION} PROPERTIES
    COMPILE_FLAGS "-Wno-return-type -Wno-unused-function"
)

# 链接库
TARGET_LINK_LIBRARIES(${EXTENSION} ${LIBCONFIG_LIBRARIES} pthread)

# 安装配置文件
INSTALL(FILES config/magic_server.conf DESTINATION ${DEFAULT_CONF_PATH})