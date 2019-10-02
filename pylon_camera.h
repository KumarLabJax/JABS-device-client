#ifndef PYLON_CAMERA_H
#define PYLON_CAMERA_H

#include <string>

#include "camera_controller.h"

/**
 * @brief CameraController for a Basler camera using pylon
 */
class PylonCameraController : public CameraController {
public:
    PylonCameraController(const std::string &directory) : CameraController(directory) {}

private:
    /**
     * @brief implements the recording thread for a Basler camera using pylon
     *
     * @param config RecordingSessionConfig
     */
    void RecordVideo(RecordingSessionConfig config);
};
#endif