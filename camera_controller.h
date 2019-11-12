// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "pixel_types.h"

namespace codecs {
static const std::vector<std::string> codec_names({"mpeg4", "libx264"});
static const std::string MPEG4 = "mpeg4";
static const std::string LIBX264 = "libx264";

/**
 * @brief function to check that a string is a valid codec name
 * @param type_name name to check
 * @return true if valid false otherwise
 */
bool Validate(std::string type_name);
} // namespace codecs


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
     * StartRecording() as a parameter.
     *
     * Setter functions added to perform validation -- any setter that does
     * parameter validation will throw a std::invalid_argument exception for
     * invalid values.
     *
     */
    class RecordingSessionConfig {
    public:
        /// get target frames per second
        int target_fps() const {return target_fps_;}

        /// get fragment flag
        bool fragment_by_hour() const {return fragment_by_hour_;}

        /// get file prefix string
        const std::string& file_prefix() const {return file_prefix_;}

        /// get duration of session
        std::chrono::seconds duration() const {return duration_;}

        /// get pixel format as string
        const std::string& pixel_format() const {return pixel_format_;}

        /// get codec name to use for encoding
        const std::string& codec() const {return codec_;}

        /// get the compression target preset name
        const std::string& compression_target() const {return compression_target_;}

        /// get the constant rate factor (CRF)
        unsigned int crf() const {return crf_;}

        /// get filtering flag
        bool apply_filter() const {return apply_filter_;}

        /// set target fps
        void set_target_fps(unsigned int target_fps);

        /// set fragment flag
        void set_fragment_by_hour(bool fragment) {fragment_by_hour_ = fragment;}

        /// set file prefix
        void set_file_prefix(const std::string& prefix);

        /// set session duration in seconds
        void set_duration(std::chrono::seconds duration);

        /// set pixel format
        void set_pixel_format(const std::string &format);

        /// set codec
        void set_codec(std::string codec);

        /// set compression preset
        void set_compression_target(const std::string &target);

        /// set constant rate factor (CRF)
        void set_crf(unsigned int crf);

        /// set filtering flag
        void set_apply_filter(bool apply_filter) {apply_filter_ = apply_filter;}

    private:
        /// target frames per second for video acquisition
        int target_fps_ = 60;

        /// video files will be split into hour-long segments
        bool fragment_by_hour_ = false;

        /// filename prefix. All files created by this session will start with this
        /// string
        std::string file_prefix_;

        /// duration of recording session, can be specified in units greater than
        /// seconds and conversion to seconds will happen automatically
        std::chrono::seconds duration_;

        /// pixel format
        std::string pixel_format_ = pixel_types::YUV420P;

        /// codec used for video encoding
        std::string codec_ = codecs::LIBX264;

        /// compression preset
        std::string compression_target_ = "veryfast";

        /// compression Constant Rate Factor (CRF) 0 = lossless, 51 = worst possible quality
        unsigned int crf_ = 11;

        bool apply_filter_ = false;
    };

    /**
     * @brief construct a new CameraController
     *
     * @param directory base video capture directory
     * @param frame_height
     * @param frame_width
     */
    CameraController(const std::string &directory, int frame_height, int frame_width);

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
    bool StartRecording(const RecordingSessionConfig &config);

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
     * @brief return the duration of the recording session
     *
     * returns the duration of the recording session in seconds. If there is
     * no active session then it returns the duration of the last session
     *
     * @return time of recording session in seconds
     */
    std::chrono::seconds elapsed_time();

    /**
     * @brief get the average frames per second
     *
     * uses a moving window average to calculate frames per second
     *
     * @return average frames per second
     */
    double avg_fps();

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

    /**
     * @brief set new frame width
     *
     * this setter, as well as SetFrameHeight and SetDirectory are used after
     * processing a HUP signal to re-read the config file
     *
     * @param width new width
     */
    void SetFrameWidth(int width);

    /**
     * @brief set new frame width
     * @param height new frame width
     */
    void SetFrameHeight(int height);

    /**
     * @brief set new output directory
     * @param dir path to new output directory
     */
    void SetDirectory(std::string dir);

protected:
    std::string directory_;     ///< directory for storing video
    std::atomic_bool stop_recording_ {false}; ///< used to signal to the recording thread to terminate early
    std::atomic_bool recording_ {false};      ///< are we recording video?
    std::thread recording_thread_;            ///< current recording thread
    std::chrono::seconds elapsed_time_;   ///< duration of completed recording session
    std::atomic<std::chrono::high_resolution_clock::duration> session_start_;
    std::vector<double> moving_avg_;  ///<  buffer storing fps for last N frames captured where N is the target framerate
    std::string err_msg_;  ///< error message if recording_err_
    int err_state_;       ///< error state of last completed recording session
    std::mutex mutex_;    ///< mutex for protecting some variables shared by controlling thread and recording thread
    int frame_width_;  ///< frame width, loaded from config file
    int frame_height_; ///< frame height, loaded from config file

    /**
     * @brief generates a timestamp string for use in filenames.
     *
     * generates a string with the format YYYY-MM-DD_HH-MM-SS
     *
     * @param time system clock time
     * @return
     */
    std::string timestamp(std::chrono::time_point<std::chrono::system_clock> time);

    /**
     * @brief generate a timestamp string with the current time
     *
     * generates a string with the format YYYY-MM-DD_HH-MM-SS using the current time
     *
     * @return
     */
    std::string timestamp();

    /**
     * @brief generates a date string in the format YYYY-MM-DD
     *
     * @param time system time to use to generate date string
     * @return
     */
    std::string DateString(std::chrono::time_point<std::chrono::system_clock> time);

    /**
     * @brief make a directory using the recording directory and a date string
     * @param time sytem time to use to generate a date string
     * @return path
     */
    std::string MakeFilePath(std::chrono::time_point<std::chrono::system_clock> time);

    /**
     * @brief get hour using a given system clock time
     * @param time current time
     * @return integer hour
     */
    int GetCurrentHour(std::chrono::time_point<std::chrono::system_clock> time);

    /**
     * @brief get hour using current time
     * @return integer hour
     */
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
    virtual void RecordVideo(const RecordingSessionConfig&) = 0;
};
#endif