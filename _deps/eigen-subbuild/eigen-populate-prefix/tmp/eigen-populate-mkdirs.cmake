# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-src")
  file(MAKE_DIRECTORY "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-src")
endif()
file(MAKE_DIRECTORY
  "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-build"
  "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-subbuild/eigen-populate-prefix"
  "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-subbuild/eigen-populate-prefix/tmp"
  "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-subbuild/eigen-populate-prefix/src/eigen-populate-stamp"
  "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-subbuild/eigen-populate-prefix/src"
  "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-subbuild/eigen-populate-prefix/src/eigen-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-subbuild/eigen-populate-prefix/src/eigen-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jbblet/user/Documents/Projects/FinLib/_deps/eigen-subbuild/eigen-populate-prefix/src/eigen-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
