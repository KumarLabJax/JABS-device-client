#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

struct RecordingSessionConfig {

    /// video files will be split into hour-long segments
    bool fragment_by_hour = false;
    
    /// filename prefix
    std::string file_prefix;
    
    /// recording session duration, can be any std::chrono::duration that can be converted
    /// to seconds
    std::chrono::seconds duration;
};


/**
 * @brief camera controller abstract base class
 *
 * this class provides common functionality (starting and stopping recording 
 * threads, generating filenames, etc). It has a pure virtual function RecordVideo() that 
 * must be implemented by a derived class. 
 *
 */
class CameraController {

    public:
        /**
         * @brief construct a new CameraController
         *
         * @param directory base video capture directory
         */
        CameraController(std::string directory);
        
        ~CameraController();
        
        /**
         * @returns true if recording thread is active, false otherwise
         */
        bool recording();
        
        /**
         * @brief start the recording thread
         *
         * @returns true if thread is started, false otherwise
         */
        bool StartRecording(RecordingSessionConfig config);
        void StopRecording();
        
    protected:
        std::string directory_;
        std::string file_prefix_;
        
        std::atomic_bool stop_recording_;
        std::atomic_bool recording_;

        std::unique_ptr<std::thread> recording_thread_;
        
        std::string timestamp();
        
    private:
        virtual void RecordVideo(RecordingSessionConfig) = 0;
};
#endif