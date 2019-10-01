#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <sys/sysinfo.h>
#include <sys/types.h>

/**
 * @brief store information about a monitored directory
 *
 */
struct DiskInfo {
    unsigned long capacity;  ///capacity of mount in megabytes
    unsigned long available; ///available disk space of mount in megabytes
};

/**
 * @brief exception thrown if there is an error registering a directory to monitor
 *
 */
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
 * @brief system information
 *
 * this class gathers system information such as total physical memory, memory available, 
 * available disk space, load average. This uses functionality only available on Linux 
 * and is non-portable.
 */
class SysInfo {
    private:
        float load_;
        unsigned long mem_available_;
        unsigned long mem_total_;
        struct sysinfo system_info;
        std::string hostname_;
        std::string release_;
        std::set<std::string> mount_points;
        std::map<std::string, DiskInfo> disk_information;
        
        void UpdateMemInfo();
        unsigned long BlocksToMb(fsblkcnt_t blocks, unsigned long bsize);
        

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
        void Sample();
        
        std::string hostname() {return hostname_;}
        
        /**
         * @brief get amount of physical memory in kB
         *
         * @return amount of RAM
         */
        unsigned long mem_total() {return mem_total_;}
            
        /**
         * @brief get memory available in kB
         *
         * @return memory available 
         */
        unsigned long mem_available() {return mem_available_;}
        
        /**
         * @brief get 1 minute load average
         *
         * @return floating point load average
         */
        float load() {return load_;}
        
        /**
         * @brief get uptime in seconds
         *
         * @return number of seconds since boot
         */
        unsigned long uptime() {return system_info.uptime;}
        
        /**
         * @brief get release string
         *
         * @return release (NVidia Tegra release string or Kernel Release)
         */
        std::string release() {return release_;}
        
        /**
         * @brief register mount point
         *
         * register a mount point so that we will gather information about it (capacity, 
         * available space)
         *
         * @param path string containing path to mount
         */
        void AddMount(const std::string path);
        
        /**
         * @brief clear list of registered mount points
         *
         */
        void ClearMounts();
        
        /**
         * @brief get mount points that have been registered for monitoring
         *
         * @return a vector of strings listing all registered mounts
         */
        std::vector<std::string> registered_mounts();
        
        /**
         * @brief returns information about capacity and available disk space
         *
         * this method looks up a mount point, specified as a string, and returns the 
         * capacity and available disk space in a struct. This mount point must have been
         * registered prior to the last call to sample(), otherwise reported capacity and
         * available space will be zero
         *
         * @return a struct containing disk capacity and available space
         */
        DiskInfo disk_info(std::string mount);
};
#endif