// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#ifndef VIDEOWRITER_H
#define VIDEOWRITER_H

#include <cstdint>
#include <memory>
#include <string>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

#include "camera_controller.h"

/**
 * custom deleter so that we can have a std::unique_ptr manage an
 * AVCodecContext pointer
 */
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* context) {
        if(context)
        {
            if(avcodec_is_open(context))
            {
                avcodec_close(context);
            }
            avcodec_free_context(&context);
        }
    }
};

/**
 * custom deleter so that we can have a std::unique_ptr manage an
 * AVFormatContext pointer
 */
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* context) {
        if (context) {
            if (context->pb) {
                // if the file has been opened, make sure to write the trailer
                av_write_trailer(context);
                // close the AVIO context, will flush internal buffer
                avio_closep(&context->pb);
            }

            // free AVFormatContext memory
            avformat_free_context(context);
        }
    }
};

/**
 * custom deleter so that we can have a std::unique_ptr manage an
 * AVFilterInOut pointer
 */
struct AVFilterInOutDeleter {
    void operator()(AVFilterInOut* io) {
        if (io) {
            avfilter_inout_free(&io);
        }
    }
};

/**
 * custom deleter so that we can have a std::unique_ptr manage an
 * AVFilterGraph pointer
 */
struct AVFilterGraphDeleter {
    void operator()(AVFilterGraph* fg) {
        if (fg) {
            avfilter_graph_free(&fg);
        }
    }
};

/**
 * custom deleter so that we can have a std::unique_ptr manage an
 * AVFrame pointer
 */
struct AVFrameDeleter {
    void operator()(AVFrame* f) {
        if (f) {
            av_frame_free(&f);
        }
    }
};

/**
 * custom deleter so that we can have a std::unique_ptr manage an
 * AVPacket pointer
 */
struct AVPacketDeleter {
    void operator()(AVPacket* p) {
        if (p) {
            // we are allocating packets with av_packet_alloc, so we need to free them
            av_packet_free(&p);
        }
    }
};

/**
 * custom deleter so that we can have a std::unique_ptr manage an
 * AVBSFContext pointer
 */
struct AVBSFContextDeleter {
    void operator()(AVBSFContext* c) {
        if (c) {
            // we are allocating packets with av_packet_alloc, so we need to free them
            av_bsf_free(&c);
        }
    }
};

// namespace of various smart pointer types
// this lets us use `av_pointer::frame foo;` vs `std::unique_ptr<AVFrame, AVFrameDeleter> foo;`
namespace av_pointer {
using frame = std::unique_ptr<AVFrame, AVFrameDeleter>;
using codec_context = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using format_context = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using in_out = std::unique_ptr<AVFilterInOut, AVFilterInOutDeleter>;
using filter_graph = std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter>;
using packet = std::unique_ptr<AVPacket, AVPacketDeleter>;
using bsf_context = std::unique_ptr<AVBSFContext, AVBSFContextDeleter>;
}

class VideoWriter {
public:
    VideoWriter(
        const std::string& filename,
        const std::string& rtmp_uri,
        int frame_width,
        int frame_height,
        const CameraController::RecordingSessionConfig& config);

    /**
     * @brief destructor -- will make sure buffers are flushed
     */
    ~VideoWriter();

    /**
     * @brief move assignment operator
     * @param rhs temporary VideoWriter instance being moved to this VideoWriter
     * @return reference to this VideoWriter
     */
    VideoWriter& operator=(VideoWriter&& rhs);


    /**
     * @brief move constructor
     *
     * constructs a new VideoWriter from a temporary VideoWriter instance moving
     * ownership of smart pointer members to new VideoWriter
     */
    VideoWriter(VideoWriter&&);

    // delete the copy constructor and copy operator since they won't work with
    // a class with std::unique_ptr members
    VideoWriter(const VideoWriter&) = delete;
    VideoWriter& operator=(const VideoWriter&) = delete;

    // delete the default constructor so an uninitialized VideoWriter can't be
    // constructed
    VideoWriter() = delete;

    /**
     * @brief encode frame from the camera and write it to the file
     *
     * this is called for each frame grabbed from the camera
     *
     * @param buffer buffer containing raw data
     * @param current_frame
     */
    void EncodeFrame(uint8_t buffer[], size_t current_frame);

    /**
     * @brief enable/disable live streaming
     * @param state new state for live streaming setting
     */
    void SetLiveStreaming(bool state) {live_stream_ = state;}

private:

    /// rtmp uri
    std::string rtmp_uri_;

    /// pointer to specified codec
    const AVCodec *ffcodec_;

    /// flag to indicate if we should send frames through filter graph before encoding
    bool apply_filter_;

    /// enable live streaming
    bool live_stream_ = {false};

    /// specified pixel format
    enum AVPixelFormat selected_pixel_format_;

    /// file output stream. Will get freed up when format_context_ is deleted
    AVStream *stream_;

    /// rtmp output stream. Will get freed up when rtmp_format_context_ is deleted
    AVStream *rtmp_stream_;

    /// source for filter graph, will get freed when filter graph is deleted
    AVFilterContext *buffersink_ctx_;

    /// sink for filter graph, will get freed when filter graph is deleted
    AVFilterContext *buffersrc_ctx_;

    // we wrap the AV pointers in std::unique_ptr with delete functions so that
    // they'll get cleaned up properly
    /// smart pointer to AVCodecContext
    av_pointer::codec_context codec_context_;
    /// smart pointer to AVFormatContext
    av_pointer::format_context format_context_;
    /// smart pointer to AVFormatContext
    av_pointer::format_context rtmp_format_context_;
    /// smart pointer to AVFilterGraph
    av_pointer::filter_graph filter_graph_;
    /// smart pointer for AVBSFContext
    av_pointer::bsf_context bsfc_;

    /**
     * @brief initialize filters
     *
     * called during constructor if apply_filter_ is set to true.
     */
    void InitFilters();

    /**
     * @brief allocate an initialize a frame
     * @return std::unique_ptr managed AVFrame pointer
     */
    av_pointer::frame InitFrame();

    /**
     * @brief encodes frame and write to file
     * @param frame AVFrame pointer containing frame data
     */
    void Encode(AVFrame *frame);

    /**
     * @brief encode a raw Yuv420p frame
     * @param buffer raw frame data from camera
     * @param current_frame frame index
     */
    void EncodeYuv420p(uint8_t buffer[], size_t current_frame);
};

#endif
