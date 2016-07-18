
# - Try to find GLOBUS_FTP_CONTROL
# Once done this will define
#  GLOBUS_FTP_CONTROL_FOUND - System has globus_ftp_control
#  GLOBUS_FTP_CONTROL_INCLUDE_DIRS - The globus_ftp_control include directories
#  GLOBUS_FTP_CONTROL_LIBRARIES - The libraries needed to use globus_ftp_control
#  GLOBUS_FTP_CONTROL_DEFINITIONS - Compiler switches required for using globus_ftp_control

find_package(PkgConfig)
pkg_check_modules(PC_GLOBUS_FTP_CONTROL QUIET globus-ftp-control)
set(GLOBUS_FTP_CONTROL_DEFINITIONS ${PC_GLOBUS_FTP_CONTROL_CFLAGS_OTHER})

find_path(GLOBUS_FTP_CONTROL_INCLUDE_DIR globus_ftp_control.h
          HINTS ${PC_GLOBUS_FTP_CONTROL_INCLUDEDIR} ${PC_GLOBUS_FTP_CONTROL_INCLUDE_DIRS}
          PATH_SUFFIXES globus )

find_library(GLOBUS_FTP_CONTROL_LIBRARY NAMES globus_ftp_control
             HINTS ${PC_GLOBUS_FTP_CONTROL_LIBDIR} ${PC_GLOBUS_FTP_CONTROL_LIBRARY_DIRS} )

set(GLOBUS_FTP_CONTROL_LIBRARIES ${GLOBUS_FTP_CONTROL_LIBRARY} )
set(GLOBUS_FTP_CONTROL_INCLUDE_DIRS ${GLOBUS_FTP_CONTROL_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set GLOBUS_FTP_CONTROL_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GLOBUS_FTP_CONTROL DEFAULT_MSG
                                  GLOBUS_FTP_CONTROL_LIBRARY GLOBUS_FTP_CONTROL_INCLUDE_DIR)

mark_as_advanced( GLOBUS_FTP_CONTROL_FOUND GLOBUS_FTP_CONTROL_INCLUDE_DIR GLOBUS_FTP_CONTROL_LIBRARY )

