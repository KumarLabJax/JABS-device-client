#ifndef PYLON_CAMERA_H
#define PYLON_CAMERA_H

#include <string>

#include "camera_controller.h"

class PylonCameraController : public CameraController
{
    public:
	    PylonCameraController(std::string directory) : CameraController(directory){}
	private:
	    void RecordVideo(RecordingSessionConfig);
};
#endif