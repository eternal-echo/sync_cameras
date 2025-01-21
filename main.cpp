#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <functional>
#include <memory>  // For smart pointers
#include <filesystem>

#include <opencv2/opencv.hpp> 
#include <spdlog/spdlog.h>      // spdlog for logging
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "V4l2Capture.h"


// Function to initialize spdlog
static void init_logger(int verbose) {
    // Setup spdlog to log to both console and file
    if (verbose == 0) {
        spdlog::set_level(spdlog::level::info);  // Set to info level by default
    } else if (verbose == 1) {
        spdlog::set_level(spdlog::level::debug);  // More detailed logging (debug level)
    } else {
        spdlog::set_level(spdlog::level::trace);  // Trace level for maximum verbosity
    }

    auto console = spdlog::stdout_color_mt("console");
    auto file = spdlog::basic_logger_mt("basic_logger", "logs/log.txt");
    spdlog::set_default_logger(console);
}

// V4L2 Capture Initialization
static std::unique_ptr<V4l2Capture> init_v4l2_capture(const std::string& device, int format, int width, int height, int fps, V4l2IoType ioTypeIn) {
    V4L2DeviceParameters param(device.c_str(), format, width, height, fps, ioTypeIn);
    std::unique_ptr<V4l2Capture> video_ctx(V4l2Capture::create(param));

    if (!video_ctx) {
        spdlog::error("Cannot initialize V4L2 capture on device {}", device);
        std::exit(-1);
    } else {
        spdlog::info("V4L2 Capture Initialized for device: {}", device);
    }

    return video_ctx;
}


// capture thread
void capture_function(std::unique_ptr<V4l2Capture>& video_ctx, std::queue<std::vector<char>> &frame_queue, 
                    std::mutex &frame_mutex, std::condition_variable &cv, std::atomic<bool> &stop) {

    timeval tv;

    spdlog::info("Starting reading.");

    while (!stop) {

        tv.tv_sec=1;
        tv.tv_usec=0;
        int ret = video_ctx->isReadable(&tv);
        if (ret == 1) {
            // Read frame from V4L2 capture
            std::vector<char> frame(video_ctx->getBufferSize());
            size_t bytesRead = video_ctx->read(frame.data(), frame.size());
            if (bytesRead > 0) {
                frame.resize(bytesRead);
                spdlog::debug("captured frame size: {} {}", bytesRead, frame.size());
                {
                    // Lock the frame queue and push the frame
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    frame_queue.push(frame);
                }
                cv.notify_one();
            }
        } else if (ret == -1) {
            spdlog::error("Error reading frame: {}", strerror(errno));
            stop = true;
        }
    }
}

// Consumer thread
void consumer_function(std::function<void(std::vector<char>, int)> process_frame, const int stop_count,
                        std::queue<std::vector<char>> &frame_queue, std::mutex &frame_mutex, std::condition_variable &cv, std::atomic<bool> &stop) {
    int frame_count = 0;
    while (!stop && frame_count < stop_count) {
        std::unique_lock<std::mutex> lock(frame_mutex);
        cv.wait(lock, [&frame_queue, &stop] { return !frame_queue.empty() || stop; });
        if (!frame_queue.empty()) {
            std::vector<char> frame = frame_queue.front();
            frame_queue.pop();
            lock.unlock();
            // Process the frame
            process_frame(frame, frame_count);
        }
        frame_count++;
    }

    if (frame_count >= stop_count) {
        stop = true;
    }
}

void parse_args(int argc, char* argv[], std::string& device, int& width, 
                int& height, int& fps, int& format, V4l2IoType &ioTypeIn, int& frame_count, int& verbose) {
    int opt;
    while ((opt = getopt(argc, argv, "v:rd:hG:f:x:")) != -1) {
        switch (opt) {
            case 'v':
                verbose = std::stoi(optarg);
                break;
            case 'r':
                ioTypeIn = IOTYPE_READWRITE;
                break;
            case 'd':
                device = optarg;
                break;
            case 'x':
                frame_count = std::stoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-d device] [-G <W>x<H>x<FPS>] [-f format] [-r] [-v level]\n"
                       "Example: %s -d /dev/video0 -G 1280x720x30 -f MJPG -v 1\n",
                       argv[0], argv[0]);
                exit(0);
                break;
            case 'G':
                if(sscanf(optarg, "%dx%dx%d", &width, &height, &fps) != 3) {
                    fprintf(stderr, "Invalid size format. Use: WxHxFPS\n");
                    exit(1);
                }
                break;
            case 'f':
                if(strcmp(optarg, "YUYV") == 0)
                    format = V4L2_PIX_FMT_YUYV;
                else if(strcmp(optarg, "MJPG") == 0)
                    format = V4L2_PIX_FMT_MJPEG; 
                else {
                    fprintf(stderr, "Unsupported format: %s\n", optarg);
                    exit(1);
                }
                break;
            case '?':
                exit(1);
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string device = "/dev/video0";  // Default device path
    int width = 640, height = 480, fps = 0;
    int frame_count = 10;
    int format = 0;
    int verbose = 0;
    V4l2IoType ioTypeIn = IOTYPE_MMAP;  // Default IO type (Memory Map)

    // Parse command line arguments
    parse_args(argc, argv, device, width, height, fps, format, ioTypeIn, frame_count, verbose);

    // Initialize logger
    init_logger(verbose);

    spdlog::info("Starting V4L2 Capture");

    // queue to store captured frames
    std::queue<std::vector<char>> frame_queue;
    std::mutex frame_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop(false);

    // Capture thread
    std::unique_ptr<V4l2Capture> video_ctx = init_v4l2_capture(device, format, width, height, fps, ioTypeIn);
    std::thread capture_thread(capture_function, std::ref(video_ctx), std::ref(frame_queue), std::ref(frame_mutex), std::ref(cv), std::ref(stop));
    std::function<void(std::vector<char>, int)> save_frame = [](std::vector<char> frame, int frame_count) {
        // Process the frame here
        // For now, just print the size of the frame
        spdlog::info("Saved frame {} size: {}", frame_count, frame.size());
        // Convert the raw MJPEG frame (std::vector<char>) to a cv::Mat using OpenCV
        // MJPEG frames are basically JPEG compressed images
        cv::Mat img = cv::imdecode(frame, cv::IMREAD_COLOR);  // Decode as color image
        if (img.empty()) {
            spdlog::error("Failed to decode frame");
            return;
        }

        // Create a unique filename for each frame
        char filename[128];
        // Create a "output" directory if it doesn't exist
        if (!std::filesystem::exists("output")) {
            std::filesystem::create_directory("output");
        }

        snprintf(filename, sizeof(filename), "output/frame_%04d.jpg", frame_count);

        // Save the frame to disk as a JPEG image
        if (cv::imwrite(filename, img)) {
            spdlog::info("Frame {} saved as: {}", frame_count, filename);
        } else {
            spdlog::error("Failed to save frame {}.", frame_count);
        }

    };
    std::thread consumer_thread(consumer_function, save_frame, frame_count, std::ref(frame_queue), std::ref(frame_mutex), std::ref(cv), std::ref(stop));

    // Wait for the capture thread to finish
    capture_thread.join();
    consumer_thread.join();

    spdlog::info("Exiting V4L2 Capture");

    return 0;
}