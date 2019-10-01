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
 * threads, generating filenames, etc). It has a pure virtual function
 * RecordVideo() that must be implemented by a derived class. RecordVideo() is
 * executed in a separate thread managed by CameraController.
 * CameraController is intended to be used from a single thread.
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
        
        /**
         * @brief destructor for CameraController, if the destructor is called
         * while there is an active recording thread it will stop the thread
         * and wait for it to terminate
         *
         */
        ~CameraController();
        
        /**
         * @brief get recording status
         *
         * @returns true if recording thread is active, false otherwise
         */
        bool recording() {return recording_;}
        
        /**
         * @brief start the recording thread
         *
         * @returns true if thread is started, false otherwise
         */
        bool StartRecording(RecordingSessionConfig config);

        /**
         * @brief stop recording thread
         *
         * Signal the recording thread to stop and waits for the thread to
         * finish. This is a no op if the recording thread has not been started
         * and is also safe to call if the recording thread terminates on its
         * own.
         */
        void StopRecording();
        
    protected:
        std::string directory_; ///directory for storing video

        std::atomic_bool stop_recording_; ///used to signal to the recording thread to terminate early
        std::atomic_bool recording_;      ///are we recording video?

        std::thread recording_thread_; ///thread used to record video
        
        std::string timestamp(); ///return a timestamp formatted for use in filenames
        
    private:
        /**
         * @brief function executed in a thread by StartRecording()
         *
         * Pure virtual function that must be implemented by classes that
         * inherit from this abstract base class to create a functioning
         * CameraController. This function will handle recording video from
         * the camera. It should set recording when it starts and finishes.
         */
        virtual void RecordVideo(RecordingSessionConfig) = 0;
};
#endif