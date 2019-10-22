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

// default update interval (in seconds) if it isn't specified in the config file
const unsigned int kDefaultSleep = 30;

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
              std::string &api_uri)
{
    INIReader ini_reader(config_path);
    
    if (ini_reader.ParseError() != 0) {
        return ini_reader.ParseError();
    }
    
    sleep_time = std::chrono::seconds(ini_reader.GetInteger("app", "update_interval", kDefaultSleep));
    video_dir = ini_reader.Get("disk", "video_capture_dir", "/tmp");
    api_uri = ini_reader.Get("app", "api", "");
            
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
   
    std::string config_path;         ///path to configuration file, will be passed as a program argument
    std::string video_capture_dir;   ///path to video capture directory, will be set from a config file
    std::string api_uri;             ///URI for webservice API
    std::chrono::seconds sleep_time; ///time to wait between status update calls to API, in seconds
    SysInfo system_info;             ///information about the host system (memory, disk, load)
    int rval;                        ///used to check return value of some functions
    
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


    rval = setConfig(config_path, sleep_time, video_capture_dir, api_uri);
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
    
    try {
         system_info.AddMount(video_capture_dir);
    }
    catch (DiskRegistrationException &e) {
        std::clog << SD_ERR << e.what() << std::endl;
        sd_notifyf(0, "STATUS=Failed to register mount");
        return 1;
    }

    PylonCameraController camera_controller(video_capture_dir);
    
    // notify systemd that we're done initializing
    sd_notify(0, "READY=1");
    
    // main loop
    while (1) {
    
        // if we've received a HUP signal then reload the configuration file
        if (hup_received) {
            system_info.ClearMounts();
            rval = setConfig(config_path, sleep_time, video_capture_dir, api_uri);
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
            hup_received = false;
        }
    
        // gather updated system information
        system_info.Sample(); 
        
        // send updated status to the server
        send_status_update(system_info, api_uri);
        
        // sleep until next iteration
        std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
    }
    
    return 0;
}