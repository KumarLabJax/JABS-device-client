#include <sys/sysinfo.h>
#include <string>
#include <fstream>
#include <limits>

#include "system_info.h"

SysInfo::SysInfo(void)
{
    this->sample();
        
    // get the amount of physical RAM -- this won't change so we don't need to update
    this->mem_total = this->system_info.totalram * this->system_info.mem_unit / 1024;
}

void SysInfo::sample(void)
{
    this->update_mem_info();
    sysinfo(&this->system_info);    
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

float SysInfo::get_load()
{
    // convert the integer value sysinfo gives us to the floating point number we expect
    return this->system_info.loads[0] / (float)(1 << SI_LOAD_SHIFT);
}

