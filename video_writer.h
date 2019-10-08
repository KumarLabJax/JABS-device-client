#ifndef VIDEOWRITER_H
#define VIDEOWRITER_H

#include <cstdint>
#include <string>


class VideoWriter {
public:
    VideoWriter(std::string filename, size_t width_, size_t height_,
        size_t target_fps, std::string pixel_format, std::string codec,
        std::string compression_target, bool lossless, std::string compression,
        bool apply_filter);
    ~VideoWriter();
    void EncodeFrame(uint8_t frame_data[], size_t current_frame, bool apply_filter);
    int RollFile(std::string filename);
};


#endif
