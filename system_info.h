#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <sys/sysinfo.h>
#include <sys/types.h>

struct disk_info {
    unsigned long capacity;
    unsigned long available;
};

class DiskRegistrationException: public std::exception {
  
  private: 
      std::string message;
      
  public:
      DiskRegistrationException(std::string msg = "Error registering path"): message(msg) {}
  
      virtual const char* what() const throw()
      {
          return this->message.c_str();
      }
};

/**
 * system information
 *
 * this class gathers system information such as total physical memory, memory available, 
 * available disk space, load average. This uses functionality only available on Linux 
 * and is non-portable.
 */
class SysInfo {
    private:
        float load;
        unsigned long mem_available;
        unsigned long mem_total;
        struct sysinfo system_info;
        std::string hostname;
        std::set<std::string> mount_points;
        std::map<std::string, disk_info> disk_information;
        
        void update_mem_info();
        unsigned long blocks_to_mb(fsblkcnt_t blocks, unsigned long bsize);
        

    public:
        /**
         * @brief constructor
         *  
         * creates new SysInfo instance and calls the sample() method to initialize the 
         * state
         *
         */
        SysInfo();
        
        /**
         * @brief sample system information
         * 
         * refreshes the view of the system by getting updated memory usage, load,
         * disk usage, etc
         *
         * @return void
         */
        void sample();
        
        std::string get_hostname() {return this->hostname;}
        
        /**
         * get amount of physical memory in kB
         *
         * @return amount of RAM
         */
        unsigned long get_mem_total() {return this->mem_total;}
            
        /**
         * get memory available in kB
         *
         * @return memory available 
         */
        unsigned long get_mem_available() {return this->mem_available;}
        
        /**
         * get 1 minute load average
         *
         * @return floating point load average
         */
        float get_load() {return this->load;}
        
        /**
         * get uptime in seconds
         *
         * @return number of seconds since boot
         */
        unsigned long get_uptime() {return this->system_info.uptime;}
        
        /**
         * @brief register mount point
         *
         * register a mount point so that we will gather information about it (capacity, 
         * available space)
         *
         * @param path string containing path to mount
         */
        void register_mount(std::string path);
        
        /**
         * @brief clear list of registered mount points
         *
         */
         void clear_mounts();
        
        /**
         * get mount points that have been registered for monitoring
         *
         * @return a vector of strings listing all registered mounts
         */
        std::vector<std::string> get_registered_mounts();
        
        /**
         * @brief returns information about capacity and available disk space
         *
         * this method looks up a mount point, specified as a string, and returns the 
         * capacity and available disk space in a struct. This mount point must have been
         * registered prior to the last call to sample(), otherwise reported capacity and
         * available space will be zero
         *
         * @returns a struct containing disk capacity and available space
         */
        disk_info get_disk_info(std::string mount);
};
#endif