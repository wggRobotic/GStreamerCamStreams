#include <librealsense2/rs.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstring>
#include <cstdlib>

std::atomic<bool> keep_running{true};

void handle_signal(int signum) {
    keep_running.store(false);
    std::cout << "[Realsense] Received signal " << signum << ". Stopping..." << std::endl;
}

GstElement *pipeline_color = nullptr, *appsrc_color = nullptr;
GstElement *pipeline_depth = nullptr, *appsrc_depth = nullptr;

void push_frame(rs2::pipeline &rs_pipeline, bool is_color, int width, int height, float depth_scale) {
    int frame_size = 3 * width * height; // Both color and depth are sent as RGB

    while (keep_running.load()) {
        rs2::frameset frames = rs_pipeline.wait_for_frames();
        if (!frames) continue;

        GstElement *appsrc = is_color ? appsrc_color : appsrc_depth;

        // Create a new buffer for each frame
        GstBuffer *buffer = gst_buffer_new_allocate(nullptr, frame_size, nullptr);
        if (!buffer) {
            std::cerr << "[Realsense] Failed to allocate GstBuffer" << std::endl;
            continue;
        }

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
            std::cerr << "[Realsense] Failed to map GstBuffer" << std::endl;
            gst_buffer_unref(buffer);
            continue;
        }

        if (is_color) {
            rs2::video_frame color_frame = frames.get_color_frame();
            if (!color_frame) {
                gst_buffer_unmap(buffer, &map);
                gst_buffer_unref(buffer);
                continue;
            }
            memcpy(map.data, color_frame.get_data(), frame_size);
        } else {
            rs2::depth_frame depth_frame = frames.get_depth_frame();
            if (!depth_frame) {
                gst_buffer_unmap(buffer, &map);
                gst_buffer_unref(buffer);
                continue;
            }
            cv::Mat depth_mat(height, width, CV_16UC1, (void*)depth_frame.get_data());
            depth_mat.convertTo(depth_mat, CV_8UC1, 255.0 / depth_scale);
            cv::applyColorMap(depth_mat, depth_mat, cv::COLORMAP_JET);
            memcpy(map.data, depth_mat.data, frame_size);
        }

        gst_buffer_unmap(buffer, &map);

        GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
        if (ret != GST_FLOW_OK) {
            std::cerr << "[Realsense] Error pushing buffer" << std::endl;
            gst_buffer_unref(buffer);
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 11) {
        std::cerr << "[Realsense] Usage: " << argv[0] << " <IP_ADDRESS> <COLOR_PORT> <DEPTH_PORT> <COLOR_FPS> <COLOR_WIDTH> <COLOR_HEIGHT> <DEPTH_FPS> <DEPTH_WIDTH> <DEPTH_HEIGHT> <DEPTH_SCALE>" << std::endl;
        return -1;
    }

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    std::string ip_address = argv[1];
    std::string color_port = argv[2];
    std::string depth_port = argv[3];

    int color_fps = std::stoi(argv[4]);
    int color_width = std::stoi(argv[5]);
    int color_height = std::stoi(argv[6]);

    int depth_fps = std::stoi(argv[7]);
    int depth_width = std::stoi(argv[8]);
    int depth_height = std::stoi(argv[9]);
    float depth_scale = std::stof(argv[10]);

    gst_init(&argc, &argv);

    std::string pipeline_color_desc = 
        "appsrc name=color_src format=time "
        "caps=video/x-raw,format=RGB,width=" + std::to_string(color_width) + 
        ",height=" + std::to_string(color_height) + ",framerate=" + std::to_string(color_fps) + "/1 "
        "! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
        "! rtph264pay config-interval=1 ! udpsink host=" + ip_address + " port=" + color_port + " sync=false";

    std::string pipeline_depth_desc = 
        "appsrc name=depth_src format=time "
        "caps=video/x-raw,format=RGB,width=" + std::to_string(depth_width) + 
        ",height=" + std::to_string(depth_height) + ",framerate=" + std::to_string(depth_fps) + "/1 "
        "! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
        "! rtph264pay config-interval=1 ! udpsink host=" + ip_address + " port=" + depth_port + " sync=false";

    GError *error = nullptr;
    pipeline_color = gst_parse_launch(pipeline_color_desc.c_str(), &error);
    pipeline_depth = gst_parse_launch(pipeline_depth_desc.c_str(), &error);

    if (!pipeline_color || !pipeline_depth || error) {
        std::cerr << "[Realsense] Failed to create GStreamer pipelines: " << (error ? error->message : "Unknown error") << std::endl;
        return -1;
    }

    appsrc_color = gst_bin_get_by_name(GST_BIN(pipeline_color), "color_src");
    appsrc_depth = gst_bin_get_by_name(GST_BIN(pipeline_depth), "depth_src");

    gst_element_set_state(pipeline_color, GST_STATE_PLAYING);
    gst_element_set_state(pipeline_depth, GST_STATE_PLAYING);

    rs2::pipeline rs_pipeline;
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_COLOR, color_width, color_height, RS2_FORMAT_RGB8, color_fps);
    cfg.enable_stream(RS2_STREAM_DEPTH, depth_width, depth_height, RS2_FORMAT_Z16, depth_fps);
    rs_pipeline.start(cfg);

    std::thread color_thread(push_frame, std::ref(rs_pipeline), true, color_width, color_height, depth_scale);
    std::thread depth_thread(push_frame, std::ref(rs_pipeline), false, depth_width, depth_height, depth_scale);

    std::cout << "[Realsense] Streaming color at " 
              << color_fps << " FPS " << "to " << ip_address << ":" << color_port << " and depth at " 
              << depth_fps << " FPS " << "to " << ip_address << ":" << depth_port << std::endl;

    color_thread.join();
    depth_thread.join();

    gst_element_set_state(pipeline_color, GST_STATE_NULL);
    gst_element_set_state(pipeline_depth, GST_STATE_NULL);
    gst_object_unref(pipeline_color);
    gst_object_unref(pipeline_depth);

    std::cout << "[Realsense] Closed" << std::endl;

    return 0;
}
