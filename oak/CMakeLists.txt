cmake_minimum_required(VERSION 3.10)

add_subdirectory(lib/depthai-core EXCLUDE_FROM_ALL)

project(oak)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(OPENCV REQUIRED opencv4)

add_executable(oak oak.cpp)

target_include_directories(oak PRIVATE 
    ${GST_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
    ${OPENCV_INCLUDE_DIRS}
)

target_link_libraries(oak PRIVATE
    ${GST_LIBRARIES}
    ${GST_APP_LIBRARIES}
    ${OPENCV_LIBRARIES}
    depthai::opencv
)

target_compile_options(oak PRIVATE 
    ${GSTREAMER_CFLAGS_OTHER}
    ${OPENCV_CFLAGS_OTHER}
)