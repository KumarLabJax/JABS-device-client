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

int setConfig(std::string config_path, unsigned int &sleep_time, std::string &video_dir)
{
    INIReader ini_reader(config_path);
    
    if (ini_reader.ParseError() != 0) {
        return ini_reader.ParseError();
    }
    
    sleep_time = ini_reader.GetInteger("app", "update_interval", 30);
    video_dir = ini_reader.Get("disk", "video_capture_dir", "/tmp");
    
    std::cout << "sleep_time " << sleep_time << std::endl;
    std::cout << "video_dir " << video_dir << std::endl;
    
    return 0;
}

int main(int argc, char **argv)
{
   
    std::string config_path;
    std::string video_capture_dir;
    unsigned int sleep_time = 30;
    SysInfo system_info;
    
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
        std::cerr << "--config <CONFIG_FILE_PATH> is a required argument\n";
        return 1;
    }
    
    if (setConfig(config_path, sleep_time, video_capture_dir) != 0) {
        std::cout << "Error parsing config file" << std::endl;
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
    
    sd_notify(0, "READY=1");
    
    while (1) {
    
        // if we've received a HUP signal then reload the configuration file
        if (hup_received) {
            system_info.clear_mounts();
            if (setConfig(config_path, sleep_time, video_capture_dir) != 0) {
                std::cout << "Error parsing config file" << std::endl;
                return 1;
            }
            system_info.register_mount(video_capture_dir);
            hup_received = 0;
        }
    
        system_info.sample();
        
        std::cout << "Uptime: " << system_info.get_uptime() << std::endl;
        
        std::cout << "MemTotal: " << system_info.get_mem_total() << " kB" << std::endl;
        std::cout << "MemAvailable: " << system_info.get_mem_available() << " kB" << std::endl;
   
        std::cout << "Load: " << std::fixed << std::setprecision(2) << system_info.get_load() << std::endl;
    
        for (auto m : system_info.get_registered_mounts()) {
            std::cout << m << std::endl;
            disk_info di = system_info.get_disk_info(m);
            std::cout << "  capacity: " << std::fixed << std::setprecision(0) << di.capacity << " GB" << std::endl;
            std::cout << "  available: " << std::fixed << std::setprecision(1) << di.available << " GB" << std::endl;
        }
        
        std::cout << "=============================================================\n";
        std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
    }
    
    
    
    
}