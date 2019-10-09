#include "video_writer.h"

VideoWriter::VideoWriter(std::string filename, const CameraController::RecordingSessionConfig& config)
{

}
VideoWriter::~VideoWriter() {}

void VideoWriter::EncodeFrame(uint8_t frame_data[], size_t current_frame)
{

}

int VideoWriter::RollFile(std::string filename)
{
    return 0;
}