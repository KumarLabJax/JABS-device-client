// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#ifndef PYLON_CAMERA_H
#define PYLON_CAMERA_H

#include <string>

#include <pylon/PylonIncludes.h>
#include <pylon/gige/BaslerGigEInstantCamera.h>
#include <pylon/VideoWriter.h>

#include "camera_controller.h"

/**
 * @brief CameraController for a Basler camera using pylon
 */
class PylonCameraController : public CameraController {
public:
    PylonCameraController(const std::string &directory, int frame_width, int frame_height, const std::string &nv_room_string, const std::string &rtmp_uri) :
        CameraController(directory, frame_width, frame_height, nv_room_string, rtmp_uri) {}

private:

    /**
     * private inner class used to extend Pylon::CConfigurationEventHandler to
     * apply our custom camera configuration
     */
    class CameraConfiguration : public Pylon::CConfigurationEventHandler {
    public:
        CameraConfiguration(int frame_width, int frame_height, int target_fps,
                            const std::string& pixel_format, bool enablePGI);
        void OnOpened(Pylon::CInstantCamera &camera);

    private:
        int frame_width_;                      ///< frame width in pixels
        int frame_height_;                     ///< frame height in pixels
        int target_fps_;                       ///< target frames per second
        std::string pixel_format_;  ///< pixel format
        bool enable_pgi_;                      ///< enable pgi flag
    };

    // private methods

    /**
     * @brief implements the recording thread for a Basler camera using pylon
     *
     * @param config RecordingSessionConfig
     */
    void RecordVideo(const RecordingSessionConfig& config);

};
#endif