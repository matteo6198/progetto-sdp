#include <vm_tlb.h>
#include <tlb.h>

int tlb_get_rr_victim(void)
{
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}