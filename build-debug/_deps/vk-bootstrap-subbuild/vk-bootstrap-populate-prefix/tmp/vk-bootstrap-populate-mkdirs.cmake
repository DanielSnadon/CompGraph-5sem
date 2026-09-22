# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-src")
  file(MAKE_DIRECTORY "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-src")
endif()
file(MAKE_DIRECTORY
  "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-build"
  "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix"
  "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/tmp"
  "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp"
  "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src"
  "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/snadon/ComputerGraphics-5sem/build-debug/_deps/vk-bootstrap-subbuild/vk-bootstrap-populate-prefix/src/vk-bootstrap-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
