# progetto-sdp

## Obiettivi

* TLB Management (Ruggero)
* On-Demand Page Loading (Matteo)
* Page replacement (Samuele)

* <i>Statistics</i>

## Deadline

* 10/08 (presentazione soluzioni)
* 21/08 (prima RC)

## Important Files

### kern/syscall:

loadelf.c: This file contains the functions responsible for loading an ELF executable from 
the filesystem into virtual-memory space. You should already be familiar with this file from 
Lab 2. Since you will be implementing on-demand page loading, you will need to change the 
behaviour that is implemented here. 
### kern/vm:

kmalloc.c: This file contains implementations of kmalloc and kfree, to support dynamic 
memory allocation for the kernel. It should not be necessary for you to change the code in 
this file, but you do need to understand how the kernel’s dynamic memory allocation works 
so that your physical-memory manager will interact properly with it. 

### kern/include:

addrspace.h: define the addrspace interface. You will need to make changes here, at least to 
define an appropriate addrspace structure. 

vm.h: Some VM-related definitions, including prototypes for some key functions, such as 
vm_fault (the TLB miss handler) and alloc_kpages (used, among other places, in kmalloc). 

### kern/arch/mips/vm:

dumbvm.c: This file shpuld not be used (when disabling option dumbvm). However, you 
can use the code here (with improvements done in lab 2). This code also includes examples of how to do things like manipulate the TLB. 

ram.c: This file includes functions that the kernel uses to manage physical memory (RAM) 
while the kernel is booting up, before the VM system has been initialized. Since your VM 
system will essentially be taking over management of physical memory, you need to 
understand how these functions work. 

### kern/arch/mips/include:

In this directory, the file tlb.h defines the functions that are used to manipulate the TLB. In 
addition, vm.h includes some macros and constants related to address translation on the 
MIPS. Note that this vm.h is different from the vm.h in kern/include.

## Statistics

You can collect the following statistics:
• TLB Faults: The number of TLB misses that have occurred (not including faults that cause 
a program to crash).  
• TLB Faults with Free: The number of TLB misses for which there was free space in the 
TLB to add the new TLB entry (i.e., no replacement is required).  
• TLB Faults with Replace: The number of TLB misses for which there was no free space 
for the new TLB entry, so replacement was required.  
• TLB Invalidations: The number of times the TLB was invalidated (this counts the number 
times the entire TLB is invalidated NOT the number of TLB entries invalidated)  
• TLB Reloads: The number of TLB misses for pages that were already in memory.
• Page Faults (Zeroed): The number of TLB misses that required a new page to be zerofilled. 
• Page Faults (Disk): The number of TLB misses that required a page to be loaded from disk. 
• Page Faults from ELF: The number of page faults that require getting a page from the ELF 
file. 
• Page Faults from Swapfile: The number of page faults that require getting a page from the 
swap file. 
• Swapfile Writes: The number of page faults that require writing a page to the swap file.  
Note that the sum of “TLB Faults with Free” and “TLB Faults with Replace” should be equal 
to “TLB Faults.” Also, the sum of “TLB Reloads,” “Page Faults (Disk),” and “Page Faults (Zeroed)” should be equal to “TLB Faults.” So this means that you should not count TLB 
faults that do not get handled (i.e., result in the program being killed). The code for printing 
out stats will print a warning if this these equalities do not hold. In addition the sum of 
“Page Faults from ELF” and “Page Faults from Swapfile” should be equal to “Page Faults 
(Disk)”.  
When it is shut down (e.g., in vm_shutdown), your kernel should display the statistics it has 
gathered. The display should look like the example below.


## Note

You are free and will need to modify existing kernel code in addition you’ll probably need 
some code to create and use some new abstractions. If you uses any or all of the following 
abstractions please place that code in the directory kern/vm using the following file names:
• coremap.c: keep track of free physical frames  
• pt.c: page tables and page table entry manipulation go here  
• segments.c: code for tracking and manipulating segments 
• vm_tlb.c: code for manipulating the tlb (including replacement) 
• swapfile.c: code for managing and manipulating the swapfile 
• vmstats.c: code for tracking stats
If you need them, corresponding header files should be placed in os161-1.11/kern/include 
in files named: addrspace.h, coremap.h, pt.h, segments.h, vm_tlb.h, vmstats.h, and 
swapfile.h. 