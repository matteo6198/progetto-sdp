#include <types.h>
#include <vm_tlb.h>
#include <mips/tlb.h>
#include <vm.h>
#include <lib.h>
#include <spl.h>

int tlb_get_rr_victim(void)
{
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}

void tlb_insert(vaddr_t vaddr, paddr_t paddr, int write){
    int spl, i;
    uint32_t ehi, elo;

	KASSERT((vaddr & PAGE_FRAME)==vaddr);
	KASSERT((paddr & PAGE_FRAME)==paddr);
	
    spl = splhigh();

	i = tlb_get_rr_victim();
	ehi = vaddr & PAGE_FRAME;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	if(write) // write allowed
		elo |= TLBLO_DIRTY;
	DEBUG(DB_VM, "tlb_manage: 0x%x -> 0x%x\n", vaddr, paddr);
	tlb_write(ehi, elo, i);
	splx(spl);
	return;
}
