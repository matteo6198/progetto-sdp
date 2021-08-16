#include <coremap.h>

static unsigned long *allocated_size;
static char *allocated_pages;
static int active = 0;
static unsigned long nRamFrames = 0;
static struct spinlock memSpinLock = SPINLOCK_INITIALIZER;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static void pageSetFree(unsigned long i)
{
#if BITMAP
	/* setting the bit to 1 means page free */
	allocated_pages[i / 8] |= (1 << (i % 8));
#else
	allocated_pages[i] = PAGE_FREE;
#endif
}
static void pageSetUsed(unsigned long i)
{
#if BITMAP
	/* clearing the bit means page used */
	allocated_pages[i / 8] &= ~(1 << (i % 8));
#else
	allocated_pages[i] = PAGE_USED;
#endif
}
static int isPageFree(unsigned long i)
{
#if BITMAP
	/* check if the bit is 1 */
	return allocated_pages[i / 8] & (1 << (i % 8));
#else
	return allocated_pages[i] == PAGE_FREE;
#endif
}

paddr_t getFreePages(unsigned long n)
{
	paddr_t addr = 0;
	unsigned long i, count;
	int found = 0;
	/* page allocation is done in mututal exclusion */
	spinlock_acquire(&memSpinLock);

	if (!active)
	{
		spinlock_release(&memSpinLock);
		return 0;
	}

	for (i = 0, count = 0; i < nRamFrames; i++)
	{
		if (isPageFree(i))
		{
			count++;
			if (count == 1)
				addr = i;
			if (count == n)
			{
				found = 1;
				break;
			}
		}
		else
			count = 0;
	}
	if (found)
	{
		for (i = addr; i < addr + n; i++)
		{
			pageSetUsed(i);
		}
		allocated_size[addr] = n;
		addr *= PAGE_SIZE;
	}
	else
	{
		addr = 0;
	}

	spinlock_release(&memSpinLock);
	return addr;
}

static paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;
#if OPT_VM_MANAGE

	addr = getFreePages(npages);
	if (addr == 0)
	{
		unsigned long page, i;
		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
		if (active)
		{
			page = addr / PAGE_SIZE;
			for (i = page; i < page + npages; i++)
			{
				pageSetUsed(i);
			}
			allocated_size[page] = npages;
		}
	}
	return addr;

#else
	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
#endif
	return addr;
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */
static void
vm_can_sleep(void)
{
	if (CURCPU_EXISTS())
	{
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

void vm_bootstrap(void)
{
#if OPT_VM_MANAGE
	unsigned long i;

	spinlock_acquire(&memSpinLock);
	/* protection from multiple bootstrap */
	if (active)
	{
		spinlock_release(&memSpinLock);
		return;
	}
	spinlock_release(&memSpinLock);
	nRamFrames = (ram_getsize() / PAGE_SIZE);

#if BITMAP
	allocated_pages = kmalloc(nRamFrames / 8 * sizeof(char));
#else
	allocated_pages = kmalloc(nRamFrames * sizeof(char));
#endif /* BITMAP */

	allocated_size = kmalloc(nRamFrames * sizeof(unsigned long));
	if (allocated_pages == NULL || allocated_size == NULL)
	{
		spinlock_release(&memSpinLock);
		return;
	}
	spinlock_acquire(&memSpinLock);
#if ALLOC_ALL_AT_BOOT
	paddr_t start = ram_stealmem(1); /* get first free address */
	unsigned long nRemPages = nRamFrames - start / PAGE_SIZE - 1;
	ram_stealmem(nRemPages);

	for (i = 0; i < start / PAGE_SIZE; i++)
	{
		pageSetUsed(i);
		allocated_size[i] = 1;
	}
	for (i = start / PAGE_SIZE; i < nRamFrames; i++)
	{
		pageSetFree(i);
		allocated_size[i] = 0;
	}
#else
	for (i = 0; i < nRamFrames; i++)
	{
#if BITMAP
		pageSetUsed(i);
#else
		allocated_pages[i] = (char)PAGE_NOT_ALLOCATED;
#endif /* BITMAP */
		allocated_size[i] = 0;
	}
#endif /* ALLOCATE_ALL_AT_BOOT */
	active = 1;
	spinlock_release(&memSpinLock);
	kprintf("virtual memory boot completed...\n");
#endif /* OPT_VM_MANAGE */
#if OPT_ONDEMAND_MANAGE
    pt_bootstrap();
#endif
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	vm_can_sleep();
	pa = getppages(npages);
	if (pa == 0)
	{
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr)
{
	/* nothing - leak the memory. */
#if OPT_VM_MANAGE
	unsigned long page, n_alloc, i;

	spinlock_acquire(&memSpinLock);
	if (!active)
	{
		spinlock_release(&memSpinLock);
		return;
	}

	page = KVADDR_2_PADDR(addr) / PAGE_SIZE;
	KASSERT(page < nRamFrames);
	/* get number of contiguous pages allocated */
	n_alloc = allocated_size[page];
	allocated_size[page] = 0;
	/* free pages on the bitmap */
	for (i = page; i < page + n_alloc; i++)
	{
		pageSetFree(i);
	}
	spinlock_release(&memSpinLock);
#endif
	(void)addr;
}

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}


void memstats(void)
{
#if OPT_VM_MANAGE
	unsigned long free_pages, i, free_mem, used_mem;
	spinlock_acquire(&memSpinLock);
	if (!active)
	{
		spinlock_release(&memSpinLock);
		kprintf("Memeory allocation table is not yet active\n");
		return;
	}
	kprintf("Memory view:\n");
	for (i = free_pages = 0; i < nRamFrames; i++)
	{
		if (isPageFree(i))
		{
			free_pages++;
			kprintf("F ");
		}
		else
			kprintf("U ");
	}
	kprintf("\n");
	spinlock_release(&memSpinLock);

	free_mem = free_pages * PAGE_SIZE;
	used_mem = nRamFrames * PAGE_SIZE - free_mem;
	kprintf("Free memory:\t%lu kB\nUsed memory:\t%lu kB\n", free_mem / 1024, used_mem / 1024);
#else
	kprintf("Virtual memory is not managed\n");
#endif
}