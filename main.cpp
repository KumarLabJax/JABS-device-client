#include <iostream>
#include <iomanip>
#include <csignal>
#include <chrono>
#include <thread>
#include <getopt.h>

#include <sysexits.h>
#include <systemd/sd-daemon.h>

#include "system_info.h"
#include "external/inih/INIReader.h"
#include "status_update.h"

// default update interval (in seconds) if it isn't specified in the config file
const unsigned int default_sleep = 30;

// used to notify the program that a HUP signal was received
static volatile sig_atomic_t hup_received = 0;

void signalHandler( int signum ) {
    switch (signum) {
        case SIGHUP:
            hup_received = 1;
            break;
        default:
            exit(signum);
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
              unsigned int &sleep_time,
              std::string &video_dir, 
              std::string &api_uri)
{
    INIReader ini_reader(config_path);
    
    if (ini_reader.ParseError() != 0) {
        return ini_reader.ParseError();
    }
    
    sleep_time = ini_reader.GetInteger("app", "update_interval", default_sleep);
    video_dir = ini_reader.Get("disk", "video_capture_dir", "/tmp");
    api_uri = ini_reader.Get("app", "api", "");
            
    return 0;
}

int main(int argc, char **argv)
{
   
    std::string config_path;
    std::string video_capture_dir;
    std::string api_uri;
    unsigned int sleep_time;
    SysInfo system_info;
    
    // setup a signal handler to catch HUP signals which indicate that the config file
    // should be reloaded
    signal(SIGHUP, signalHandler); 
    
    
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
    
    if (setConfig(config_path, sleep_time, video_capture_dir, api_uri) != 0) {
        std::clog << SD_ERR << "Error parsing config file" << std::endl;
        return 1;
    }   
    
    if (api_uri.empty()) {
        std::clog << SD_ERR << "'api' is a required config file parameter\n";
        return 1;
    } 
    
    try {
         system_info.register_mount(video_capture_dir);
    }
    catch (DiskRegistrationException &e) {
        std::clog << SD_ERR << e.what() << std::endl;
        sd_notifyf(0, "STATUS=Failed to register mount");
        exit(EX_CONFIG);
    }
    
    // notify systemd that we're done initializing
    sd_notify(0, "READY=1");
    
    // main loop
    while (1) {
    
        // if we've received a HUP signal then reload the configuration file
        if (hup_received) {
            system_info.clear_mounts();
            if (setConfig(config_path, sleep_time, video_capture_dir, api_uri) != 0) {
                std::clog << SD_ERR << "Error parsing config file" << std::endl;
                sd_notifyf(0, "STATUS=Failed to register mount");
                return 1;
            }
            
            
            try {
                system_info.register_mount(video_capture_dir);
            }
            catch (DiskRegistrationException &e) {
                std::clog << SD_ERR << e.what() << std::endl;
                sd_notifyf(0, "STATUS=Failed to register mount");
                exit(EX_CONFIG);
            }
            
            hup_received = 0;
        }
    
        // gather updated system information
        system_info.sample(); 
        
        // send updated status to the server
        send_status_update(system_info, api_uri);
        
        // sleep until next interation
        std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
    }
    
    return 0;
}