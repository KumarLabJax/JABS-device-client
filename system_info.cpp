#include <string>
#include <fstream>
#include <limits>

#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <assert.h>

#include "system_info.h"

SysInfo::SysInfo(void)
{
    Sample();
    char buffer[1024];
    gethostname(buffer, sizeof(buffer));
    hostname_ = std::string(buffer);

    // get the amount of physical RAM -- this won't change so we don't need to update
    mem_total_ = system_info.totalram * system_info.mem_unit / 1024;
}

/*
 * this method is intended to be called periodically to update memory available,
 * disk available, system load, etc
 */
void SysInfo::Sample(void)
{
    sysinfo(&system_info);
    UpdateMemInfo();

    // convert the integer value sysinfo gives us to the floating point number we expect
    load_ = system_info.loads[0] / (float)(1 << SI_LOAD_SHIFT);


    // update disk information
    for(auto m : mount_points) {
        struct statvfs buf;
        DiskInfo di;

        if (statvfs(m.c_str(), &buf) == 0) {
            di.capacity = BlocksToMb(buf.f_blocks, buf.f_frsize);
            di.available = BlocksToMb(buf.f_bavail, buf.f_bsize);
        } else {
        // TODO handle the error case. for now we just return zero for capacity/available
            di.capacity = 0;
            di.available = 0;
        }
        disk_information[m] = di;
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

    mem_available_ = 0;

    while(file >> token) {
        if(token == "MemAvailable:") {
            unsigned long mem;
            if(file >> mem) {
                mem_available_ = mem;
            }
        }
        // ignore rest of the line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void SysInfo::AddMount(std::string mount)
{
    // TODO check that mount point exists before registering, otherwise throw exception
    mount_points.insert(mount);
}

void SysInfo::ClearMounts()
{
    mount_points.clear();
    disk_information.clear();
}

std::vector<std::string> SysInfo::registered_mounts()
{
    // return a vector instead of the set we're using internally
    std::vector<std::string> v(mount_points.begin(), mount_points.end());
    return v;
}

DiskInfo SysInfo::disk_info(std::string mount)
{

    DiskInfo di;

    auto it = disk_information.find(mount);
    assert(it != disk_information.end());

    di.capacity = it->second.capacity;
    di.available = it->second.available;

    return di;
}

unsigned long SysInfo::BlocksToMb(fsblkcnt_t blocks, unsigned long bsize)
{
    // convert number of filesystem blocks into a size in mB
    return blocks * bsize / (1048576);
}

