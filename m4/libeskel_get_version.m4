# libdsys_get_version.m4
# $Id$
#
# NOTE:
# M4 macro to get version information from VERSION file.
# (Adapted from LAM-MPI implementation.)

AC_DEFUN([LIBESKEL_GET_VERSION],
[
libeskel_ver_dir="$1"
libeskel_ver_file="$2"
libeskel_ver_prog="sh $libeskel_ver_dir/get_libeskel_version $libeskel_ver_file"

libeskel_ver_run() {
  [eval] LIBESKEL_${2}=`$libeskel_ver_prog -${1}`
}

libeskel_ver_run f VERSION
libeskel_ver_run M MAJOR_VERSION
libeskel_ver_run m MINOR_VERSION
libeskel_ver_run r RELEASE_VERSION
libeskel_ver_run a ALPHA_VERSION
libeskel_ver_run b BETA_VERSION

unset libeskel_ver_dir libeskel_ver_file libeskel_ver_prog libeskel_ver_run

# Prevent multiple expansion
define([LIBESKEL_GET_VERSION], [])

])
