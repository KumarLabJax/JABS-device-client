#include <chrono>
#include <ctime>
#include <fstream>
#include <thread>
#include <iomanip>
#include <iostream>

#include "pylon_camera.h"
#include "video_writer.h"

namespace chrono = std::chrono;
using namespace Pylon;
using namespace GenApi;


void PylonCameraController::RecordVideo(RecordingSessionConfig config)
{


    recording_err_state_ = 0;

    //std::unique_ptr<uint8_t> frame_data(new uint8_t[config.frame_width * config.frame_height]);

    std::string filename = config.file_prefix;
    std::string timestamp_filename = config.file_prefix;
    std::string timestamp_start_filename = config.file_prefix;

    int next_hour;
    size_t current_frame = 0;   // frame number in the current file
    size_t frames_captured = 0; // total number of frames captured in session
    uint64_t first_click;
    uint64_t last_click = 0;
    double current_fps;

    timestamp_filename.append("_timestamps.txt");
    timestamp_start_filename.append("_start_timestamp.txt");

    // open files
    std::ofstream timestamp_file (timestamp_filename, std::ofstream::out);
    std::ofstream timestamp_start_file (timestamp_start_filename, std::ofstream::out);

    timestamp_file << std::fixed << std::setprecision(6);


    PylonAutoInitTerm autoInitTerm;
    CImageFormatConverter img_converter;
    CGrabResultPtr ptrGrabResult;
    CBaslerGigEInstantCamera camera;

    CameraConfiguration customConfig(config.frame_width, config.frame_height, config.target_fps, config.pixel_format, "Once", false);

    try {
        camera.Attach(CTlFactory::GetInstance().CreateFirstDevice());
        camera.RegisterConfiguration(&customConfig, RegistrationMode_ReplaceAll, Cleanup_None);
        camera.MaxNumBuffer = 15;
        camera.Open();
    } catch (const GenericException &e) {
        recording_ = false;

        // set some member variables so the controlling thread knows what's going on
        err_msg_ = "unable to configure camera: " + std::string(e.what());
        std::cerr << err_msg_ << std::endl;
        recording_err_state_ = 1;
        return;
    }

    // save the start time of the recording session
    auto start_time = chrono::system_clock::now();
    std::time_t t = chrono::system_clock::to_time_t(start_time);
    session_start_.store(start_time.time_since_epoch());
    timestamp_start_file << "Recording started at Local Time: " << std::ctime(&t);

    filename = MakeFilePath(start_time, config.file_prefix);
    if (config.fragment_by_hour) {
        next_hour = (GetCurrentHour(start_time) + 1) % 24;
        filename.append("_" + timestamp(start_time));
    }

    VideoWriter video_writer(filename, config.frame_height, config.frame_width, config.target_fps, config.pixel_format,
        config.codec, config.compression_target, config.lossless, std::to_string(config.compression), config.apply_filter);

    // start grabbing frames
    camera.StartGrabbing(GrabStrategy_OneByOne);
    // main acquisition loop
    while(1) {
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::system_clock::now().time_since_epoch() - session_start_.load());

        // check to see if we've completed the specified duration or we've been told
        // to terminate early
        if (this->stop_recording_ || elapsed >= config.duration) {
            break;
        }

        // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
        try {
            camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);
        } catch (const GenericException &e) {
            //bailing out -- should we retry?
            err_msg_ = "Timeout retrieving frame: " + std::string(e.what());
            recording_err_state_ = 1;
            break;
        }

        if (!ptrGrabResult->GrabSucceeded()) {
            std::cerr << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << std::endl;
            continue;
        }

        auto pImageBuffer = (uint8_t *)ptrGrabResult->GetBuffer();
        auto frame_timestamp = ptrGrabResult->GetTimeStamp();

        if (frames_captured == 0) {
            first_click = frame_timestamp;
        }

        current_fps = 1.0/((frame_timestamp - last_click)/125000000.0);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (moving_avg_.size() == moving_avg_.capacity())
                moving_avg_.pop_back();
            moving_avg_.insert(moving_avg_.begin(), current_fps);
            last_click = frame_timestamp;
        }

        // NOTE: Camera tick is measured at 125MHz, so divide by 125,000,000 to get the value in seconds
        timestamp_file << (frame_timestamp - first_click) / 125000000.0 << std::endl;

        // send frame to the encoder
        video_writer.EncodeFrame(pImageBuffer, current_frame, config.apply_filter);

        current_frame++;
        frames_captured++;

        // check to see if we need to roll over to a new file
        if (config.fragment_by_hour && GetCurrentHour() == next_hour) {
            start_time = chrono::system_clock::now();
            filename = MakeFilePath(start_time, config.file_prefix);
            filename.append("_" + timestamp(start_time));

            // tell video_writer to write to the new file
            if (video_writer.RollFile(filename) !=0 ) {
                err_msg_ = "Unable to setup next video";
                recording_err_state_ = 1;
                break;
            }

            next_hour = (GetCurrentHour(start_time) + 1) % 24;
            current_frame = 0;
        }

        // This is auto-applied when leaving scope, but can't hurt
        ptrGrabResult.Release();
    }


    camera.StopGrabbing();
    camera.DeregisterConfiguration(&customConfig);
    camera.Close();

    // normal exit
    elapsed_time_ = chrono::duration_cast<chrono::seconds>(
        chrono::system_clock::now().time_since_epoch() - session_start_.load());
    recording_ = false;
}

PylonCameraController::CameraConfiguration::CameraConfiguration(
    int frame_width, int frame_height, int target_fps, std::string pixel_format,
    std::string auto_gain, bool enable_pgi)
{
    frame_width_ = frame_width;
    frame_height_ = frame_height;
    target_fps_ = target_fps;

    // Only change if it matches "Mono12"
    if (pixel_format == "Mono12") {
        pixel_format_ = "Mono12";
    } else {
        // Both YUV420P and Mono8 get Mono8 from the camera
        pixel_format_ = "Mono8";
    }

    if (auto_gain == "Continuous") {
        auto_gain_ = "Continuous";
    } else {
        auto_gain_ = "Once";
    }

    enable_pgi_ = enable_pgi;
}

void PylonCameraController::CameraConfiguration::OnOpened(CInstantCamera &camera)
{
    try {
        // Get the camera control object.
        INodeMap &control = camera.GetNodeMap();

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
        if (camera.IsGigE()) {
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->AutoFunctionAOISelector.SetValue(
                Basler_GigECameraParams::AutoFunctionAOISelector_AOI1);

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

            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->AutoFunctionAOISelector.SetValue(
                Basler_GigECameraParams::AutoFunctionAOISelector_AOI2);

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
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->ExposureTimeRaw.SetValue(15000);

            // Set autogain
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->ExposureAuto.SetValue(
                Basler_GigECameraParams::ExposureAuto_Off);
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->GainAuto.SetValue(
                Basler_GigECameraParams::GainAuto_Once);

            // Stream parameters (for more efficient gige communication)
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->GevStreamChannelSelector.SetValue(
                Basler_GigECameraParams::GevStreamChannelSelector_StreamChannel0);
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->GevSCPD.SetValue(0);
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->GevSCFTD.SetValue(0);
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->GevSCBWR.SetValue(5);
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->GevSCBWRA.SetValue(2);
            dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->GevSCPSPacketSize.SetValue(9000);

            // PGI mode stuff
            if (enable_pgi_) {
                dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->PgiMode.SetValue(
                    Basler_GigECameraParams::PgiMode_On);
                dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->NoiseReductionRaw.SetValue(10);
                dynamic_cast<CBaslerGigEInstantCamera *>(&camera)->SharpnessEnhancementRaw.SetValue(100);
            }
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





