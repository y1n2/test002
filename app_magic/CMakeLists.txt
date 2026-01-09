# CMakeLists.txt for app_magic extension

PROJECT("MAGIC Unified Extension" C)

# 查找依赖库
FIND_PACKAGE(LibXml2 REQUIRED)
FIND_PACKAGE(Threads REQUIRED)

# 查找 libnetfilter_conntrack (用于流量监控)
FIND_LIBRARY(NFCONNTRACK_LIBRARY NAMES netfilter_conntrack)
IF(NOT NFCONNTRACK_LIBRARY)
    MESSAGE(WARNING "libnetfilter_conntrack not found, traffic monitoring will be disabled")
    SET(NFCONNTRACK_LIBRARY "")
ENDIF()

# 设置源文件
SET(APP_MAGIC_SRC
    app_magic.c
    magic_config.c
    magic_policy.c
    magic_session.c
    magic_lmi.c
    magic_cic.c
    magic_cic_push.c
    magic_dataplane.c
    magic_adif.c
    magic_dict_handles.c
    magic_group_avp_simple.c
    magic_tft_validator_3gpp.c
    magic_napt_validator.c
    mih_transport.c
    magic_traffic_monitor.c
    magic_cdr.c
)

# 包含目录
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBXML2_INCLUDE_DIR}
)

# 创建扩展库
FD_ADD_EXTENSION(app_magic ${APP_MAGIC_SRC})

# 链接库
TARGET_LINK_LIBRARIES(app_magic
    ${LIBXML2_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${NFCONNTRACK_LIBRARY}
)

# 安装配置文件
INSTALL(
    DIRECTORY config/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/freeDiameter/app_magic
    FILES_MATCHING PATTERN "*.xml"
)

# 编译选项
TARGET_COMPILE_OPTIONS(app_magic PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

MESSAGE(STATUS "app_magic extension configured")
MESSAGE(STATUS "  Source files: ${APP_MAGIC_SRC}")
MESSAGE(STATUS "  LibXML2: ${LIBXML2_LIBRARIES}")
