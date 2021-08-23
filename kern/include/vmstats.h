#ifndef _VMSTATS_H_
#define _VMSTATS_H_

#include <lib.h>

#define VMS_FAULTS          0 /* The number of TLB misses that have occurred (not including faults that cause a program to crash) */ 
#define VMS_FAULTS_FREE     1 /* The number of TLB misses for which there was free space in the TLB to add the new TLB entry (i.e., no replacement is required)*/
#define VMS_FAULTS_REPLACE  2 /* The number of TLB misses for which there was no free space for the new TLB entry, so replacement was required*/
#define VMS_INVALIDATE      3 /* The number of times the TLB was invalidated (this counts the number times the entire TLB is invalidated NOT the number of TLB entries invalidated)*/
#define VMS_RELOAD          4 /* The number of TLB misses for pages that were already in memory*/
#define VMS_FAULTS_ZEROED   5 /* The number of TLB misses that required a new page to be zerofilled. */
#define VMS_FAULTS_DISK     6 /* The number of TLB misses that required a page to be loaded from disk. */
#define VMS_FAULTS_ELF      7 /* The number of page faults that require getting a page from the ELF file. */
#define VMS_FAULTS_SWAPFILE 8 /* The number of page faults that require getting a page from the swap file. */  
#define VMS_SWAPFILE_WRITES 9 /* The number of page faults that require writing a page to the swap file. */

void vms_update(unsigned char code);

void vms_print(void);
#endif /* _VMSTATS_H_ */



