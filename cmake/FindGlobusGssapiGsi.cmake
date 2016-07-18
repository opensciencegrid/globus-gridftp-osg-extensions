
# - Try to find GLOBUS_GSSAPI_GSI
# Once done this will define
#  GLOBUS_GSSAPI_GSI_FOUND - System has globus_gssapi_gsi
#  GLOBUS_GSSAPI_GSI_INCLUDE_DIRS - The globus_gssapi_gsi include directories
#  GLOBUS_GSSAPI_GSI_LIBRARIES - The libraries needed to use globus_gssapi_gsi
#  GLOBUS_GSSAPI_GSI_DEFINITIONS - Compiler switches required for using globus_gssapi_gsi

find_package(PkgConfig)
pkg_check_modules(PC_GLOBUS_GSSAPI_GSI QUIET globus-gssapi-gsi)
set(GLOBUS_GSSAPI_GSI_DEFINITIONS ${PC_GLOBUS_GSSAPI_GSI_CFLAGS_OTHER})

find_path(GLOBUS_GSSAPI_GSI_INCLUDE_DIR gssapi.h
          HINTS ${PC_GLOBUS_GSSAPI_GSI_INCLUDEDIR} ${PC_GLOBUS_GSSAPI_GSI_INCLUDE_DIRS}
          PATH_SUFFIXES globus )

find_library(GLOBUS_GSSAPI_GSI_LIBRARY NAMES globus_gssapi_gsi
             HINTS ${PC_GLOBUS_GSSAPI_GSI_LIBDIR} ${PC_GLOBUS_GSSAPI_GSI_LIBRARY_DIRS} )

set(GLOBUS_GSSAPI_GSI_LIBRARIES ${GLOBUS_GSSAPI_GSI_LIBRARY} )
set(GLOBUS_GSSAPI_GSI_INCLUDE_DIRS ${GLOBUS_GSSAPI_GSI_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set GLOBUS_GSSAPI_GSI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GLOBUS_GSSAPI_GSI DEFAULT_MSG
                                  GLOBUS_GSSAPI_GSI_LIBRARY GLOBUS_GSSAPI_GSI_INCLUDE_DIR)

mark_as_advanced( GLOBUS_GSSAPI_GSI_FOUND GLOBUS_GSSAPI_GSI_INCLUDE_DIR GLOBUS_GSSAPI_GSI_LIBRARY )

