#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>

#include "camera_controller.h"

namespace codecs {
bool Validate(std::string name)
{
    return std::find(codec_names.begin(), codec_names.end(), name) != codec_names.end();
}
} //namespace codecs

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

bool CameraController::StartRecording(const RecordingSessionConfig& config)
{
    elapsed_time_ = std::chrono::seconds::zero();

    // don't do anything if there is already an active recording thread
    if (recording_) {
        std::cerr << "Recording thread already running " << std::endl;
        return false;
    }

    // avoid resizing moving_avg_ during acquisition loop since we already
    // know the size
    auto window_size = config.target_fps();
    moving_avg_.reserve(window_size);

    // if a previous recording thread terminated on its own (recorded for the
    // specified duration) make sure to call join() so the thread is cleaned up
    if (recording_thread_.joinable()) {
        recording_thread_.join();
    }

    recording_ = true;
    recording_thread_ = std::thread(&CameraController::RecordVideo, this, config);

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

double CameraController::avg_fps()
{
    double avg = 0.0;
    if (moving_avg_.size()) {
        std::lock_guard<std::mutex> lock(mutex_);
        avg = std::accumulate(moving_avg_.begin(), moving_avg_.end(), 0.0)/moving_avg_.size();
    }
    return avg;
}

const std::string CameraController::error_string()
{
    return err_msg_;
}

int CameraController::recording_error()
{
    return err_state_;
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

void CameraController::RecordingSessionConfig::set_target_fps(unsigned int target_fps)
{
    target_fps_ = target_fps;
}

void CameraController::RecordingSessionConfig::set_file_prefix(const std::string& prefix)
{
    if (prefix.empty()) {
        throw std::invalid_argument("file prefix can not be empty");
    }
    file_prefix_ = prefix;
}

void CameraController::RecordingSessionConfig::set_duration(std::chrono::seconds duration)
{
    if (duration.count() < 1) {
        throw std::invalid_argument("duration must be at least one second");
    }
    duration_ = duration;
}

void CameraController::RecordingSessionConfig::set_frame_height(size_t height)
{
    frame_height_ = height;
}

void CameraController::RecordingSessionConfig::set_frame_width(size_t width)
{
    frame_width_ = width;
}

void CameraController::RecordingSessionConfig::set_pixel_format(const std::string &format)
{
    if (!pixel_types::Validate(format)) {
        throw std::invalid_argument("invalid pixel format");
    }
    pixel_format_ = format;
}

void CameraController::RecordingSessionConfig::set_codec(std::string codec)
{
    if (!codecs::Validate(codec)) {
        throw std::invalid_argument("invalid codec name");
    }
    codec_ = codec;
}

void CameraController::RecordingSessionConfig::set_compression_target(const std::string &target)
{
    compression_target_ = target;
}

void CameraController::RecordingSessionConfig::set_crf(unsigned int crf)
{
    if (crf > 51) {
        throw std::invalid_argument("crf must be in the range [0, 51]");
    }
    crf_ = crf;
}
