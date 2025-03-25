#include <gst/gst.h>
#include <iostream>
#include <sstream>
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> keep_running{true};

void handle_signal(int signum) {
    keep_running.store(false);
    std::cout << "[V4L2Streamer] Received signal " << signum << ". Stopping..." << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        std::cerr << "[V4L2Streamer] Usage: " << argv[0] << " <VIDEO_NUM> <IP_ADDRESS> <PORT> <FPS> <WIDTH> <HEIGHT>" << std::endl;
        return -1;
    }

    // Read command-line arguments
    int video_number = std::stoi(argv[1]);
    std::string ip_address = argv[2];
    int udp_port = std::stoi(argv[3]);
    int frame_rate = std::stoi(argv[4]);
    int frame_width = std::stoi(argv[5]);
    int frame_height = std::stoi(argv[6]);

    // Construct the GStreamer pipeline string
    std::ostringstream pipeline_description;
    pipeline_description << "v4l2src device=/dev/video" << video_number
                         << " ! video/x-raw,framerate=" << frame_rate << "/1,width=" << frame_width << ",height=" << frame_height
                         << " ! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
                         << "! rtph264pay config-interval=1 ! udpsink host=" << ip_address
                         << " port=" << udp_port << " sync=false";

    std::string pipeline_str = pipeline_description.str();

    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create the pipeline
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_str.c_str(), &error);

    if (!pipeline || error) {
        std::cerr << "[V4L2Streamer] Failed to create pipeline: " << (error ? error->message : "Unknown error") << std::endl;
        return -1;
    }

    // Handle termination signals
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Start the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    std::cout << "[V4L2Streamer] Streaming from /dev/video" << video_number << " at " << frame_rate << " FPS "
              << "to " << ip_address << ":" << udp_port << std::endl;

    // Keep running until stopped
    while (keep_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop and clean up
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    std::cout << "[V4L2Streamer] Stopped streaming." << std::endl;
    return 0;
}
