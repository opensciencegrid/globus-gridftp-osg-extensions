
# - Try to find VOMS
# Once done this will define
#  VOMS_FOUND - System has VOMS
#  VOMS_INCLUDE_DIRS - The voms include directories
#  VOMS_LIBRARIES - The libraries needed to use voms
#  VOMS_DEFINITIONS - Compiler switches required for using voms

find_package(PkgConfig)
pkg_check_modules(PC_VOMS QUIET voms-2.0)
set(VOMS_DEFINITIONS ${PC_VOMS_CFLAGS_OTHER})

find_path(VOMS_INCLUDE_DIR voms_apic.h
          HINTS ${PC_VOMS_INCLUDEDIR} ${PC_VOMS_INCLUDE_DIRS}
          PATH_SUFFIXES voms )

find_library(VOMS_LIBRARY NAMES vomsapi
             HINTS ${PC_VOMS_LIBDIR} ${PC_VOMS_LIBRARY_DIRS} )

set(VOMS_LIBRARIES ${VOMS_LIBRARY} )
set(VOMS_INCLUDE_DIRS ${VOMS_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set VOMS_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(VOMS DEFAULT_MSG
                                  VOMS_LIBRARY VOMS_INCLUDE_DIR)

mark_as_advanced( VOMS_FOUND VOMS_INCLUDE_DIR VOMS_LIBRARY )

