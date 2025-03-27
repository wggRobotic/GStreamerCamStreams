cmake_minimum_required(VERSION 3.10)

project(usbcam)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(OPENCV REQUIRED opencv4)

add_executable(usbcam usbcam.cpp)

target_include_directories(usbcam PRIVATE 
    ${GST_INCLUDE_DIRS}
)

target_link_libraries(usbcam PRIVATE 
    ${GST_LIBRARIES}
)

target_compile_options(usbcam PRIVATE 
    ${GSTREAMER_CFLAGS_OTHER}
)
