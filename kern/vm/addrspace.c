/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <kern/errno.h>
#include <types.h>
#include <lib.h>
#include <proc.h>
#include <pt.h>
#include <addrspace.h>
#include <vm.h>
#include <vm_tlb.h>
#include <opt-ondemand_manage.h>
#include <opt-tlb_manage.h>
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

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as == NULL)
	{
		return NULL;
	}

	as->as_vbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_npages2 = 0;
#if OPT_ONDEMAND_MANAGE
	as->as_offset1 = 0;
	as->as_filesize1 = 0;
	as->as_offset2 = 0;
	as->as_filesize2 = 0;
	as->as_flags = 0;
#else
	as->as_pbase1 = 0;
	as->as_pbase2 = 0;
	as->as_stackpbase = 0;
#endif
	return as;
}

void as_destroy(struct addrspace *as)
{
	vm_can_sleep();
#if OPT_ONDEMAND_MANAGE
	pt_delete_PID(as);
#else
	free_kpages(PADDR_TO_KVADDR(as->as_pbase1));
	free_kpages(PADDR_TO_KVADDR(as->as_pbase2));
	free_kpages(PADDR_TO_KVADDR(as->as_stackpbase));
#endif
	kfree(as);
}

void as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL)
	{
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++)
	{
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void as_deactivate(void)
{
	/* nothing */
}

#if OPT_ONDEMAND_MANAGE
int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
					 int readable, int writeable, int executable,
					 off_t offset, size_t filesize)
{
	size_t npages;

	vm_can_sleep();

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	if (pt_insert(vaddr, npages, readable, writeable, executable))
	{
		return ENOSYS;
	}

	if (as->as_vbase1 == 0)
	{
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		as->as_filesize1 = filesize;
		as->as_offset1 = offset;
		if (readable)
			as->as_flags |= 1 << 2;
		if (writeable)
			as->as_flags |= 1 << 1;
		if (executable)
			as->as_flags |= 1;
		return 0;
	}

	if (as->as_vbase2 == 0)
	{
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		as->as_filesize2 = filesize;
		as->as_offset2 = offset;
		if (readable)
			as->as_flags |= 1 << 5;
		if (writeable)
			as->as_flags |= 1 << 4;
		if (executable)
			as->as_flags |= 1 << 3;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("Warning: too many regions\n");
	return ENOSYS;
}
#else
int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
					 int readable, int writeable, int executable)
{
	size_t npages;

	vm_can_sleep();

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0)
	{
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0)
	{
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}
#endif
# if !OPT_ONDEMAND_MANAGE
static void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}
#endif

#if !OPT_ONDEMAND_MANAGE
int as_prepare_load(struct addrspace *as)
{
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	vm_can_sleep();

	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0)
	{
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0)
	{
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0)
	{
		return ENOMEM;
	}

	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	return 0;
}
#endif

int as_complete_load(struct addrspace *as)
{
	vm_can_sleep();
	(void)as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	(void)as;
#if OPT_ONDEMAND_MANAGE
	//TODO: what to do with as?
	pt_insert(USERSTACK - STACKPAGES * PAGE_SIZE, STACKPAGES, 1, 1, 0);
#else
	KASSERT(as->as_stackpbase != 0);
#endif
	*stackptr = USERSTACK;
	return 0;
}

int as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	vm_can_sleep();

	new = as_create();
	if (new == NULL)
	{
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

#if OPT_ONDEMAND_MANAGE
	new->as_offset2 = old->as_offset2;
	new->as_filesize2 = old->as_filesize2;
	new->as_offset1 = old->as_offset1;
	new->as_filesize1 = old->as_filesize1;
	new->as_flags = old->as_flags;

	pt_insert(new->as_vbase1, new->as_npages1, (new->as_flags & 4), (new->as_flags & 2), (new->as_flags & 1));
	pt_insert(new->as_vbase2, new->as_npages2, new->as_flags & 0x20, new->as_flags & 0x10, new->as_flags & 8);
	pt_insert(USERSTACK - STACKPAGES * PAGE_SIZE, STACKPAGES, 1, 1, 0);
#endif

#if !OPT_ONDEMAND_MANAGE
	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new))
	{
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
			(const void *)PADDR_TO_KVADDR(old->as_pbase1),
			old->as_npages1 * PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
			(const void *)PADDR_TO_KVADDR(old->as_pbase2),
			old->as_npages2 * PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
			(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
			DUMBVM_STACKPAGES * PAGE_SIZE);
#endif

	*ret = new;
	return 0;
}

#if OPT_TLB_MANAGE
int vm_fault(int faulttype, vaddr_t faultaddress)
{
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;
	uint8_t flags;

	faultaddress &= PAGE_FRAME;

	//DEBUG(DB_VM, "tlb_manage: fault: 0x%x\n", faultaddress);

	switch (faulttype)
	{
	case VM_FAULT_READONLY:
		return EFAULT;
	case VM_FAULT_READ:
	case VM_FAULT_WRITE:
		break;
	default:
		return EINVAL;
	}

	if (curproc == NULL)
	{
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL)
	{
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);

	paddr = pt_get_page(faultaddress, &flags);
	if (paddr == ERR_CODE)
	{
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	i = tlb_get_rr_victim();
	ehi = faultaddress;
	elo = paddr | TLBLO_VALID;
	if((flags & 0x2)) // write allowed
		elo |= TLBLO_DIRTY;
	DEBUG(DB_VM, "tlb_manage: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_write(ehi, elo, i);
	splx(spl);
	return 0;
}

#endif