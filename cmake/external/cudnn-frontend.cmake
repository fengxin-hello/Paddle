# Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include(ExternalProject)

set(CUDNN_FRONTEND_CUDNN_MIN_VERSION 8000)

if(NOT WITH_GPU)
  message(FATAL_ERROR "Can't enable CUDNN Frontend API without CUDA.")
endif()
if(CUDNN_VERSION LESS 8000)
  message(
    FATAL_ERROR
      "Minimum CUDNN version is ${CUDNN_FRONTEND_CUDNN_MIN_VERSION}. Current: ${CUDNN_VERSION}"
  )
endif()

if((NOT DEFINED CUDNN_FRONTEND_NAME) OR (NOT DEFINED CUDNN_FRONTEND_URL))
  set(CUDNN_FRONTEND_VER
      "1.23.2"
      CACHE STRING "" FORCE)
  set(CUDNN_FRONTEND_NAME
      "cudnn-frontend"
      CACHE STRING "" FORCE)
  set(CUDNN_FRONTEND_URL
      "https://github.com/NVIDIA/cudnn-frontend/archive/refs/tags/v0.7.1.tar.gz"
      CACHE STRING "" FORCE)
  set(CUDNN_FRONTEND_CACHE_FILENAME "v0.7.1.tar.gz")
endif()
set(CUDNN_FRONTEND_URL_MD5 "d8f911df571f8b0d40226efa9c0150c8")

message(
  STATUS
    "CUDNN_FRONTEND_NAME: ${CUDNN_FRONTEND_NAME}, CUDNN_FRONTEND_URL: ${CUDNN_FRONTEND_URL}"
)
set(DIRENT_DOWNLOAD_DIR "${PADDLE_SOURCE_DIR}/third_party/cudnn-frontend")
# Version: v0.7.1
set(CUDNN_FRONTEND_PREFIX_DIR ${THIRD_PARTY_PATH}/cudnn-frontend)
set(CUDNN_FRONTEND_SOURCE_DIR
    ${THIRD_PARTY_PATH}/cudnn-frontend/src/extern_cudnn_frontend/include)
set(CUSPARSELT_INSTALL_DIR ${THIRD_PARTY_PATH}/install/cudnn-frontend)
set(CUDNN_FRONTEND_INCLUDE_DIR ${CUDNN_FRONTEND_SOURCE_DIR})
include_directories(${CUDNN_FRONTEND_INCLUDE_DIR})

message(
  STATUS
    "Adding cudnn-frontend. Version: ${CUDNN_FRONTEND_VER}. Directory: ${DIRENT_DOWNLOAD_DIR}"
)

function(download_cudnn_frontend)
  message(
    STATUS
      "Downloading ${CUDNN_FRONTEND_URL} to ${CUDNN_FRONTEND_DOWNLOAD_DIR}/${CUDNN_FRONTEND_CACHE_FILENAME}"
  )
  # NOTE: If the version is updated, consider emptying the folder; maybe add timeout
  file(
    DOWNLOAD ${CUDNN_FRONTEND_URL}
    ${CUDNN_FRONTEND_DOWNLOAD_DIR}/${CUDNN_FRONTEND_CACHE_FILENAME}
    EXPECTED_MD5 ${CUDNN_FRONTEND_URL_MD5}
    STATUS ERR)
  if(ERR EQUAL 0)
    message(STATUS "Download ${CUDNN_FRONTEND_CACHE_FILENAME} success")
  else()
    message(
      FATAL_ERROR
        "Download failed, error: ${ERR}\n You can try downloading ${CUDNN_FRONTEND_CACHE_FILENAME} again"
    )
  endif()
endfunction()

if(EXISTS ${CUDNN_FRONTEND_DOWNLOAD_DIR}/${CUDNN_FRONTEND_CACHE_FILENAME})
  file(MD5 ${CUDNN_FRONTEND_DOWNLOAD_DIR}/${CUDNN_FRONTEND_CACHE_FILENAME}
       CUDNN_FRONTEND_MD5)
  if(NOT CUDNN_FRONTEND_MD5 STREQUAL CUDNN_FRONTEND_URL_MD5)
    # clean build file
    file(REMOVE_RECURSE ${CUDNN_FRONTEND_PREFIX_DIR})
    download_cudnn_frontend()
  endif()
else()
  download_cudnn_frontend()
endif()

ExternalProject_Add(
  extern_cudnn_frontend
  ${EXTERNAL_PROJECT_LOG_ARGS}
  URL ${CUDNN_FRONTEND_DOWNLOAD_DIR}/${CUDNN_FRONTEND_CACHE_FILENAME}
  PREFIX ${CUDNN_FRONTEND_PREFIX_DIR}
  DOWNLOAD_DIR ${CUDNN_FRONTEND_DOWNLOAD_DIR}
  DOWNLOAD_NO_PROGRESS 1
  UPDATE_COMMAND ""
  PATCH_COMMAND
    patch -d ${CUDNN_FRONTEND_SOURCE_DIR} -p2 <
    ${PADDLE_SOURCE_DIR}/patches/cudnn-frontend/0001-patch-for-paddle.patch
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND "")

add_library(cudnn-frontend INTERFACE)
add_dependencies(cudnn-frontend extern_cudnn_frontend)
