# The acl_wl extension
PROJECT("Access Control / White List" C)

# Parser files
# Check if bison and flex are available
IF(BISON_EXECUTABLE AND FLEX_EXECUTABLE)
  BISON_FILE(acl_wl_conf.y)
  FLEX_FILE(acl_wl_conf.l)
  SET_SOURCE_FILES_PROPERTIES(lex.acl_wl_conf.c acl_wl_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")
ELSE()
  # Use pre-generated files
  MESSAGE(STATUS "Using pre-generated parser files in acl_wl")
ENDIF()

# List of source files
SET( ACL_WL_SRC
	acl_wl.h
	acl_wl.c
	aw_tree.c
	lex.aw_conf.c
	aw_conf.tab.c
	aw_conf.tab.h
)

# Compile as a module
FD_ADD_EXTENSION(acl_wl ${ACL_WL_SRC})


####
## INSTALL section ##

# We install with the daemon component because it is a base feature.
INSTALL(TARGETS acl_wl
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-daemon)
