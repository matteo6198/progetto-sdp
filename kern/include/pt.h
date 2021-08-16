#ifndef _PT_H
#define _PT_H

#define STACKPAGES  18


// Errors
#define ERR_CODE ((paddr_t) -1)

#include "opt-ondemand_manage.h"

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <coremap.h>
#include <proc_syscalls.h>
#include <vm_tlb.h>

/* macros for accessing the PT entry fields */
#define PT_V_ADDR(entry) (entry->hi & PAGE_FRAME)
#define PT_P_ADDR(entry) (entry->lo & PAGE_FRAME)
#define PT_PID(entry)    (entry->hi & (~PAGE_FRAME))
#define PT_RAM(entry)    (entry->lo & 1)
#define PT_SWAP(entry)   ((entry->lo & 2) >> 1)
#define PT_DIRTY(entry)  ((entry->lo & 4) >> 2)
#define PT_READ(entry)   ((entry->lo & 8) >> 3)
#define PT_WRITE(entry)  ((entry->lo & 16) >> 4)
#define PT_EXEC(entry)   ((entry->lo & 32) >> 5)

struct pt_entry{
    int hi;     /* contains v_addr and pid */
    int lo;     /* contains p_addr and flags */
};

struct pt{
    struct pt_entry *entry;
    struct pt *next;
};

/* bootstrap for the page table */
void pt_bootstrap(void);

/* returns the entry corresponding to the page associated to the address v_addr (if that page is not in memory it will be loaded)*/
int pt_get_page(vaddr_t v_addr);

/* insert n_pages pages into the page table without allocating them in RAM */
int pt_insert(vaddr_t v_addr, unsigned int n_pages, int read, int write, int exec);

/* delete all pages of this process from page table */
void pt_delete_PID(struct addrspace *as);

#endif