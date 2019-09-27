#include <string>
#include <fstream>
#include <limits>

#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/utsname.h>

#include "system_info.h"

SysInfo::SysInfo(void)
{
    this->Sample();
    char buffer[1024];
    gethostname(buffer, sizeof(buffer));
    this->hostname_ = std::string(buffer);
        
    // get the amount of physical RAM -- this won't change so we don't need to update
    this->mem_total_ = this->system_info.totalram * this->system_info.mem_unit / 1024;
    
    // get the release. We use the first line of /etc/nv_tegra_release if it is available
    // and accessible, otherwise we fall back to using the Linux Kernel release string
    std::ifstream release_file("/etc/nv_tegra_release");
    if (release_file) {
        std::getline(release_file, this->release_);
    } else {
        struct utsname buf;
        if (uname(&buf) != 0) {
            this->release_ =  std::string("UNKNOWN");
        } else {
            this->release_ =  std::string(buf.release);
        }
    }
}

/*
 * this method is intended to be called periodically to update memory available, 
 * disk available, system load, etc
 */
void SysInfo::Sample(void)
{
    sysinfo(&this->system_info);
    this->UpdateMemInfo();
    
    // convert the integer value sysinfo gives us to the floating point number we expect
    this->load_ = this->system_info.loads[0] / (float)(1 << SI_LOAD_SHIFT);
    
    
    // update disk information
    for(auto m : this->mount_points) {
        struct statvfs buf;
        DiskInfo di;
        
        if (statvfs(m.c_str(), &buf) == 0) {
            di.capacity = this->BlocksToMb(buf.f_blocks, buf.f_frsize);
            di.available = this->BlocksToMb(buf.f_bavail, buf.f_bsize);
        } else {
            // TODO handle the error case. for now we just return zero for capacity/available
            di.capacity = 0;
            di.available = 0;
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
void SysInfo::UpdateMemInfo()
{
    std::string token;
    std::ifstream file("/proc/meminfo");
    
    this->mem_available_ = 0;

    while(file >> token) {
        if(token == "MemAvailable:") {
            unsigned long mem;
            if(file >> mem) {
                this->mem_available_ = mem;
            } 
        } 
        // ignore rest of the line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void SysInfo::AddMount(std::string mount)
{
    // TODO check that mount point exists before registering, otherwise throw exception
    this->mount_points.insert(mount);
}

void SysInfo::ClearMounts()
{
    this->mount_points.clear();
    this->disk_information.clear();
}

std::vector<std::string> SysInfo::registered_mounts()
{
    // return a vector instead of the set we're using internally
    std::vector<std::string> v(this->mount_points.begin(), this->mount_points.end());
    return v;
}

DiskInfo SysInfo::disk_info(std::string mount)
{

    DiskInfo di;
    
    auto it = this->disk_information.find(mount);
    assert(it != this->disk_information.end());
        
    di.capacity = it->second.capacity;
    di.available = it->second.available;

    return di;
}

unsigned long SysInfo::BlocksToMb(fsblkcnt_t blocks, unsigned long bsize)
{
    // convert number of filesystem blocks into a size in mB
    return blocks * bsize / (1048576);
}


