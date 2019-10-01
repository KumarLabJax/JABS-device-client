#include <chrono>
#include <thread>

#include "pylon_camera.h"

namespace chrono = std::chrono;

void PylonCameraController::RecordVideo(RecordingSessionConfig config) {

    recording_ = true;
    auto session_start = chrono::system_clock::now();

    while(1) {
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::system_clock::now() - session_start
        );
        
        // check to see if we've completed the specified duration or we've been told
        // to terminate early
        if (this->stop_recording_ || elapsed >= config.duration) {
            break;
        }
        
        //TODO  implement me
        
    }
    recording_ = false;
}
