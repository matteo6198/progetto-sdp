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
- a new field for each segment has been inserted in order to maintain the offset in 
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

### SWAPFILE

This file is essential for managing the operations of swap in and swap out of the pages.
Every time a process needs to find a page in case of a page fault, it first looks for that
page in the SWAPFILE. If it's found, it's read from there; if it's not present in the file,
it's looked for and read in the ELF.
Instead, if a free frame is needed in memory we look for a victim page,
Then, once the victim is selected, if that page can be written (as it owns the writing rights) it is written to the SWAPFILE
since it may have been modified by the current process; else, if the page is readonly, it is simply discarded.
A simple <code>hash table</code> (a preallocated array of uint32_t with size swap_filesize/page_size)
has been used to track the pages that have been written into the SWAPFILE in order to speed up the
input/output on the file itself. Each entry contains the virtual address of the page and the PID of the 
relative process, both codified in a single uint32_t: the last
11 bits are the PID and 20 bits are reserved for the virtual address.

|Virtual address|Empty| PID|
|:--:|:--:|:--:|
|20 bit| 1 bit|11 bit|

The <code>hash_swap</code> function is a simple hash that returns the starting position in the table from which we begin to look
for the target page in case we want to read it from the file, or for a free position on the
SWAPFILE if we want to write it. Collisions may happen, if
the hash function returns an index corresponding to a portion of the file that was already occupied,
in this case a linear scan is made starting from that index until we find a free position.
The file is limited to 9 MB, as requested by the specifics, so if the hash table is full (the SWAPFILE has reached its 
maximum size) a call to "panic" is made by the kernel.

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


## TLB

The TLB in System/161 includes 64 entries. Each entry is a 64-bit value which can be split
in two different parts: high and low.
The higher part is filled with the virtual page number, it may be followed by the ASID but
in this project we don't use that field. The lower part is filled with the physical page number
and four 1-bit fields: global, valid, dirty, nocache.
Global and nocache fields are not used in this project.
The valid bit is used to represent a valid translation from virtual memory to physical one, the
dirty bit is used to represent a memory reference with write permission.
If the dirty bit is not set and a write operation is attempted, the MMU generates an EX MOD exception.

### TLB Replacement Policy

The policy adopted to handle entries replacement is the one specified in the requirements file.
With the Round-Robin replacement, we can imagine the TLB as a circular buffer, in which each new entry
is inserted after the last inserted entry. When the TLB fills, the counter starts again from the first position.
When a TLB invalidation occurs, there is no need to reset the counter.

### TLB Invalidation

As in the DUMBVM system, every time a context-switch happens,
the kernel calls <code>as_activate()</code>
which invalidates the TLB. In this way every time a process runs,
we can ensure that every valid TLB entry is owned
by the current process and therefore we can avoid to use the ASID field.
Other TLB invalidations happen when the kernel gets/frees memory.

### Read-only text segment

In order to ensure that each text segment is read only, TLB entries must be set properly.
This is accomplished through the <code>dirty bit</code> mentioned before.
The first time that a process makes an access to a page belonging to the text segment,
a new entry must be inserted in the TLB. The dirty bit is set if and only if the page
can be written. Thus, if the process tries to write a page that is in the TLB without the
dirty bit set, the MMU generates a EX MOD exception, then the kernel kills the process.
