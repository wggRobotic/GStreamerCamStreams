cmake_minimum_required(VERSION 3.10)

add_subdirectory(lib/depthai-core EXCLUDE_FROM_ALL)

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



add_executable(usbcam src/usbcam.cpp)

target_include_directories(usbcam PRIVATE 
    ${GST_INCLUDE_DIRS}
)

target_link_libraries(usbcam PRIVATE 
    ${GST_LIBRARIES}
)

target_compile_options(usbcam PRIVATE 
    ${GSTREAMER_CFLAGS_OTHER}
)



add_executable(oak src/oak.cpp)

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



add_executable(raspicam src/raspicam.cpp)

target_include_directories(raspicam PRIVATE 
    ${GST_INCLUDE_DIRS}
)

target_link_libraries(raspicam PRIVATE 
    ${GST_LIBRARIES}
)

target_compile_options(raspicam PRIVATE 
    ${GSTREAMER_CFLAGS_OTHER}
)