#include <string>
#include <fstream>
#include <limits>

#include <sys/sysinfo.h>
#include <sys/statvfs.h>

#include "system_info.h"

SysInfo::SysInfo(void)
{
    this->sample();
        
    // get the amount of physical RAM -- this won't change so we don't need to update
    this->mem_total = this->system_info.totalram * this->system_info.mem_unit / 1024;
}

void SysInfo::sample(void)
{
    sysinfo(&this->system_info);
    this->update_mem_info();
    
    // convert the integer value sysinfo gives us to the floating point number we expect
    this->load = this->system_info.loads[0] / (float)(1 << SI_LOAD_SHIFT);
    
    // update disk information
    for(auto m : this->mount_points) {
        struct statvfs buf;
        disk_info di;
        
        if (statvfs(m.c_str(), &buf) == 0) {
            di.capacity = this->blocks_to_gb(buf.f_blocks, buf.f_frsize);
            di.available = this->blocks_to_gb(buf.f_bavail, buf.f_bsize);
        } else {
            di.capacity = 0.0;
            di.available = 0.0;
        }
        this->disk_information[m] = di;
    }
}

/*
 * This method uses /proc/meminfo to get an updated view of memory usage. This is more 
 * accurate than using the information returned by sysinfo, because Linux will use 
 * free memory for disk caching. To sysinfo this appears to be used, but the kernel will
 * free it as soon as it is needed by another program so it should be included in the 
 * reported available memory. 
 */
void SysInfo::update_mem_info()
{
    std::string token;
    std::ifstream file("/proc/meminfo");
    
    this->mem_available = 0;

    while(file >> token) {
        if(token == "MemAvailable:") {
            unsigned long mem;
            if(file >> mem) {
                this->mem_available = mem;
            } 
        } 
        // ignore rest of the line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void SysInfo::register_mount(std::string mount)
{
    // TODO check that mount point exists before registering, otherwise throw exception
    this->mount_points.insert(mount);
}

std::vector<std::string> SysInfo::get_registered_mounts()
{
    std::vector<std::string> v(this->mount_points.begin(), this->mount_points.end());
    return v;
}

disk_info SysInfo::get_disk_info(std::string mount)
{

    disk_info di;
    
    auto it = this->disk_information.find(mount);
    if (it != this->disk_information.end()) {
        di.capacity = it->second.capacity;
        di.available = it->second.available;
        
    } else {
        di.capacity = 0.0;
        di.available = 0.0;
    }
    return di;
}

double SysInfo::blocks_to_gb(fsblkcnt_t blocks, unsigned long bsize)
{
    // convert number of filesystem blocks into a size in GB
    return blocks / (1073741824.0 / bsize);
}

