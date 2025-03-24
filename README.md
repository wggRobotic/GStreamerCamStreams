# GStreamerRealsense
GStreamer color and depth stream for Intel Realsense D435 cam

Build: g++ stream.cpp -o stream $(pkg-config --cflags --libs gstreamer-1.0 opencv4) -lrealsense2

Usage: ./stream <IP_ADDRESS> <COLOR_PORT> <DEPTH_PORT> <FPS> <WIDTH> <HEIGHT> <DEPTH_SCALE>
