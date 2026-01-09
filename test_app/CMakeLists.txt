# The test_app extension
PROJECT("Test Diameter Application" C)

# Parser files
# Check if bison and flex are available
IF(BISON_EXECUTABLE AND FLEX_EXECUTABLE)
  BISON_FILE(test_app_conf.y)
  FLEX_FILE(test_app_conf.l)
  SET_SOURCE_FILES_PROPERTIES(lex.test_app_conf.c test_app_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")
ELSE()
  # Use pre-generated files
  MESSAGE(STATUS "Using pre-generated parser files in test_app")
ENDIF()

# List of source files
SET( APP_TEST_SRC
	test_app.h
	test_app.c
	lex.ta_conf.c
	ta_conf.tab.c
	ta_conf.tab.h
	ta_dict.c
	ta_cli.c
	ta_bench.c
	ta_serv.c
)

# Compile as a module
FD_ADD_EXTENSION(test_app ${APP_TEST_SRC})


####
## INSTALL section ##

INSTALL(TARGETS test_app
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)
