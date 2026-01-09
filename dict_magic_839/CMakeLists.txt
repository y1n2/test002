# Dictionary extension for MAGIC (ARINC-839)
PROJECT("Dictionary extension for MAGIC (ARINC-839)" C)

# Build the extension as a module
FD_ADD_EXTENSION(dict_magic_839  dict_magic.c add_avps.c dict_magic_codes.c)
INSTALL(TARGETS dict_magic_839 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/freeDiameter)

# Install header file for application usage
INSTALL(FILES dict_magic_codes.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/freeDiameter)