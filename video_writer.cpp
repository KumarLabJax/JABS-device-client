#include <cstring>
#include <stdexcept>
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

#include "pixel_types.h"
#include "video_writer.h"
#include "camera_controller.h"

// replace the av_err2str macro with something that works for C++
// see libav-user mailing list: https://ffmpeg.org/pipermail/libav-user/2013-January/003458.html
#undef av_err2str
#define av_err2str(errnum) av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)

// move assignment operator
VideoWriter & VideoWriter::operator=(VideoWriter &&o)
{
    if (this != &o)
    {
        ffcodec_ = o.ffcodec_;
        apply_filter_ = o.apply_filter_;
        selected_pixel_format_ = o.selected_pixel_format_;
        stream_ = o.stream_;
        codec_context_ = std::move(o.codec_context_);
        format_context_ = std::move(o.format_context_);
        filter_graph_ = std::move(o.filter_graph_);
    }
    return *this;
}

// move constructor
VideoWriter::VideoWriter(VideoWriter &&o) : ffcodec_(o.ffcodec_), apply_filter_(o.apply_filter_),
                                            selected_pixel_format_(o.selected_pixel_format_),
                                            stream_(o.stream_),
                                            codec_context_(std::move(o.codec_context_)),
                                            format_context_(std::move(o.format_context_)),
                                            filter_graph_(std::move(o.filter_graph_)){}

VideoWriter::VideoWriter(const std::string& filename, const CameraController::RecordingSessionConfig& config)
{
    if (config.codec() != codecs::LIBX264) {
        throw std::invalid_argument("currently only libx264 codec is supported");
    }

    std::string full_filename = filename;
    full_filename.append(".avi");


    ffcodec_ = avcodec_find_encoder_by_name(config.codec().c_str());
    if (!ffcodec_) {
        throw std::invalid_argument("unable to get codec " + config.codec());
    }

    apply_filter_ = false; //config.apply_filter();
    codec_context_ = av_pointer::codec_context(avcodec_alloc_context3(ffcodec_));

    if (!codec_context_) {
        throw std::runtime_error("unable to initialize AVCodecContext");
    }

    codec_context_->width = config.frame_width();
    codec_context_->height = config.frame_height();
    codec_context_->time_base = (AVRational){1, config.target_fps()};
    codec_context_->framerate = (AVRational){config.target_fps(), 1};
    codec_context_->global_quality = 0;
    codec_context_->compression_level = 0;

    if (config.pixel_format() == pixel_types::MONO12) {
        codec_context_->bits_per_raw_sample = 12;
    } else {
        codec_context_->bits_per_raw_sample = 8;
    }

    // x264 only settings
    if (config.codec() == codecs::LIBX264) {
        av_opt_set(codec_context_->priv_data, "preset", config.compression_target().c_str(), 0);
        av_opt_set(codec_context_->priv_data, "crf", std::to_string(config.crf()).c_str(), 0);
    }

    codec_context_->gop_size = 1;
    codec_context_->max_b_frames = 1;

    if (config.pixel_format() == pixel_types::MONO8) {
        selected_pixel_format_ = AV_PIX_FMT_GRAY8;
    } else if (config.pixel_format() == pixel_types::MONO12) {
        selected_pixel_format_ = AV_PIX_FMT_GRAY12;
    } else {
        selected_pixel_format_ = AV_PIX_FMT_YUV420P;
    }
    codec_context_->pix_fmt = selected_pixel_format_;

    // Open up the ffmpeg codec
    if (avcodec_open2(codec_context_.get(), ffcodec_, NULL) < 0) {
        throw std::runtime_error("unable to open ffmpeg codec");
    }

    // setup the stream
    AVFormatContext *tmp_f_context;
    avformat_alloc_output_context2(&tmp_f_context, NULL, NULL, full_filename.c_str());
    format_context_ = av_pointer::format_context(tmp_f_context);
    stream_ = avformat_new_stream(format_context_.get(), ffcodec_);
    avcodec_parameters_from_context(stream_->codecpar, codec_context_.get());
    stream_->time_base = codec_context_->time_base;
    stream_->r_frame_rate = codec_context_->framerate;

    /* open the output file, if needed */
    int r = avio_open(&format_context_->pb, full_filename.c_str(), AVIO_FLAG_WRITE);
    if (r < 0) {
        throw std::runtime_error("unable to open " + full_filename + " : " + av_err2str(r));
    }

    if (avformat_write_header(format_context_.get(), NULL) < 0) {
        throw std::runtime_error("unable to write header");
    }

    // initializing filter
    if (apply_filter_) {
        InitFilters();
    }
}

VideoWriter::~VideoWriter()
{
    // flush buffers
    if (apply_filter_) {
        int rval = av_buffersrc_write_frame(buffersrc_ctx_, NULL);
        if (rval < 0) {
            std::cerr << "av_bufferserc_write_frame() returned " << rval << std::endl;
        }
    }
    apply_filter_ = false;
    Encode((AVFrame*)NULL);
}

void VideoWriter::InitFilters()
{
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    enum AVPixelFormat pix_fmts[] = {selected_pixel_format_, AV_PIX_FMT_NONE};
    const char *filter_descr = "hqdn3d=luma_spatial=10";

    // use smart pointers for these so they get cleaned up if we throw any
    // exceptions during initialization
    av_pointer::in_out outputs(avfilter_inout_alloc());
    av_pointer::in_out inputs(avfilter_inout_alloc());

    filter_graph_ = av_pointer::filter_graph(avfilter_graph_alloc());

    if (!outputs || !inputs || !filter_graph_) {
        throw std::runtime_error("unable to initialize filters");
    }

    avfilter_graph_set_auto_convert(filter_graph_.get(), AVFILTER_AUTO_CONVERT_NONE);

    std::string args =
        "video_size=" + std::to_string(codec_context_->width) + "x" + std::to_string(codec_context_->height) +
        ":pix_fmt=" + std::to_string(codec_context_->pix_fmt) + ":time_base=" +
        std::to_string(codec_context_->time_base.num) + "/" + std::to_string(codec_context_->time_base.den);

    if (avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc, "in", args.c_str(), NULL, filter_graph_.get()) < 0) {
        throw std::runtime_error("unable to create buffer source");
    }

    /* buffer video sink: to terminate the filter chain. */
    if (avfilter_graph_create_filter(&buffersink_ctx_, buffersink, "out", NULL, NULL, filter_graph_.get()) < 0) {
        throw std::runtime_error("unable to create buffer sink");
    }

    if (av_opt_set_int_list(buffersink_ctx_, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        throw std::runtime_error("unable to set ouptut pixel format");
    }

    /*
	 * Set the endpoints for the filter graph. The filter_graph will
	 * be linked to the graph described by filters_descr.
	 *
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */

    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx_;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    /*
	 * The buffer sink input must be connected to the output pad of
	 * the last filter described by filters_descr; since the last
	 * filter output label is not specified, it is set to "out" by
	 * default.
	 */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx_;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    // need to manage inputs & outputs manually now so we can pass pointer references
    AVFilterInOut *ins = inputs.release();
    AVFilterInOut *outs = outputs.release();

    int rval = avfilter_graph_parse_ptr(filter_graph_.get(), filter_descr, &ins, &outs, NULL);

    // free the released pointers
    avfilter_inout_free(&ins);
    avfilter_inout_free(&outs);

    if (rval < 0) {
        throw std::runtime_error("unable to link filter description to filter graph");
    }

    if (avfilter_graph_config(filter_graph_.get(), NULL) < 0) {
        throw std::runtime_error("unable to configure filter graph");
    }
}

// initialize a frame and return a pointer to it
av_pointer::frame VideoWriter::InitFrame() {

    // temporary smart pointer, can be returned from function without using
    // std::move to tranfer ownership
    av_pointer::frame frame(av_frame_alloc());

    if (!frame) {
        throw std::runtime_error("unable to allocate frame");
    }

    // initialize newly allocated frame
    frame->format = codec_context_->pix_fmt;
    frame->width  = codec_context_->width;
    frame->height = codec_context_->height;

    // 0 means "system select".
    // 8 recommended from doc tutorial?
    if (av_frame_get_buffer(frame.get(), 8) < 0) {
        throw std::runtime_error("unable to allocate frame buffer");
    }

    // make writable
    if (av_frame_make_writable(frame.get()) < 0) {
        throw std::runtime_error("unable to make frame writable");
    }

    return frame;
}

// encode a raw frame from the camera
void VideoWriter::EncodeFrame(uint8_t buffer[], size_t current_frame)
{
    if (selected_pixel_format_ == AV_PIX_FMT_YUV420P) {
        EncodeYuv420p(buffer, current_frame);
    } else {
        // unknown pixel format, we shouldn't get this far with an unknown pixel format
        // this will let us know we haven't implemented the encoder yet
        throw std::logic_error("encoder not implemented for pixel format");
    }
}

// encode a frame using Yuv420p pixel format
void VideoWriter::EncodeYuv420p(uint8_t buffer[], size_t current_frame)
{
    // create smart pointer to frame
    auto frame = InitFrame();

    // copy data to frame
    std::memcpy(frame->data[0], buffer, codec_context_->width * codec_context_->height);
    frame->pts = current_frame;

    /* Cb and Cr always set to grayscale*/
    for (int y = 0; y < codec_context_->height / 2; y++) {
        auto yl1 = y * frame->linesize[1];
        auto yl2 = y * frame->linesize[2];
        for (int x = 0; x < codec_context_->width / 2; x++) {
            frame->data[1][yl1 + x] = 128;
            frame->data[2][yl2 + x] = 128;
        }
    }

    // send frame to encoder
    Encode(frame.get());
}

// send the frame to the encoder, filtering first if necessary
void VideoWriter::Encode(AVFrame *frame)
{
    int rval;
    if (!apply_filter_) {
        //send frame to encoder
        rval = avcodec_send_frame(codec_context_.get(), frame);
        if (rval < 0) {
            throw std::runtime_error("error sending frame for encoding");
        }
    } else {
        // push frame to filter
        if (av_buffersrc_add_frame_flags(buffersrc_ctx_, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
            throw std::runtime_error("could not send frame to filter graph");
        }

        // pull frame from filter
        // frame smart pointer
        av_pointer::frame filt_frame = av_pointer::frame(av_frame_alloc());
        if (!filt_frame) {
            throw std::runtime_error("unable to allocate filtered frame");
        }

        av_buffersink_get_frame(buffersink_ctx_, filt_frame.get());

        rval = avcodec_send_frame(codec_context_.get(), filt_frame.get());
        if (rval < 0) {
            throw std::runtime_error("Error sending a frame for encoding");
        }
    }

    // get packets from encoder
    while (rval >= 0) {
        // create smart pointer to allocated packet
        av_pointer::packet pkt(av_packet_alloc());
        if (!pkt) {
            throw std::runtime_error("unable to allocate packet");
        }

        rval = avcodec_receive_packet(codec_context_.get(), pkt.get());
        if (rval == AVERROR(EAGAIN) || rval == AVERROR_EOF) {
            return;
        } else if (rval < 0) {
            throw std::runtime_error("error during encoding");
        }

        // write packet
        av_interleaved_write_frame(format_context_.get(), pkt.get());
    }
}


