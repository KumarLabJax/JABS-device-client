#include <iostream>
#include <iomanip>
#include <csignal>
#include <chrono>
#include <thread>

#include <sysexits.h>
#include <systemd/sd-daemon.h>

#include "system_info.h"

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

int main(int argc, char **argv)
{
   
    unsigned int sleep_time = 30;
    SysInfo system_info;
    
    signal(SIGTERM, signalHandler); 
    signal(SIGHUP, signalHandler); 
    
    try {
        system_info.register_mount(std::string("/tmp"));
    }
    catch (DiskRegistrationException &e) {
        std::clog << SD_ERR << e.what() << std::endl;
        sd_notifyf(0, "STATUS=Failed to register mount");
        exit(EX_CONFIG);
    }
    
    sd_notify(0, "READY=1");
    
    while (1) {
    
        if (hup_received) {
            //TODO need to reload configuration
            hup_received = 0;
        }
    
        system_info.sample();
        
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