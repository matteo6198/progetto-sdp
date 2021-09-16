#include <coremap.h>

static unsigned int *allocated_size;
static char *allocated_pages;
static int active = 0;
static unsigned int nRamFrames = 0;
static unsigned int kernPages = 0;
static struct spinlock memSpinLock = SPINLOCK_INITIALIZER;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static void pageSetFree(unsigned int i)
{
	/* setting the bit to 1 means page free */
	allocated_pages[i / 8] |= (1 << (i % 8));
}
void pageSetUsed(unsigned int i)
{
	/* clearing the bit means page used */
	allocated_pages[i / 8] &= ~(1 << (i % 8));
}
static int isPageFree(unsigned int i)
{
	/* check if the bit is 1 */
	return allocated_pages[i / 8] & (1 << (i % 8));
}

paddr_t getFreePages(unsigned int n)
{
	paddr_t addr = 0;
	unsigned int i, count;
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
	spinlock_acquire(&memSpinLock);
	if (active){
		spinlock_release(&memSpinLock);
		addr = pt_getkpages(npages);
	}else{
		spinlock_release(&memSpinLock);
		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
	}
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

	allocated_pages = kmalloc(nRamFrames / 8 * sizeof(char));

	allocated_size = kmalloc(nRamFrames * sizeof(unsigned int));
	if (allocated_pages == NULL || allocated_size == NULL)
	{
		spinlock_release(&memSpinLock);
		return;
	}
	spinlock_acquire(&memSpinLock);

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
	active = 1;
	kernPages = ((start / PAGE_SIZE + CLUSTER_SIZE) / CLUSTER_SIZE) * CLUSTER_SIZE;
	spinlock_release(&memSpinLock);
	kprintf("virtual memory boot completed...\n");
    pt_bootstrap(start / PAGE_SIZE);
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

int return_mem(uint32_t page){
	int i, cnt = 0, n_alloc;
	spinlock_acquire(&memSpinLock);
	/* get number of contiguous pages allocated */
	n_alloc = allocated_size[page];
	allocated_size[page] = 0;
	/* free pages on the bitmap */
	for (i = page; i < (int)page + n_alloc; i++)
	{
		pageSetFree(i);
	}
	i = (kernPages - 1);
	while(i >= 0 && isPageFree(i)){
		i--;
	}
	cnt = (kernPages - 1 ) - i;
	if(cnt / CLUSTER_SIZE >= 2){
		cnt /= CLUSTER_SIZE;
		for(i=cnt*CLUSTER_SIZE; i> 0; i--){
			kernPages--;
			pageSetUsed(kernPages);
		}
	}else{
		cnt = 0;
	}
	spinlock_release(&memSpinLock);
	return cnt;
}

void free_kpages(vaddr_t addr)
{
	unsigned long page;

	if (!active)
	{
		return;
	}

	page = KVADDR_2_PADDR(addr) / PAGE_SIZE;
	KASSERT(page < nRamFrames);

	pt_freekpages(page);
}

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("Tried to do tlb shootdown?!\n");
}


void memstats(void)
{
	unsigned long free_pages, i, free_mem, used_mem;
	spinlock_acquire(&memSpinLock);
	if (!active)
	{
		spinlock_release(&memSpinLock);
		kprintf("Memeory allocation table is not yet active\n");
		return;
	}
	kprintf("Memory view:\n");
	for (i = free_pages = 0; i < kernPages; i++)
	{
		if (isPageFree(i))
		{
			free_pages++;
			kprintf("F ");
		}
		else
			kprintf("U ");
	}
	spinlock_release(&memSpinLock);
	kprintf("|");
	free_pages += pt_stats();
	kprintf("\n");

	free_mem = free_pages * PAGE_SIZE;
	used_mem = nRamFrames * PAGE_SIZE - free_mem;
	kprintf("Free memory:\t%lu kB\nUsed memory:\t%lu kB\n", free_mem / 1024, used_mem / 1024);
	kprintf("Kernel memory:\t%u kB\n", kernPages * PAGE_SIZE / 1024);
}

void free_ppage(paddr_t paddr){
	unsigned long page;
	KASSERT((paddr & PAGE_FRAME) == paddr);

	page = (unsigned long) paddr / PAGE_SIZE;
	spinlock_acquire(&memSpinLock);

	if (!active)
	{
		spinlock_release(&memSpinLock);
		return;
	}
	allocated_size[page] = 0;
	pageSetFree(page);

	kernPages++;
	spinlock_release(&memSpinLock);

}