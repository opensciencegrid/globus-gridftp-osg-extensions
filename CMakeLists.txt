
cmake_minimum_required( VERSION 2.6 )
project( globus-gridftp-osg-extensions )
set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

# These variables get compiled into the final module.
SET(OSG_EXTENSIONS_VERSION_MAJOR 0)
SET(OSG_EXTENSIONS_VERSION_MINOR 1)
include (Timestamp)
TIMESTAMP( OSG_EXTENSIONS_TIMESTAMP )

find_package( GlobusCommon REQUIRED )
find_package( GlobusGridftpServer REQUIRED )

if( CMAKE_COMPILER_IS_GNUCXX )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror" )
endif()

if( CMAKE_COMPILER_IS_GNUCC )
  set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror" )
endif()

SET( CMAKE_MODULE_LINKER_FLAGS "-Wl,--no-undefined")

include_directories( ${GLOBUS_COMMON_INCLUDE_DIRS} )
include_directories( ${GLOBUS_GRIDFTP_SERVER_INCLUDE_DIRS} )

include_directories( "${PROJECT_SOURCE_DIR}" )

add_library( globus_gridftp_server_osg MODULE src/osg_extension_dsi.c )
target_link_libraries( globus_gridftp_server_osg ${GLOBUS_COMMON_LIBRARY} ${GLOBUS_GRIDFTP_SERVER_LIBRARY} )

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
  SET(CMAKE_INSTALL_LIBDIR "lib64")
endif()

install(
  TARGETS globus_gridftp_server_osg
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

set_target_properties(
  globus_gridftp_server_osg
  PROPERTIES
  VERSION   "0.0.1"
  SOVERSION "0"
  LINK_INTERFACE_LIBRARIES "" )

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/version.h)
