#include <gst/gst.h>
#include <iostream>
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

    std::string video_number = argv[1];
    std::string ip_address = argv[2];
    std::string port = argv[3];
    int frame_rate = std::stoi(argv[4]);
    int frame_width = std::stoi(argv[5]);
    int frame_height = std::stoi(argv[6]);

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    gst_init(&argc, &argv);

    std::string pipeline_string = "v4l2src device=/dev/video" + video_number + " ! video/x-raw,framerate=" + std::to_string(frame_rate) + "/1,width=" + std::to_string(frame_width) + ",height=" + std::to_string(frame_height)
                         + " ! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
                         + "! rtph264pay config-interval=1 ! udpsink host=" + ip_address
                         + " port=" + port + " sync=false";

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_string.c_str(), &error);

    if (!pipeline || error) {
        std::cerr << "[V4L2Streamer] Failed to create pipeline: " << (error ? error->message : "Unknown error") << std::endl;
        return -1;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    std::cout << "[V4L2Streamer] Streaming from /dev/video" << video_number << " at " << frame_rate << " FPS " << "to " << ip_address << ":" << port << std::endl;

    while (keep_running.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    std::cout << "[V4L2Streamer] Stopped streaming." << std::endl;
    return 0;
}
