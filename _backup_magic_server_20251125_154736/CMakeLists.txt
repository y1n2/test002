cmake_minimum_required(VERSION 3.10)
project(MAGIC_Server C)

# 设置 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
add_compile_options(-Wall -Wextra -g)

# 查找依赖库
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
find_package(Threads REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBXML2_INCLUDE_DIRS}
)

# 链接目录
link_directories(
    ${LIBXML2_LIBRARY_DIRS}
)

#==============================================================================
# 1. CM Core Simple (简单的 CM Core 服务器)
#==============================================================================
add_executable(cm_core_simple
    cm_core_simple.c
    dlm_common.c
)

target_link_libraries(cm_core_simple
    Threads::Threads
    m
)

#==============================================================================
# 2. MAGIC Core Main (主 MAGIC Core 服务器)
#==============================================================================
add_executable(magic_core_main
    magic_core_main.c
    xml_config_parser.c
    policy_engine.c
    dlm_common.c
)

target_link_libraries(magic_core_main
    ${LIBXML2_LIBRARIES}
    Threads::Threads
    m
)

#==============================================================================
# 3. CM Core Server (完整的 CM Core 服务器 - 需要 LMI 支持)
#==============================================================================
# 注意: cm_core_server 使用 lmi/ 目录中的协议定义，暂时禁用
# 如需启用，请确保包含 lmi/ 目录并链接所需的源文件
#
# if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cm_core/cm_core_server.c")
#     add_executable(cm_core_server
#         cm_core/cm_core_server.c
#         lmi/magic_ipc_utils.c
#         lmi/magic_lmi_utils.c
#         dlm_common.c
#     )
#
#     target_include_directories(cm_core_server PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/lmi
#     )
#
#     target_link_libraries(cm_core_server
#         Threads::Threads
#         m
#     )
# endif()

#==============================================================================
# 4. 测试程序
#==============================================================================

# XML 解析器测试
add_executable(test_xml_parser
    test_xml_parser.c
    xml_config_parser.c
)

target_link_libraries(test_xml_parser
    ${LIBXML2_LIBRARIES}
    m
)

# 策略引擎测试
add_executable(test_policy_engine
    test_policy_engine.c
    policy_engine.c
    xml_config_parser.c
    dlm_common.c
)

target_link_libraries(test_policy_engine
    ${LIBXML2_LIBRARIES}
    Threads::Threads
    m
)

#==============================================================================
# 安装规则
#==============================================================================
install(TARGETS cm_core_simple magic_core_main
    RUNTIME DESTINATION bin
)

# 安装配置文件
install(DIRECTORY config/
    DESTINATION etc/magic_server
    FILES_MATCHING PATTERN "*.xml"
)

#==============================================================================
# 显示配置信息
#==============================================================================
message(STATUS "")
message(STATUS "======================================")
message(STATUS "  MAGIC Server Build Configuration")
message(STATUS "======================================")
message(STATUS "  Project:        ${PROJECT_NAME}")
message(STATUS "  C Compiler:     ${CMAKE_C_COMPILER}")
message(STATUS "  C Standard:     ${CMAKE_C_STANDARD}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  libxml2:        ${LIBXML2_VERSION}")
message(STATUS "  Threads:        Found")
message(STATUS "")
message(STATUS "Build Targets:")
message(STATUS "  - cm_core_simple     (CM Core 简单服务器)")
message(STATUS "  - magic_core_main    (MAGIC Core 主服务器)")
message(STATUS "  - test_xml_parser    (XML 解析器测试)")
message(STATUS "  - test_policy_engine (策略引擎测试)")
message(STATUS "======================================")
message(STATUS "")
