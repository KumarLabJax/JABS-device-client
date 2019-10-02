#include <chrono>
#include <thread>

#include "pylon_camera.h"

namespace chrono = std::chrono;

void PylonCameraController::RecordVideo(RecordingSessionConfig config) {

    session_start_.store(chrono::system_clock::now().time_since_epoch());

    while(1) {
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::system_clock::now().time_since_epoch() - session_start_.load());
        
        // check to see if we've completed the specified duration or we've been told
        // to terminate early
        if (this->stop_recording_ || elapsed >= config.duration) {
            break;
        }
        
        //TODO  implement me
        
    }
    elapsed_time_ = chrono::duration_cast<chrono::seconds>(
        chrono::system_clock::now().time_since_epoch() - session_start_.load());
    recording_ = false;
}
