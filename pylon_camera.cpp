// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#include <chrono>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include "pylon_camera.h"
#include "video_writer.h"

namespace chrono = std::chrono;
using namespace Pylon;
using namespace GenApi;


void PylonCameraController::RecordVideo(const RecordingSessionConfig &config)
{
    // reset the CameraController err_state_
    // this is set to let the controlling thread know that we encountered an error
    err_state_ = 0;

    std::string filename;   // output video filename
    std::string output_dir; // output directory

    // if config.fragment_by_hour is true, current hour == next_hour triggers
    // rolling over to a new file
    int next_hour = 0;

    size_t current_frame = 0;   // frame number in the current file
    size_t frames_captured = 0; // total number of frames captured in session
    uint64_t first_click = 0;   // timestamp of first frame captured
    uint64_t last_click = 0;    // timestamp of last frame captured
    double current_fps;         // current acquisition framerate

    // setup the output directory
    try {
        output_dir = MakeOutputDir(chrono::system_clock::now());
    } catch (const std::runtime_error &e) {
        // couldn't setup the file path. set error string and return
        recording_ = false;
        err_msg_ = "unable to setup output dir: " + std::string(e.what());
        err_state_ = 1;
        return;
    }

    // setup filenames for timestamp files
    // file for storing timestamp of each frame
    std::string timestamp_filename = output_dir + config.file_prefix() + "timestamps.txt";
    // file for storing timestamp of recording session start
    std::string timestamp_start_filename = output_dir + config.file_prefix() + "start_timestamp.txt";

    // open files
    std::ofstream timestamp_file (timestamp_filename, std::ofstream::out);
    std::ofstream timestamp_start_file (timestamp_start_filename, std::ofstream::out);

    // terminate recording session if we were unable to open either file
    if (!timestamp_file || ! timestamp_start_file) {
        err_state_ = 1;
        err_msg_ = "error opening timestamp files";
        recording_ = false;
        return;
    }

    // set format for floating point output to the timestamp file
    timestamp_file << std::fixed << std::setprecision(6);

    // attach and configure the camera
    PylonAutoInitTerm autoInitTerm;
    CImageFormatConverter img_converter;
    CGrabResultPtr ptrGrabResult;
    CBaslerGigEInstantCamera camera;

    try {
        camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
        // customConfig will be managed by the Basler API so we are not using a smart pointer
        CameraConfiguration *customConfig = new CameraConfiguration(frame_width_, frame_height_,
                                                                    config.target_fps(), config.pixel_format(), false);
        camera.RegisterConfiguration(customConfig, RegistrationMode_ReplaceAll, Cleanup_Delete);
        camera.MaxNumBuffer = 15;
        camera.Open();
    } catch (const GenericException &e) {
        // couldn't attach to or configure the camera. set error string and return
        recording_ = false;
        err_msg_ = "unable to configure camera: " + std::string(e.what());
        err_state_ = 1;
        return;
    }
    // done configuring camera

    // save the start time of the recording session
    auto start_time = chrono::system_clock::now();
    std::time_t t = chrono::system_clock::to_time_t(start_time);
    session_start_.store(start_time.time_since_epoch());
    timestamp_start_file << "Recording started at Local Time: " << std::ctime(&t);
    timestamp_start_file.close();

    if (config.fragment_by_hour()) {
        next_hour = (GetCurrentHour(start_time) + 1) % 24;
        filename = output_dir + config.file_prefix() + timestamp(start_time);
    } else {
        filename = output_dir + config.file_prefix();
    }

    VideoWriter video_writer(filename, frame_width_, frame_height_, config);

    // camera is configured and we're ready to start capturing video
    // start grabbing frames
    camera.StartGrabbing(GrabStrategy_OneByOne);

    // main recording loop
    while(1) {
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::system_clock::now().time_since_epoch() - session_start_.load());

        // check to see if we've completed the specified duration or we've been told
        // to terminate early
        if (this->stop_recording_ || elapsed >= config.duration()) {
            break;
        }

        // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
        try {
            camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
        } catch (const GenericException &e) {
            // bailing out -- should we retry?
            err_msg_ = "Timeout retrieving frame: " + std::string(e.what());
            err_state_ = 1;
            break;
        }

        if (!ptrGrabResult->GrabSucceeded()) {
            std::cerr << "Error: " << ptrGrabResult->GetErrorCode()
                      << " " << ptrGrabResult->GetErrorDescription() << std::endl;
            continue;
        }

        // got a frame from the camera
        // get a pointer to the image buffer and
        auto pImageBuffer = (uint8_t *)ptrGrabResult->GetBuffer();
        // get the timestamp of the frame
        auto frame_timestamp = ptrGrabResult->GetTimeStamp();

        // record timestamp of first frame
        if (frames_captured == 0) {
            first_click = frame_timestamp;
        }

        // calculate current framerate and add to the vector used for computing
        // the moving average
        current_fps = 1.0/((frame_timestamp - last_click)/125000000.0);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (moving_avg_.size() == moving_avg_.capacity())
                moving_avg_.pop_back();
            moving_avg_.insert(moving_avg_.begin(), current_fps);
            last_click = frame_timestamp;
        }

        // record timestamp of current frame, as an offset from the first timestamp
        // NOTE: Camera tick is measured at 125MHz, so divide by 125,000,000 to get the value in seconds
        timestamp_file << (frame_timestamp - first_click) / 125000000.0 << std::endl;

        // send frame to the encoder
        video_writer.EncodeFrame(pImageBuffer, current_frame);

        current_frame++;
        frames_captured++;

        // check to see if we need to roll over to a new file
        if (config.fragment_by_hour() && GetCurrentHour() == next_hour) {
            start_time = chrono::system_clock::now();
            filename = output_dir + config.file_prefix() + timestamp(start_time);

            // setup a VideoWriter to the new filename:
            video_writer = VideoWriter(filename, frame_width_, frame_height_, config);

            next_hour = (GetCurrentHour(start_time) + 1) % 24;
            current_frame = 0;
        }

        ptrGrabResult.Release();
    }

    // out of acquisition loop, stop grabbing frames and shutdown the camera
    camera.StopGrabbing();
    camera.Close();

    elapsed_time_ = chrono::duration_cast<chrono::seconds>(
        chrono::system_clock::now().time_since_epoch() - session_start_.load());
    recording_ = false;
}

PylonCameraController::CameraConfiguration::CameraConfiguration(
    int frame_width, int frame_height, int target_fps,
    const std::string& pixel_format, bool enable_pgi)
{
    frame_width_ = frame_width;
    frame_height_ = frame_height;
    target_fps_ = target_fps;

    // for YUV420P we get Mono8 off the camera
    if (pixel_format == pixel_types::YUV420P) {
        pixel_format_ = pixel_types::MONO8;
    } else {
        // pixel_format should have been validated by the time we get here
        pixel_format_ = pixel_format;
    }

    enable_pgi_ = enable_pgi;
}

void PylonCameraController::CameraConfiguration::OnOpened(CInstantCamera &camera)
{
    try {
        // Get the camera control object.
        INodeMap &control = camera.GetNodeMap();

        if (!camera.IsGigE()) {
            throw RUNTIME_EXCEPTION("Could not apply configuration: Only GigE cameras are currently supported.");
        }

        CBaslerGigEInstantCamera *gige_camera = dynamic_cast<CBaslerGigEInstantCamera *>(&camera);


        // Get the parameters for setting the image area of interest (Image AOI).
        const CIntegerPtr width = control.GetNode("Width");
        const CIntegerPtr height = control.GetNode("Height");
        const CIntegerPtr offset_x = control.GetNode("OffsetX");
        const CIntegerPtr offset_y = control.GetNode("OffsetY");
        const CIntegerPtr auto_roi_width = control.GetNode("AutoFunctionAOIWidth");
        const CIntegerPtr auto_roi_height = control.GetNode("AutoFunctionAOIHeight");
        const CIntegerPtr auto_roi_offset_x = control.GetNode("AutoFunctionAOIOffsetX");
        const CIntegerPtr auto_roi_offset_y = control.GetNode("AutoFunctionAOIOffsetY");

        // Set the AOI.
        if (IsWritable(offset_x)) {
            offset_x->SetValue(offset_x->GetMin());
        }

        if (IsWritable(offset_y)) {
            offset_y->SetValue(offset_y->GetMin());
        }

        // Assign frame width/height
        width->SetValue(frame_width_);
        height->SetValue(frame_height_);

        // Re-center, the GetMax is already shifted given the width/height
        if (IsWritable(offset_x)) {
            offset_x->SetValue(int(offset_x->GetMax() / 2));
        }
        if (IsWritable(offset_y)) {
            offset_y->SetValue(int(offset_y->GetMax() / 2));
        }

        // Set the pixel data format.
        CEnumerationPtr(control.GetNode("PixelFormat"))->FromString(pixel_format_.c_str());

        CEnumerationPtr(control.GetNode("ShutterMode"))->FromString("Global");

        gige_camera->AutoFunctionAOISelector.SetValue(Basler_GigECameraParams::AutoFunctionAOISelector_AOI1);

        // Assign the auto-gain to only look at the RoI assigned for exposure balancing
        //camera.AutoFunctionAOISelector.SetValue(AutoFunctionAOISelector_AOI1);
        if (IsWritable(auto_roi_offset_x)) {
            auto_roi_offset_x->SetValue(auto_roi_offset_x->GetMin());
        }
        if (IsWritable(auto_roi_offset_y)) {
            auto_roi_offset_y->SetValue(auto_roi_offset_y->GetMin());
        }

        // Assign frame width/height
        auto_roi_width->SetValue(frame_width_);
        auto_roi_height->SetValue(frame_height_);

        // Re-center, the GetMax is already shifted given the width/height
        if (IsWritable(auto_roi_offset_x)) {
            auto_roi_offset_x->SetValue(int(auto_roi_offset_x->GetMax() / 2));
        }

        if (IsWritable(auto_roi_offset_y)) {
            auto_roi_offset_y->SetValue(int(auto_roi_offset_y->GetMax() / 2));
        }

        gige_camera->AutoFunctionAOISelector.SetValue(Basler_GigECameraParams::AutoFunctionAOISelector_AOI2);

        // Assign the auto-gain to only look at the RoI assigned for white balancing
        if (IsWritable(auto_roi_offset_x)) {
            auto_roi_offset_x->SetValue(auto_roi_offset_x->GetMin());
        }

        if (IsWritable(auto_roi_offset_y)) {
            auto_roi_offset_y->SetValue(auto_roi_offset_y->GetMin());
        }

        // Assign frame width/height
        auto_roi_width->SetValue(frame_width_);
        auto_roi_height->SetValue(frame_height_);

        // Re-center, the GetMax is already shifted given the width/height
        if (IsWritable(auto_roi_offset_x)) {
            auto_roi_offset_x->SetValue(int(auto_roi_offset_x->GetMax() / 2));
        }

        if (IsWritable(auto_roi_offset_y)) {
            auto_roi_offset_y->SetValue(int(auto_roi_offset_y->GetMax() / 2));
        }

        // Enforce a 15ms exposure time manually
        gige_camera->ExposureTimeRaw.SetValue(15000);

        // Set autogain
        gige_camera->ExposureAuto.SetValue(Basler_GigECameraParams::ExposureAuto_Off);
        gige_camera->GainAuto.SetValue( Basler_GigECameraParams::GainAuto_Once);

        // Stream parameters (for more efficient gige communication)
        gige_camera->GevStreamChannelSelector.SetValue(Basler_GigECameraParams::GevStreamChannelSelector_StreamChannel0);
        gige_camera->GevSCPD.SetValue(0);
        gige_camera->GevSCFTD.SetValue(0);
        gige_camera->GevSCBWR.SetValue(5);
        gige_camera->GevSCBWRA.SetValue(2);
        gige_camera->GevSCPSPacketSize.SetValue(9000);

        // PGI mode stuff
        if (enable_pgi_) {
            gige_camera->PgiMode.SetValue(Basler_GigECameraParams::PgiMode_On);
            gige_camera->NoiseReductionRaw.SetValue(10);
            gige_camera->SharpnessEnhancementRaw.SetValue(100);
        }

        // Framerate items
        CBooleanPtr(control.GetNode("AcquisitionFrameRateEnable"))->SetValue(true);
        CFloatPtr(control.GetNode("AcquisitionFrameRateAbs"))->SetValue(target_fps_);
    }
    catch (const GenericException &e) {
        throw RUNTIME_EXCEPTION(
            "Could not apply configuration. const GenericException caught in OnOpened method msg=%hs", e.what());
    }
}





