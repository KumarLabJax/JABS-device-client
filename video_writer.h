#ifndef VIDEOWRITER_H
#define VIDEOWRITER_H

#include <cstdint>
#include <string>

#include "camera_controller.h"

class VideoWriter {
public:
    VideoWriter(std::string filename, const CameraController::RecordingSessionConfig& config);
    ~VideoWriter();
    void EncodeFrame(uint8_t frame_data[], size_t current_frame);
    int RollFile(std::string filename);
};

#endif
