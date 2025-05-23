# Copyright 2017 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##

# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

add_subdirectory(
       ${CMAKE_CURRENT_SOURCE_DIR}/../../backend/pose_graph
       ${CMAKE_BINARY_DIR}/../../../backend/pose_graph/build
)

# Import the ARCore (Google Play Services for AR) library.
add_library(arcore SHARED IMPORTED)
set_target_properties(arcore PROPERTIES IMPORTED_LOCATION
                      ${ARCORE_LIBPATH}/${ANDROID_ABI}/libarcore_sdk_c.so
                      INTERFACE_INCLUDE_DIRECTORIES ${ARCORE_INCLUDE}
)
# Import the glm header file from the NDK.
add_library( glm INTERFACE )
set_target_properties( glm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${GLM_INCLUDE})

# This is the main app library.
add_library(hello_ar_native SHARED
            src/main/cpp/hello_ar_application.cc

            src/main/cpp/renderer/texture.cc
            src/main/cpp/renderer/obj_renderer.cc
            src/main/cpp/renderer/plane_renderer.cc
            src/main/cpp/renderer/background_renderer.cc

            src/main/cpp/path_navigator.cc
            src/main/cpp/direction_helper.cc
            src/main/cpp/astar_pathfinding.cc
            
            src/main/cpp/util.cc
            
            src/main/cpp/jni_interface.cc
            src/main/cpp/java_bridge.cc)

set(POSE_GRAPH_INCLUDE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../backend/pose_graph/src/include)

target_include_directories(hello_ar_native PRIVATE
           ${POSE_GRAPH_INCLUDE_SRC}
           src/main/cpp
           src/main/cpp/include
           src/main/cpp/include/renderer)
           
target_link_libraries(hello_ar_native
                      pose_graph
                      android
                      log
                      GLESv2
                      glm
                      arcore)
