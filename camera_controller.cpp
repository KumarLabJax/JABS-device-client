#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "camera_controller.h"

CameraController::CameraController(std::string directory) :
  directory_(directory),
  stop_recording_(false),
  recording_(false) {}
  
CameraController::~CameraController() {
    
    // make sure we stopped the recording thread
    if (recording_) {
        std::cerr << "CameraController destructor called with active recording thread. "
                  << "Terminating thread..." << std::endl;
        StopRecording();
    }
    
    // if the recording_thread_ pointer is still valid then it means the recording thread
    // loop finished on its own. call join to clean up the thread. 
    if (recording_thread_.joinable()) {
        recording_thread_.join();
    }
}

std::string CameraController::timestamp()
{
    std::stringstream buf;
    
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    buf << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return buf.str();

}

bool CameraController::recording()
{
    return recording_;
}


bool CameraController::StartRecording(RecordingSessionConfig config)
{
    // don't do anything if there is already an active recording thread
    if (recording_) {
        std::cerr << "Recording thread already running " << std::endl;
        return false;
    }

    // if a previous recording thread terminated on its own (recorded for the
    // specified duration)
    if (recording_thread_.joinable()) {
        recording_thread_.join();
    }
    
    recording_thread_ = std::thread(&CameraController::RecordVideo, this, config);
    return true;
}


void CameraController::StopRecording() {
    if (recording_) {
        stop_recording_ = true;
        
        // wait for the recording thread to finish
        recording_thread_.join();
        
        // reset the unique pointer so the destructor knows if it needs to call join
        // on the thread
        //recording_thread_.reset();
        recording_ = false;
    }
}