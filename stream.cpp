#include <librealsense2/rs.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>

GstElement *pipeline_color, *appsrc_color;
GstElement *pipeline_depth, *appsrc_depth;

// Function to push frames to GStreamer pipeline
void push_frame(rs2::pipeline &rs_pipeline, bool is_color, int width, int height, float depth_scale) {
    rs2::frameset frames;

    while (true) {
        frames = rs_pipeline.wait_for_frames();
        if (!frames) continue;

        cv::Mat frame_mat;

        if (is_color) {
            rs2::video_frame color_frame = frames.get_color_frame();
            if (!color_frame) continue;
            frame_mat = cv::Mat(height, width, CV_8UC3, (void*)color_frame.get_data());
            cv::cvtColor(frame_mat, frame_mat, cv::COLOR_RGB2BGR);
        } else {
            rs2::depth_frame depth_frame = frames.get_depth_frame();
            if (!depth_frame) continue;

            // Convert depth to grayscale based on user-defined depth scale
            cv::Mat depth_mat(height, width, CV_16UC1, (void*)depth_frame.get_data());
            cv::Mat depth_norm, depth_color;
            depth_mat.convertTo(depth_norm, CV_8UC1, 255.0 / depth_scale);
            cv::applyColorMap(depth_norm, depth_color, cv::COLORMAP_JET);
            frame_mat = depth_color;
        }

        GstElement *appsrc = is_color ? appsrc_color : appsrc_depth;
        
        int frame_size = width * height * 3;
        GstBuffer *buffer = gst_buffer_new_allocate(NULL, frame_size, NULL);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_WRITE);
        memcpy(map.data, frame_mat.data, frame_size);
        gst_buffer_unmap(buffer, &map);

        GstFlowReturn ret;
        g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
        gst_buffer_unref(buffer);

        if (ret != GST_FLOW_OK) {
            std::cerr << "Error pushing buffer" << std::endl;
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        std::cerr << "Usage: " << argv[0] << " <IP_ADDRESS> <COLOR_PORT> <DEPTH_PORT> <FPS> <WIDTH> <HEIGHT> <DEPTH_SCALE>" << std::endl;
        return -1;
    }

    std::string ip_address = argv[1];
    std::string color_port = argv[2];
    std::string depth_port = argv[3];
    int fps = std::stoi(argv[4]);
    int width = std::stoi(argv[5]);
    int height = std::stoi(argv[6]);
    float depth_scale = std::stof(argv[7]);  // New argument

    gst_init(&argc, &argv);

    // Unique appsrc names
    std::string pipeline_color_desc = "appsrc name=color_src format=time "
                                      "caps=video/x-raw,format=BGR,width=" + std::to_string(width) +
                                      ",height=" + std::to_string(height) + ",framerate=" + std::to_string(fps) + "/1 "
                                      "! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
                                      "! rtph264pay config-interval=1 ! udpsink host=" + ip_address + " port=" + color_port + " sync=false";

    std::string pipeline_depth_desc = "appsrc name=depth_src format=time "
                                      "caps=video/x-raw,format=BGR,width=" + std::to_string(width) +
                                      ",height=" + std::to_string(height) + ",framerate=" + std::to_string(fps) + "/1 "
                                      "! videoconvert ! x264enc speed-preset=ultrafast tune=zerolatency "
                                      "! rtph264pay config-interval=1 ! udpsink host=" + ip_address + " port=" + depth_port + " sync=false";

    GError *error = nullptr;
    pipeline_color = gst_parse_launch(pipeline_color_desc.c_str(), &error);
    pipeline_depth = gst_parse_launch(pipeline_depth_desc.c_str(), &error);

    if (!pipeline_color || !pipeline_depth || error) {
        std::cerr << "Failed to create GStreamer pipelines: " << (error ? error->message : "") << std::endl;
        return -1;
    }

    appsrc_color = gst_bin_get_by_name(GST_BIN(pipeline_color), "color_src");
    appsrc_depth = gst_bin_get_by_name(GST_BIN(pipeline_depth), "depth_src");

    gst_element_set_state(pipeline_color, GST_STATE_PLAYING);
    gst_element_set_state(pipeline_depth, GST_STATE_PLAYING);

    rs2::pipeline rs_pipeline;
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_COLOR, width, height, RS2_FORMAT_RGB8, fps);
    cfg.enable_stream(RS2_STREAM_DEPTH, width, height, RS2_FORMAT_Z16, fps);
    rs_pipeline.start(cfg);

    std::thread color_thread(push_frame, std::ref(rs_pipeline), true, width, height, depth_scale);
    std::thread depth_thread(push_frame, std::ref(rs_pipeline), false, width, height, depth_scale);

    color_thread.join();
    depth_thread.join();

    gst_element_set_state(pipeline_color, GST_STATE_NULL);
    gst_element_set_state(pipeline_depth, GST_STATE_NULL);
    gst_object_unref(pipeline_color);
    gst_object_unref(pipeline_depth);

    return 0;
}
