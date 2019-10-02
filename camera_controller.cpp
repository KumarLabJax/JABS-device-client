#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "camera_controller.h"

CameraController::CameraController(const std::string &directory)  : directory_(directory) {}
  
CameraController::~CameraController() {
    
    // make sure we stopped the recording thread
    if (recording_) {
        std::cerr << "CameraController destructor called with active recording thread. "
                  << "Terminating thread..." << std::endl;
        StopRecording();
    }
    
    // if we are here and the recording_thread_  is still joinable then it means
    // the recording thread loop finished on its own and set recording_ to
    // false. Call join() to clean up the thread and avoid an exception.
    if (recording_thread_.joinable()) {
        recording_thread_.join();
    }
}

std::string CameraController::timestamp()
{
    std::stringstream buf;
    
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm = *std::localtime(&t);
    buf << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return buf.str();
}

bool CameraController::StartRecording(RecordingSessionConfig config)
{
    // validate the config
    if (config.file_prefix.empty()) {
        throw std::invalid_argument("file_prefix cannot be empty");
    }
    if (config.duration <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("duration must be at least one second");
    }

    // don't do anything if there is already an active recording thread
    if (recording_) {
        std::cerr << "Recording thread already running " << std::endl;
        return false;
    }

    std::cerr << "starting thread\n";

    // if a previous recording thread terminated on its own (recorded for the
    // specified duration) make sure to call join() so the thread is cleaned up
    if (recording_thread_.joinable()) {
        std::cerr << "foo" << std::endl;
        recording_thread_.join();
    }
    
    recording_thread_ = std::thread(&CameraController::RecordVideo, this, config);
    recording_ = true;

    return true;
}

void CameraController::StopRecording() {
    if (recording_) {
        stop_recording_ = true;
        // wait for the recording thread to finish
        recording_thread_.join();
    }
}

std::chrono::seconds CameraController::elapsed_time()
{
    if (recording_) {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch() - session_start_.load());
    }
    return elapsed_time_;
}