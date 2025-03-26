#include <iostream>
#include <depthai/depthai.hpp>
#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <atomic>
#include <csignal>
#include <string>

std::atomic<bool> keep_running{true};

void handle_signal(int signum) {
    keep_running.store(false);
    std::cout << "[OAK] Received signal " << signum << ". Stopping..." << std::endl;
}

int main(int argc, char *argv[]){

    if (argc != 6) {
        std::cerr << "[OAK] Usage: " << argv[0] << " <IP_ADDRESS> <PORT> <FPS> <WIDTH> <HEIGHT>" << std::endl;
        return -1;
    }

    std::string ip_address = argv[1];
    std::string udp_port = argv[2];
    int fps = std::stoi(argv[3]);
    int width = std::stoi(argv[4]);
    int height = std::stoi(argv[5]);

    std::string pipeline_desc = 
        "appsrc name=oak_src format=time "
        "caps=video/x-raw,format=RGB,width=" + std::to_string(width) + 
        ",height=" + std::to_string(height) + ",framerate=" + std::to_string(fps) + "/1 "
        "! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
        "! rtph264pay config-interval=1 ! udpsink host=" + ip_address + " port=" + udp_port + " sync=false";

    gst_init(nullptr, nullptr);

    // Create the pipeline
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

    if (!pipeline || error) {
        std::cerr << "[OAK] Failed to create pipeline: " << (error ? error->message : "Unknown error") << std::endl;
        return -1;
    }

    // Handle termination signals
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    // Create pipeline
    dai::Pipeline dai_pipeline;
    auto colorCam = dai_pipeline.create<dai::node::ColorCamera>();
    auto xlinkOut = dai_pipeline.create<dai::node::XLinkOut>();
    xlinkOut->setStreamName("preview");
    
    // Set the resolution of the camera to a specific dimension (e.g., 640x480 or 1280x720)
    colorCam->setResolution(dai::ColorCameraProperties::SensorResolution::THE_1080_P); // 1280x720
    colorCam->setFps(30); // Set framerate to 30 FPS

    // Set interleaved preview stream (you can also change this to mono for grayscale if needed)
    colorCam->setInterleaved(true);
    colorCam->preview.link(xlinkOut->input);

    GstElement* app_src = gst_bin_get_by_name(GST_BIN(pipeline), "oak_src");

    try {
        // Try connecting to device and start the pipeline
        dai::Device device(dai_pipeline);

        // Get output queue
        auto preview = device.getOutputQueue("preview");

        cv::Mat frame;
        while (keep_running.load()) {

            // Receive 'preview' frame from device
            auto imgFrame = preview->get<dai::ImgFrame>();

            // Show the received 'preview' frame
            cv::imshow("preview", cv::Mat(imgFrame->getHeight(), imgFrame->getWidth(), CV_8UC3, imgFrame->getData().data()));

            // Wait and check if 'q' pressed
            if (cv::waitKey(1) == 'q') break;
        }
    } catch (const std::runtime_error& err) {
        std::cout << err.what() << std::endl;
    }

    return 0;
}
