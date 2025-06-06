#include <chrono>
#include <iostream>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <depthai/depthai.hpp>

using namespace std;
using namespace std::chrono;

std::atomic<bool> keep_running{true};

void handle_signal(int signum) {
    keep_running.store(false);
    std::cout << "[Realsense] Received signal " << signum << ". Stopping..." << std::endl;
}

GstElement *gst_pipeline = nullptr, *appsrc = nullptr;

static const std::vector<std::string> labelMap = {
    "blasting_agents", "corrosive", "dangerous_when_wet", "explosives",
    "flammable_gas", "flammable_solid", "fuel_oil", "inhalation_hazard",
    "non_flammable_gas", "organic_peroxide", "oxidizer",
    "oxygen", "poison", "radioactive", "spontaneously_combustible"
};

static std::atomic<bool> syncNN{true};

int main(int argc, char** argv) {

    if (argc != 3) {
        std::cerr << "[OAK] Usage: " << argv[0] << " <IP_ADDRESS> <PORT>" << std::endl;
        return -1;
    }

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    std::string ip_address = argv[1];
    std::string port = argv[2];

    gst_init(&argc, &argv);

    std::string pipeline_desc = 
        "appsrc name=src format=time "
        "caps=video/x-raw,format=RGB,width=640,height=640,framerate=35/1 "
        "! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
        "! rtph264pay config-interval=1 ! udpsink host=" + ip_address + " port=" + port + " sync=false";

    std::string nnPath("../yolov8n_640_hazmat15.blob");

    // Print which blob we are using
    printf("[OAK] Using blob at path: %s\n", nnPath.c_str());

    // Create pipeline
    dai::Pipeline pipeline;

    // Define sources and outputs
    auto camRgb = pipeline.create<dai::node::ColorCamera>();
    auto detectionNetwork = pipeline.create<dai::node::YoloDetectionNetwork>();
    auto xoutRgb = pipeline.create<dai::node::XLinkOut>();
    auto nnOut = pipeline.create<dai::node::XLinkOut>();

    xoutRgb->setStreamName("rgb");
    nnOut->setStreamName("detections");

    // Properties
    camRgb->setPreviewSize(640, 640);
    camRgb->setResolution(dai::ColorCameraProperties::SensorResolution::THE_1080_P);
    camRgb->setInterleaved(false);
    camRgb->setColorOrder(dai::ColorCameraProperties::ColorOrder::BGR);
    camRgb->setFps(40);

    // Network specific settings
    detectionNetwork->setConfidenceThreshold(0.5f);
    detectionNetwork->setNumClasses(15);
    detectionNetwork->setCoordinateSize(4);
    detectionNetwork->setIouThreshold(0.5f);
    detectionNetwork->setBlobPath(nnPath);
    detectionNetwork->setNumInferenceThreads(2);
    detectionNetwork->input.setBlocking(false);

    // Linking
    camRgb->preview.link(detectionNetwork->input);
    if(syncNN) {
        detectionNetwork->passthrough.link(xoutRgb->input);
    } else {
        camRgb->preview.link(xoutRgb->input);
    }

    detectionNetwork->out.link(nnOut->input);

    // Connect to device and start pipeline
    dai::Device device(pipeline);

    // Output queues will be used to get the rgb frames and nn data from the outputs defined above
    auto qRgb = device.getOutputQueue("rgb", 4, false);
    auto qDet = device.getOutputQueue("detections", 4, false);

    cv::Mat frame;
    std::vector<dai::ImgDetection> detections;
    auto startTime = steady_clock::now();
    int counter = 0;
    float fps = 0;
    auto color2 = cv::Scalar(255, 255, 255);

    // Add bounding boxes and text to the frame and show it to the user
    auto displayFrame = [](std::string name, cv::Mat frame, std::vector<dai::ImgDetection>& detections) {
        int frame_size = 3 * 640 * 640;

        auto color = cv::Scalar(255, 0, 0);
        // nn data, being the bounding box locations, are in <0..1> range - they need to be normalized with frame width/height
        for(auto& detection : detections) {
            int x1 = detection.xmin * frame.cols;
            int y1 = detection.ymin * frame.rows;
            int x2 = detection.xmax * frame.cols;
            int y2 = detection.ymax * frame.rows;

            uint32_t labelIndex = detection.label;
            std::string labelStr = to_string(labelIndex);
            if(labelIndex < labelMap.size()) {
                labelStr = labelMap[labelIndex];
            }
            cv::putText(frame, labelStr, cv::Point(x1 + 10, y1 + 20), cv::FONT_HERSHEY_TRIPLEX, 0.5, 255);
            std::stringstream confStr;
            confStr << std::fixed << std::setprecision(2) << detection.confidence * 100;
            cv::putText(frame, confStr.str(), cv::Point(x1 + 10, y1 + 40), cv::FONT_HERSHEY_TRIPLEX, 0.5, 255);
            cv::rectangle(frame, cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2)), color, cv::FONT_HERSHEY_SIMPLEX);
        }

        GstBuffer *buffer = gst_buffer_new_allocate(nullptr, frame_size, nullptr);
        if (!buffer) {
            std::cerr << "[OAK] Failed to allocate GstBuffer" << std::endl;
            return;
        }

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
            std::cerr << "[OAK] Failed to map GstBuffer" << std::endl;
            gst_buffer_unref(buffer);
            return;
        }

        memcpy(map.data, frame.data, frame_size);

        gst_buffer_unmap(buffer, &map);

        GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
        if (ret != GST_FLOW_OK) {
            std::cerr << "[OAK] Error pushing buffer" << std::endl;
            gst_buffer_unref(buffer);
            return;
        }
    };

    GError *error = nullptr;
    gst_pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

    if (!gst_pipeline || error) {
        std::cerr << "[OAK] Failed to create GStreamer pipeline: " << (error ? error->message : "Unknown error") << std::endl;
        return -1;
    }

    appsrc = gst_bin_get_by_name(GST_BIN(gst_pipeline), "src");

    gst_element_set_state(gst_pipeline, GST_STATE_PLAYING);

    std::cout << "[OAK] Streaming to " << ip_address << ":" << port << std::endl;

    while(keep_running.load()) {
        std::shared_ptr<dai::ImgFrame> inRgb;
        std::shared_ptr<dai::ImgDetections> inDet;

        if(syncNN) {
            inRgb = qRgb->get<dai::ImgFrame>();
            inDet = qDet->get<dai::ImgDetections>();
        } else {
            inRgb = qRgb->tryGet<dai::ImgFrame>();
            inDet = qDet->tryGet<dai::ImgDetections>();
        }

        counter++;
        auto currentTime = steady_clock::now();
        auto elapsed = duration_cast<duration<float>>(currentTime - startTime);
        if(elapsed > seconds(1)) {
            fps = counter / elapsed.count();
            counter = 0;
            startTime = currentTime;
        }

        if(inRgb) {
            frame = inRgb->getCvFrame();
            std::stringstream fpsStr;
            fpsStr << "NN fps: " << std::fixed << std::setprecision(2) << fps;
            cv::putText(frame, fpsStr.str(), cv::Point(2, inRgb->getHeight() - 4), cv::FONT_HERSHEY_TRIPLEX, 0.4, color2);
        }

        if(inDet) {
            detections = inDet->detections;
        }

        if(!frame.empty()) {
            displayFrame("rgb", frame, detections);
        }
    }
    
    gst_element_set_state(gst_pipeline, GST_STATE_NULL);
    gst_object_unref(gst_pipeline);

    std::cout << "[OAK] Closed" << std::endl;
}