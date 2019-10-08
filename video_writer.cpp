#include "video_writer.h"

VideoWriter::VideoWriter(std::string filename, size_t width_, size_t height_,
                         size_t target_fps, std::string pixel_format, std::string codec,
                         std::string compression_target, bool lossless, std::string compression,
                         bool apply_filter)
{

}
VideoWriter::~VideoWriter() {}

void VideoWriter::EncodeFrame(uint8_t frame_data[], size_t current_frame, bool apply_filter)
{

}

int VideoWriter::RollFile(std::string filename)
{
    return 0;
}