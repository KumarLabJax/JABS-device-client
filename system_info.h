#include <sys/sysinfo.h>

/**
 * system information
 *
 * this class gathers system information such as total physical memory, memory available, 
 * available disk space, load average. This uses functionality only available on Linux 
 * and is non-portable.
 */
class SysInfo {
    private:
        struct sysinfo system_info;
        unsigned long mem_available;
        unsigned long mem_total;
        void update_mem_info();

    public:
        /**
         * constructor
         *  
         * creates new SysInfo instance and calls the sample() method to initialize the 
         * state
         *
         */
        SysInfo();
        
        /**
         * sample system information
         * 
         * get updated memory usage, load, disk usage
         *
         * @return void
         */
        void sample();
        
        /**
         * get amount of physical memory
         *
         * get the amount of physical RAM in kB
         *
         * @return amount of RAM
         */
        unsigned long get_mem_total() {return this->mem_total;}
        
        
        /**
         * get memory available
         *
         * get amount of available RAM in kB
         *
         * @return memory available 
         */
        unsigned long get_mem_available() {return this->mem_available;}
        
       
        
        /**
         * get 1 minute load average
         *
         * @return floating point load average
         */
        float get_load();
};