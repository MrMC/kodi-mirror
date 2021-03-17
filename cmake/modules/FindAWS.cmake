#.rst:
# FindAWS
# --------
# Finds the aws libraries
#
# This will define the following variables::
#
# AWS_FOUND - system has aws
# AWS_INCLUDE_DIRS - the aws include directory
# AWS_LIBRARIES - the aws libraries
# AWS_DEFINITIONS - the aws definitions

find_path(AWS_INCLUDE_DIR NAMES aws)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  find_library(AWS_0_LIBRARY NAMES s2n libs2n PATH_SUFFIXES aws)
endif()

find_library(AWS_1_LIBRARY NAMES IotIdentity-cpp libIotIdentity-cpp PATH_SUFFIXES aws)
find_library(AWS_2_LIBRARY NAMES Discovery-cpp libDiscovery-cpp PATH_SUFFIXES aws)
find_library(AWS_3_LIBRARY NAMES IotShadow-cpp libIotShadow-cpp PATH_SUFFIXES aws)
find_library(AWS_4_LIBRARY NAMES IotJobs-cpp libIotJobs-cpp PATH_SUFFIXES aws)
find_library(AWS_5_LIBRARY NAMES aws-c-event-stream libaws-c-event-stream PATH_SUFFIXES aws)
find_library(AWS_6_LIBRARY NAMES aws-checksums libaws-checksums PATH_SUFFIXES aws)
find_library(AWS_7_LIBRARY NAMES aws-c-mqtt libaws-c-mqtt PATH_SUFFIXES aws)
find_library(AWS_8_LIBRARY NAMES aws-c-auth libaws-c-auth PATH_SUFFIXES aws)
find_library(AWS_9_LIBRARY NAMES aws-c-http libaws-c-http PATH_SUFFIXES aws)
find_library(AWS_10_LIBRARY NAMES aws-c-compression libaws-c-compression PATH_SUFFIXES aws)
find_library(AWS_11_LIBRARY NAMES aws-c-cal libaws-c-cal PATH_SUFFIXES aws)
find_library(AWS_12_LIBRARY NAMES aws-c-io libaws-c-io PATH_SUFFIXES aws)
find_library(AWS_13_LIBRARY NAMES aws-c-common libaws-c-common PATH_SUFFIXES aws)
find_library(AWS_14_LIBRARY NAMES aws-crt-cpp libaws-crt-cpp PATH_SUFFIXES aws)

set(AWS_VERSION 1.0.0)

#if(ENABLE_INTERNAL_UDFREAD)
# todo later
#endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(AWS
                                  REQUIRED_VARS AWS_INCLUDE_DIR
                                  AWS_1_LIBRARY
                                  AWS_2_LIBRARY
                                  AWS_3_LIBRARY
                                  AWS_4_LIBRARY
                                  AWS_5_LIBRARY
                                  AWS_6_LIBRARY
                                  AWS_7_LIBRARY
                                  AWS_8_LIBRARY
                                  AWS_9_LIBRARY
                                  AWS_10_LIBRARY
                                  AWS_11_LIBRARY
                                  AWS_12_LIBRARY
                                  AWS_13_LIBRARY
                                  AWS_14_LIBRARY
                                  VERSION_VAR AWS_VERSION)

if(AWS_FOUND)
  set(AWS_LIBRARIES ${AWS_1_LIBRARY} ${AWS_2_LIBRARY}
                    ${AWS_3_LIBRARY} ${AWS_4_LIBRARY}
                    ${AWS_5_LIBRARY} ${AWS_6_LIBRARY}
                    ${AWS_7_LIBRARY} ${AWS_8_LIBRARY}
                    ${AWS_9_LIBRARY} ${AWS_10_LIBRARY}
                    ${AWS_11_LIBRARY} ${AWS_12_LIBRARY}
                    ${AWS_13_LIBRARY} ${AWS_14_LIBRARY})
  if(CMAKE_SYSTEM_NAME MATCHES "Linux")                  
    list(APPEND AWS_LIBRARIES ${AWS_0_LIBRARY})
  endif()             
  set(AWS_INCLUDE_DIRS ${AWS_INCLUDE_DIR})
  set(AWS_DEFINITIONS -DHAS_AWS=1)
endif()

mark_as_advanced(AWS_INCLUDE_DIRS AWS_LIBRARIES)