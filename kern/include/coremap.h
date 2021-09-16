#ifndef _COREMAP_H_
#define _COREMAP_H_


#define PAGE_FREE (char)0
#define PAGE_NOT_ALLOCATED (char)1
#define PAGE_USED (char)2

#define KVADDR_2_PADDR(vaddr) (unsigned long)(vaddr - MIPS_KSEG0)

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <vm.h>
#include <cpu.h>
#include <thread.h>
#include <pt.h>
#include <current.h>
#include "opt-paging.h"
#include <opt-vm_manage.h>

paddr_t getFreePages(unsigned int n);
void free_ppage(paddr_t paddr);
void pageSetUsed(unsigned int i);
int return_mem(uint32_t page);
#endif /* _COREMAP_H_ */