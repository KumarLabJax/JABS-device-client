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
    PylonCameraController(const std::string &directory) : CameraController(directory) {}

private:

    /**
     * private inner class used to extend Pylon::CConfigurationEventHandler to
     * apply our custom camera configuration
     */
    class CameraConfiguration : public Pylon::CConfigurationEventHandler {
    public:
        CameraConfiguration(int frame_width, int frame_height, int target_fps, std::string pixel_format,
                            std::string auto_gain, bool enablePGI);
        void OnOpened(Pylon::CInstantCamera &camera);

    private:
        int frame_width_;
        int frame_height_;
        int target_fps_;
        std::string pixel_format_;
        std::string auto_gain_;
        bool enable_pgi_;
    };

    // private methods

    /**
     * @brief implements the recording thread for a Basler camera using pylon
     *
     * @param config RecordingSessionConfig
     */
    void RecordVideo(RecordingSessionConfig config);

};
#endif