# Report

## Page table

The page table implemented in this project is an inverted page table that uses a 
clustered hash table in order to speed up the search of pages.
In fact this hash table is organized in a set of clusters of 4 pages in which each 
page can be mapped. The hash function adopted returns only the 
cluster number and the page to be inserted or removed may be found only among the 
pages contained inside the cluster.

### Page table entries

Each entry in the page table is stored on 32 bits organized as follows:
- 20 higher bits to store the virtual page number
- 11 bits to store the process identifier (PID)
- 1 bit for the dirty flag

|Virtual address| PID| Dirty|
|:--:|:--:|:--:|
|20 bit| 11 bit|1 bit|

The physical address of the page, as it is possible to notice, is not stored in the 
entry because it is derived by the index of the entry inside the hash table by 
multiplying it by the size of a page.

### On demand page loading

When a new process starts, it has no page allocated to it. At every page fault, the address which was causing the fault is adopted in order to find the missing page.
There are 3 possible scenarios:

- the fault page was already in memory and so the entry related to that page is simply inserted in the TLB
- the fault page was not in memory but in the SWAPFILE and in that case the page is loaded from it and the correct entry is inserted in the TLB
- the page was not in memory nor in the SWAPFILE so the page is loaded from the program ELF file

In order to support the last case, some modifications have been apported to the 
<code>struct proc</code> in order to insert in it the pointer to the <code>struct
 vnode</code> related to the ELF file of the program. That file is opened when the 
 process is run and it is closed only when the process has terminated his execution.
In order to correctly retrieve the correct position in the ELF file of the selected 
page, some modifications have been apported to the <code>struct addrspace</code>:

- the information relative to the physical address of the segments and the stack have
 been removed
- a new filed for each segment has been inserted in order to maintain the offset in 
the ELF file of the start of the segment
- a new field for each segment has been inserted in order to keep the start of the 
virtual address of the segment in the ELF file (which may be not page aligned)
- a new field for each segment in order to store the file size of the segment
- a new field for the permission on the 2 segments in the form (- - R2 W2 X2 R1 W1 X1)

All of those modifications, although not strictly necessary since those information can be read every time from the header of the ELF file, have been apported in order to speed up the search of those information.

### Page replacement

Every time an insertion of a new page is done in the hash table, the content of the 
whole cluster, which index was given by the hash function, is scanned. There are two 
main situations:

1. A free frame is found (characterized by an entry set to 0)
1. No free frames are available in the cluster

In the first case, the frame found is used for the new page. In the second case page 
replacement is necessary. In fact, a page (the victim page) is randomly selected 
among the one of the cluster. If the victim page had the dirty bit set, the swap out
function is called in order to write the page to the SWAPFILE, if that bit was not set
the page is simply discarded since it can be easily retrieved from the ELF file of the
running program.

### Kernel memory

In the whole project, the kernel memory is always kept separated from the user one.
During the startup phase of the OS, the kernel allocates most of the memory that it 
will need during the whole execution.
In order to keep track of the memory used from the kernel, a separate bitmap and a 
vector for the number of pages allocated have been adopted. 
All the remaining memory (apart from some pages that may be used for the alignment of 
clusters in the page table) is free for user processes and the kernel can't allocate 
it directly. That memory is signaled as used in the kernel bitmap.
In order to permit the kernel to allocate frames at every time during the execution, 
a mechanism has been adopted in order to grab some memory which was allocated to user 
processes.
In fact, when no free kernel memory can be found to satisfy a request, some clusters 
(as much as needed to surely satisfy the request) are taken from user memory and are 
allocated to kernel that now will be able to get the required pages.
When the kernel will free the pages acquired, if they are enough to form a new user 
cluster, they are returned to the user memory.
That process is very expensive since at every operation all the pages in the page 
table must be swapped out since the number of cluster is changed and consequently the 
hash function adopted too so, in case of a new lookup in the page table, the searched 
page will not be found.
In order to reduce that cost, memory is returned to the user only if there are more than 2 free clusters at the end of kernel memory.