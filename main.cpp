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

struct AppConfig
{
    std::string output_dir;  ///< path to video capture directory
    std::string api_uri;     ///< URI for webservice API
    std::string rtmp_uri;    ///< URI for rtmp publishing endpoint
    std::string location;    ///< device location string
    int frame_width;         ///< frame width
    int frame_height;        ///< frame height
    std::chrono::seconds sleep_time; ///< time to wait between status update calls to API, in seconds
};

// default update interval (in seconds) if it isn't specified in the config file
const unsigned int kDefaultSleep = 30;

// default frame dimensions
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
 *
 * @return AppConfig struct containing config parameters read from file
 */
AppConfig readConfig(std::string config_path)
{
    AppConfig config;
    INIReader ini_reader(config_path);
    
    if (ini_reader.ParseError() != 0) {
        throw std::runtime_error("Unable to parse config file");
    }
    
    config.sleep_time = std::chrono::seconds(ini_reader.GetInteger("app", "update_interval", kDefaultSleep));
    config.output_dir = ini_reader.Get("disk", "video_capture_dir", "/tmp");
    config.api_uri = ini_reader.Get("app", "api", "");
    config.rtmp_uri = ini_reader.Get("streaming", "rtmp", "");

    // TODO: consider making these required config parameters and remove defaults
    config.frame_width = ini_reader.GetInteger("video", "frame_width", kDefaultFrameWidth);
    config.frame_height = ini_reader.GetInteger("video", "frame_height", kDefaultFrameHeight);

    config.location = ini_reader.Get("app", "location", "");

    return config;
}

std::string getNvBoardString(std::string hostname, std::string location)
{
    static const std::string not_allowed_chars = "().?'\"[]{}<>;*&^$#@!`~|\t\n%";
    std::string board_string;

    size_t last_index = hostname.find_last_not_of("0123456789");
    int nv_num = 0;
    if (last_index < hostname.length()) {
        nv_num = std::stoi(hostname.substr(last_index + 1));
    }

    board_string = "NV" + std::to_string(nv_num) + "-" + location;

    for (unsigned int i = 0; i < not_allowed_chars.length(); ++i)
    {
        board_string.erase(std::remove(board_string.begin(), board_string.end(), not_allowed_chars.at(i)), board_string.end());
    }

    return board_string;
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
   
    AppConfig appConfig;     ///< app configuration, loaded from config file
    std::string config_path; ///< path to configuration file, will be passed as a program argument

    SysInfo system_info;     ///< information about the host system (memory, disk, load)
    bool short_sleep;        ///< indicates that we don't want to sleep full amount before next iteration

    std::string nv_room_string;

    
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


    try {
        appConfig = readConfig(config_path);
    } catch (const std::runtime_error& error) {
        std::clog << SD_ERR << "Unable to read config file\n";
        if (errno > 0) {
            std::clog <<  std::strerror(errno) << std::endl;
        }
        return 1;
    }

    
    if (appConfig.api_uri.empty()) {
        std::clog << SD_ERR << "'api' is a required config file parameter\n";
        return 1;
    }

    if (appConfig.frame_height <= 0 || appConfig.frame_width<= 0) {
        std::clog << SD_ERR << "invalid frame dimensions\n";
        return 1;
    }
    
    try {
         system_info.AddMount(appConfig.output_dir);
    }
    catch (DiskRegistrationException &e) {
        std::clog << SD_ERR << e.what() << std::endl;
        sd_notifyf(0, "STATUS=Failed to register mount");
        return 1;
    }

    nv_room_string = getNvBoardString(system_info.hostname(), appConfig.location);

    PylonCameraController camera_controller(appConfig.output_dir, appConfig.frame_width, appConfig.frame_height, nv_room_string);
    
    // notify systemd that we're done initializing
    sd_notify(0, "READY=1");
    
    // main loop
    while (1) {
        short_sleep = false;
    
        // if we've received a HUP signal, and we aren't busy recording then
        // reload the configuration file
        if (hup_received && !camera_controller.recording()) {
            system_info.ClearMounts();
            try {
                appConfig = readConfig(config_path);
                nv_room_string = getNvBoardString(system_info.hostname(), appConfig.location);
            } catch (const std::runtime_error& error) {
                std::clog << SD_ERR << "Unable to read config file during reload\n";
                if (errno > 0) {
                    std::clog <<  std::strerror(errno) << std::endl;
                }
                return 1;
            }

            try {
                system_info.AddMount(appConfig.output_dir);
            }
            catch (DiskRegistrationException &e) {
                std::clog << SD_ERR << e.what() << std::endl;
                return 1;
            }
            camera_controller.SetDirectory(appConfig.output_dir);
            camera_controller.SetFrameHeight(appConfig.frame_height);
            camera_controller.SetFrameWidth(appConfig.frame_width);
            camera_controller.SetNvRoomString(nv_room_string);

            hup_received = false;
        }
    
        // gather updated system information
        system_info.Sample(); 
        
        // send updated status to the server
        ServerCommand* svr_command = send_status_update(system_info, camera_controller, appConfig.api_uri, appConfig.location);

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
                if (!recording_parameters.file_prefix.empty()) {
                    config.set_file_prefix(recording_parameters.file_prefix);
                } else {
                    config.set_file_prefix(system_info.hostname());
                }
                config.set_duration(std::chrono::seconds(recording_parameters.duration));
                config.set_fragment_by_hour(recording_parameters.fragment_hourly);
                config.set_session_id(recording_parameters.session_id);
                config.set_target_fps(recording_parameters.target_fps);
                config.set_apply_filter(recording_parameters.apply_filter);

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
            std::this_thread::sleep_for(std::chrono::seconds(appConfig.sleep_time));
        }

    }
    return 0;
}