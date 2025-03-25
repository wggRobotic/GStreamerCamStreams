cmake_minimum_required(VERSION 3.10)
project(GStreamerCamStreams)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(OPENCV REQUIRED opencv4)

find_package(realsense2 REQUIRED)

add_executable(realsense src/realsense.cpp)

target_include_directories(realsense PRIVATE 
    ${GST_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
    ${OPENCV_INCLUDE_DIRS}
    ${realsense2_INCLUDE_DIRS}
)

target_link_libraries(realsense PRIVATE 
    ${GST_LIBRARIES}
    ${GST_APP_LIBRARIES}
    ${OPENCV_LIBRARIES}
    realsense2
)

target_compile_options(realsense PRIVATE 
    ${GSTREAMER_CFLAGS_OTHER}
    ${OPENCV_CFLAGS_OTHER}
)
