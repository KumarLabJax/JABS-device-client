#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <vector>



namespace pixel_type {
    class PixelType {
    public:
        operator std::string() const
        {
            return str_;
        }
    protected:
        const std::string str_;
    };

    class YUV420P: public PixelType {
    protected:
        const std::string str_ = "YUV420P";
    };
}

namespace codec {
    class CodecType {
    public:
        operator std::string() const
        {
            return str_;
        }
    protected:
        const std::string str_;
    };

    class MPEG4 : public CodecType {
    protected:
        const std::string str_ = "mpeg4";
    };

    class LIBX264 : public CodecType {
    protected:
        const std::string str_ = "libx264";
    };
}



/**
 * @brief camera controller abstract base class
 *
 * This class provides common functionality for controlling a camera (starting
 * and stopping recording threads, generating filenames, etc). It has a pure
 * virtual function RecordVideo() that must be implemented by a derived class.
 * RecordVideo() is executed in a separate thread managed by CameraController.
 * CameraController is intended to be used from a single thread.
 *
 */
class CameraController {

public:

    /**
     * @brief collection of recording session attributes to be passed into
     * StartRecording() as a parameter
     */
    struct RecordingSessionConfig {

        /// target frames per second for video acquisition
        unsigned int target_fps = 60;

        /// video files will be split into hour-long segments
        bool fragment_by_hour = false;

        /// filename prefix. All files created by this session will start with this
        /// string
        std::string file_prefix;

        /// duration of recording session, can be specified in units greater than
        /// seconds and conversion to seconds will happen automatically
        std::chrono::seconds duration;

        size_t frame_height = 800;
        size_t frame_width = 800;

        pixel_type::PixelType pixel_format = pixel_type::YUV420P();
        codec::CodecType codec = codec::LIBX264();
        std::string compression_target = "veryslow";
        int compression = 12;
        bool lossless = false;
        bool apply_filter = true;
    };

    /**
     * @brief construct a new CameraController
     *
     * @param directory base video capture directory
     */
    CameraController(const std::string &directory);

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
    bool recording() { return recording_; }

    /**
     * @brief start the recording thread
     *
     * @returns true if thread is started, false otherwise
     */
    bool StartRecording(RecordingSessionConfig config);

    /**
     * @brief return the duration of the recording session
     *
     * returns the duration of the recording session in seconds. If there is
     * no active session then it returns the duration of the last session
     *
     * @return time of recording session in seconds
     */
    std::chrono::seconds elapsed_time();

    /**
     * @brief stop recording thread
     *
     * Signal the recording thread to stop and waits for the thread to
     * finish. This is a no op if the recording thread has not been started
     * and is also safe to call if the recording thread terminates on its
     * own.
     */
    void StopRecording();

    /**
     * @brief get error string set by recording thread
     *
     * get the error string set by the recording thread. value is undefined if
     * the recording thread has not terminated with an error
     *
     * @return error string
     */
    const std::string error_string();

    /**
     * @brief get a recording error code
     *
     * get error code set by the recording thread. value is undefined if the
     * recording thread has not terminated
     *
     * @return 0 if there were no errors, non-zero value otherwise
     */
    int recording_error();

protected:
    const std::string directory_;     ///< directory for storing video
    std::atomic_bool stop_recording_ {false}; ///< used to signal to the recording thread to terminate early
    std::atomic_bool recording_ {false};      ///< are we recording video?
    std::thread recording_thread_;
    std::chrono::seconds elapsed_time_;   ///< duration of completed recording session
    std::atomic<std::chrono::high_resolution_clock::duration> session_start_;
    std::vector<double> moving_avg_;
    std::string err_msg_;
    int recording_err_state_;
    std::mutex mutex_;

    std::string timestamp();          ///< return a timestamp formatted for use in filenames
    std::string timestamp(std::chrono::time_point<std::chrono::system_clock> time);
    std::string DateString(std::chrono::time_point<std::chrono::system_clock> time);
    std::string MakeFilePath(std::chrono::time_point<std::chrono::system_clock> time, std::string filename);
    int GetCurrentHour(std::chrono::time_point<std::chrono::system_clock> time);
    int GetCurrentHour();


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