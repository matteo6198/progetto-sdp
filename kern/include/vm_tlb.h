#ifndef _VM_TLB_H_
#define _VM_TLB_H_

int tlb_get_rr_victim(void);
void tlb_insert(vaddr_t vaddr, paddr_t paddr);
void tlb_invalidate(void);
#endif /* _VM_TLB_H_ */