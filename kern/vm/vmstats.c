#include <vmstats.h>

unsigned int vms_faults = 0;
unsigned int vms_faults_free = 0;
unsigned int vms_faults_replace = 0;
unsigned int vms_invalidate = 0;
unsigned int vms_reload = 0;
unsigned int vms_faults_zeroed = 0;
unsigned int vms_faults_disk = 0;
unsigned int vms_faults_elf = 0;
unsigned int vms_faults_swapfile = 0;
unsigned int vms_swapfile_writes = 0;

void vms_update(unsigned char code)
{
    switch(code){
        case VMS_FAULTS:
        vms_faults++;
        break;                  
        case VMS_FAULTS_FREE:
        vms_faults_free++;
        break;        
        case VMS_FAULTS_REPLACE:
        vms_faults_replace++;
        break;  
        case VMS_INVALIDATE:
        vms_invalidate++;
        break;          
        case VMS_RELOAD:
        vms_reload++;
        break;                  
        case VMS_FAULTS_ZEROED:
        vms_faults_zeroed++;
        break;    
        case VMS_FAULTS_DISK:
        vms_faults_disk++;
        break;        
        case VMS_FAULTS_ELF:
        vms_faults_elf++;
        break;          
        case VMS_FAULTS_SWAPFILE:
        vms_faults_swapfile++;
        break;
        case VMS_SWAPFILE_WRITES:
        vms_swapfile_writes++;
        break;
        default:
    }
}

void vms_print(void)
{
    kprintf("[vmstats] TLB Faults: %u\n", vms_faults);
    kprintf("[vmstats] TLB Faults with Free: %u\n", vms_faults_free);
    kprintf("[vmstats] TLB Faults with Replace: %u\n", vms_faults_replace);
    kprintf("[vmstats] TLB Invalidations: %u\n", vms_invalidate);
    kprintf("[vmstats] TLB Reloads: %u\n", vms_reload);
    kprintf("[vmstats] Page Faults (Zeroed) : %u\n", vms_faults_zeroed);
    kprintf("[vmstats] Page Faults (Disk): %u\n", vms_faults_disk);
    kprintf("[vmstats] Page Faults from ELF: %u\n", vms_faults_elf);
    kprintf("[vmstats] Page Faults from Swapfile : %u\n", vms_faults_swapfile);
    kprintf("[vmstats] Swapfile Writes: %u\n", vms_swapfile_writes);
}