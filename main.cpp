/**
 * @brief device client for the Kumar Lab Long-Term Monitoring system (LTMS)
 *
 * This program implements a service that is run continuously (through systemd)
 * on the video acquisition computers. It continuously monitors the system,
 * gathering info on disk space consumption, system load, memory utilization,
 * etc and then sends that information to the LTMS webservice.
 * It will also perform video acquisition on request from the webservice.
 *
 * @author glen.beane@jax.org
 */

// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#include <atomic>
#include <chrono>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <thread>

#include <systemd/sd-daemon.h>

#include "external/inih/INIReader.h"
#include "status_update.h"
#include "system_info.h"
#include "ltm_exceptions.h"
#include "pylon_camera.h"
#include "server_command.h"

// default update interval (in seconds) if it isn't specified in the config file
const unsigned int kDefaultSleep = 30;

const int kDefaultFrameWidth = 800;
const int kDefaultFrameHeight = 800;

// used to notify the program that a HUP signal was received
std::atomic_bool hup_received = { false };

void signalHandler( int signum ) {
    switch (signum) {
        case SIGHUP:
            hup_received = true;
            break;
        default:
            throw NotImplementedException();
    }
}

/* read a configuration file and set configuration variables
 *
 * @param config_path string containing the path to the configuration file
 * @param sleep_time modified to contain the "update_interval" param from the config file
 * @param video_dir modified to contain the path to the directory to store video files in
 * @param api_uri modified to contain the uri of the REST API
 *
 * @return zero on success, the value of INIReader ParseError() otherwise
 */
int setConfig(std::string config_path,
              std::chrono::seconds &sleep_time,
              std::string &video_dir, 
              std::string &api_uri,
              int &frame_width,
              int &frame_height)
{
    INIReader ini_reader(config_path);
    
    if (ini_reader.ParseError() != 0) {
        return ini_reader.ParseError();
    }
    
    sleep_time = std::chrono::seconds(ini_reader.GetInteger("app", "update_interval", kDefaultSleep));
    video_dir = ini_reader.Get("disk", "video_capture_dir", "/tmp");
    api_uri = ini_reader.Get("app", "api", "");
    frame_width = ini_reader.GetInteger("video", "frame_width", kDefaultFrameWidth);
    frame_height = ini_reader.GetInteger("video", "frame_height", kDefaultFrameHeight);

    return 0;
}

/**
 * @brief program main
 *
 * program takes one argument, which is required '--config <path_to_config_file>'
 *
 * @param argc count of program arguments
 * @param argv array of program arguments
 * @return 0 on success, positive integer otherwise
 */
int main(int argc, char **argv)
{
   
    std::string config_path;         ///< path to configuration file, will be passed as a program argument
    std::string video_capture_dir;   ///< path to video capture directory, will be set from a config file
    std::string api_uri;             ///< URI for webservice API
    std::chrono::seconds sleep_time; ///< time to wait between status update calls to API, in seconds
    SysInfo system_info;             ///< information about the host system (memory, disk, load)
    int rval;                        ///< used to check return value of some functions
    bool short_sleep;                ///< indicates that we don't want to sleep full amount before next iteration

    int frame_width;              ///< frame width from config file
    int frame_height;             ///< frame height from config file
    
    // setup a signal handler to catch HUP signals which indicate that the
    // config file should be reloaded
    signal(SIGHUP, signalHandler); 
    
    // parse command line options
    static struct option long_options[] = {
            {"config",  required_argument, 0, 'c'},
            {0, 0, 0, 0}
    };
    int option_index = 0;
    int c;

    while((c = getopt_long (argc, argv, "c:", long_options, &option_index)) != -1 )
    {
        switch(c)
        {
            case 'c':
                config_path = std::string(optarg);
                break;
        }
    }


    
    if (config_path.empty()) {
        std::clog << SD_ERR << "--config <CONFIG_FILE_PATH> is a required argument\n";
        return 1;
    }


    rval = setConfig(config_path, sleep_time, video_capture_dir, api_uri, frame_width, frame_height);
    if (rval > 0) {
        std::clog << SD_ERR << "Error parsing config file" << std::endl;
        return 1;
    } else if (rval == -1) {
        std::clog << SD_ERR << "Unable to open config file: "
                  << std::strerror(errno)<< std::endl;
        return 1;
    }

    
    if (api_uri.empty()) {
        std::clog << SD_ERR << "'api' is a required config file parameter\n";
        return 1;
    }

    if (frame_height <= 0 || frame_width<= 0) {
        std::clog << SD_ERR << "invalid frame dimensions\n";
        return 1;
    }
    
    try {
         system_info.AddMount(video_capture_dir);
    }
    catch (DiskRegistrationException &e) {
        std::clog << SD_ERR << e.what() << std::endl;
        sd_notifyf(0, "STATUS=Failed to register mount");
        return 1;
    }

    PylonCameraController camera_controller(video_capture_dir, frame_width, frame_height);
    
    // notify systemd that we're done initializing
    sd_notify(0, "READY=1");
    
    // main loop
    while (1) {
        short_sleep = false;
    
        // if we've received a HUP signal, and we aren't busy recording then
        // reload the configuration file
        if (hup_received && !camera_controller.recording()) {
            system_info.ClearMounts();
            rval = setConfig(config_path, sleep_time, video_capture_dir, api_uri, frame_width, frame_height);
            if (rval > 0) {
                std::clog << SD_ERR << "Error parsing config file during reload" << std::endl;
                return 1;
            } else if (rval == -1) {
                std::clog << SD_ERR << "Unable to open config file during reload: "
                          << std::strerror(errno)<< std::endl;
                return 1;
            }
            
            try {
                system_info.AddMount(video_capture_dir);
            }
            catch (DiskRegistrationException &e) {
                std::clog << SD_ERR << e.what() << std::endl;
                return 1;
            }
            camera_controller.SetDirectory(video_capture_dir);
            camera_controller.SetFrameHeight(frame_height);
            camera_controller.SetFrameWidth(frame_width);

            hup_received = false;
        }
    
        // gather updated system information
        system_info.Sample(); 
        
        // send updated status to the server
        ServerCommand* svr_command = send_status_update(system_info, camera_controller, api_uri);

        switch (svr_command->command()) {
            case CommandTypes::NOOP:
                std::clog << SD_DEBUG << "NOOP" << std::endl;
                break;
            case CommandTypes::START_RECORDING:
            {
                std::clog << SD_DEBUG << "START_RECORDING" << std::endl;

                // cast the svr_command pointer to a RecordCommand* so we can
                // access the parameters() method.
                RecordingParameters recording_parameters = static_cast<RecordCommand*>(svr_command)->parameters();

                // setup recording session configuration
                PylonCameraController::RecordingSessionConfig config;
                config.set_file_prefix(recording_parameters.file_prefix);
                config.set_duration(std::chrono::seconds(recording_parameters.duration));
                config.set_fragment_by_hour(recording_parameters.fragment_hourly);
                config.set_session_id(recording_parameters.session_id);

                camera_controller.StartRecording(config);
                short_sleep = true;
                break;
            }
            case CommandTypes::STOP_RECORDING:
                std::clog << SD_DEBUG << "STOP_RECORDING" << std::endl;
                camera_controller.StopRecording();
                short_sleep = true;
                break;
            case CommandTypes::COMPLETE:
                std::clog << SD_DEBUG << "COMPLETE" << std::endl;
                camera_controller.ClearSession();
                short_sleep = true;
                break;
            case CommandTypes::UNKNOWN:
                std::clog << SD_ERR << "Server responded with unknown command" << std::endl;
                break;
        }

        free(svr_command);

        // sleep until next iteration. if we are actively working commands,
        // don't sleep very long
        if (short_sleep) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
            std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
        }

    }
    return 0;
}