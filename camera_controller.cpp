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
    return timestamp(std::chrono::system_clock::now());
}

std::string CameraController::timestamp(std::chrono::time_point<std::chrono::system_clock> time)
{
    std::stringstream buf;
    std::time_t t = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::localtime(&t);
    buf << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return buf.str();
}

std::string CameraController::DateString(std::chrono::time_point<std::chrono::system_clock> time)
{
    std::stringstream buf;
    std::time_t t = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::localtime(&t);
    buf << std::put_time(&tm, "%Y-%m-%d");
    return buf.str();
}

int CameraController::GetCurrentHour(std::chrono::time_point<std::chrono::system_clock> time){
    std::time_t t = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::localtime(&t);
    return tm.tm_hour;
}

int CameraController::GetCurrentHour(){
    return GetCurrentHour(std::chrono::system_clock::now());
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

    elapsed_time_ = std::chrono::seconds::zero();

    // don't do anything if there is already an active recording thread
    if (recording_) {
        std::cerr << "Recording thread already running " << std::endl;
        return false;
    }

    // avoid resizing moving_avg_ during acquisition loop since we already
    // know the size
    moving_avg_.reserve(config.target_fps);

    // if a previous recording thread terminated on its own (recorded for the
    // specified duration) make sure to call join() so the thread is cleaned up
    if (recording_thread_.joinable()) {
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

const std::string CameraController::error_string()
{
    return err_msg_;
}

int CameraController::recording_error()
{
    return recording_err_state_;
}

std::string CameraController::MakeFilePath(std::chrono::time_point<std::chrono::system_clock> time, std::string filename)
{
    std::string path = directory_;

    if (directory_.back() == '/') {
        path.append(DateString(time) + "/");
    } else {
        path.append("/" + DateString(time) + "/");
    }
    path.append(filename);

    return path;
}