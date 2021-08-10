#include <pt.h>

#define ERR_CODE 1

struct pt* pagetable;
static int nRamFrames = 0;
static int hash_mask = 0;

/* bootstrap for the page table */
void pt_bootstrap(void){
    nRamFrames = (ram_getsize() / PAGE_SIZE);
    // build mask
    int cnt = 1;
    hash_mask = 1;
    while(cnt <= nRamFrames){
        cnt = cnt << 1;
        hash_mask = (hash_mask << 1) | 1;
    }
    // allocating page table
    pagetable = kmalloc(nRamFrames * sizeof(struct pt));
    if(pagetable == NULL)
        panic("Error allocating pagetable: out of memory.");
    // init page table
    for(cnt = 0; cnt < nRamFrames; cnt++){
        pagetable[cnt].next = NULL;
        pagetable[cnt].entry = NULL;
    }
}

/* returns the index of the page at address v_addr in the pagetable using an hash function */
static int pt_hash(vaddr_t v_addr){
    pid_t pid = curproc->pid;
    int res = (int) (pid * v_addr);
    return res & hash_mask;
}

/* select the victim page among the one in RAM */
static int pt_get_victim(void){
    return 0;
}

/* returns the entry corresponding to the page associated to the address v_addr (if that page is not in memory it will be loaded)*/
int pt_get_page(vaddr_t v_addr){
    v_addr &= PAGE_FRAME;/*
    // ricerca nella PT
    int found = 0;
    struct pt* ptr = pagetable + pt_hash(v_addr);
    while(ptr!=NULL && ptr->entry != NULL){
        if(PT_PID(ptr->entry) == curproc->pid 
            && PT_V_ADDR(ptr->entry) == v_addr){
                found = 1;
                if(PT_RAM(ptr->entry))
                    return TLB_insert(ptr->entry);  // inserisco nella TLB
                else
                    break;
            }
        ptr = ptr->next;
    }
    if(!found){
        // page not found
        return ERR_CODE;
    }

    paddr_t ram_page_number = getFreePages(1);  // ritorna 0 se no frame libere
    if(!ram_page_number){
        int victim = pt_get_victim();   // page replacement 
        int result = swap_out(PT_P_ADDR((&pagetable[victim])->entry));   
        if(!result){
            // impossibile fare swap (frose serve kill al processo corrente)
            return ERR_CODE;
        }
        // aggiornamento PT

        ram_page_number = PT_P_ADDR(pagetable[victim].entry);
    }
    // aggiornamento entry nell PT e inserimento nella TLB
    ptr->entry->lo &= 0xFFF; // clear dei bit
    ptr->entry->lo |= ram_page_number;  // insert del physycal addr
    TLB_insert(ptr->entry);     // la TLB deve essere settata prima di fare le operazioni di lettura

    if(PT_SWAP(ptr->entry)){
        if(!swap_in(v_addr)){
            // stop processo corrente
            return ERR_CODE;
        }

        // aggiornamento PT (set RAM bit, clear SWAP bit)
    }else{
        // load della pagina da disco
        if(!load_page(v_addr)){
            // stop processo corrente 
            return ERR_CODE;
        }
        // aggiornamento PT (set RAM bit)
    }
*/
    return 0;
}

/* insert n_pages pages into the page table without allocating them in RAM */
int pt_insert(vaddr_t v_addr, unsigned int n_pages, int read, int write, int exec){
    unsigned int i;
    struct pt* ptr, *tmp;
    struct pt_entry* entry;
    int flags = 0;
    KASSERT((v_addr && ~PAGE_FRAME) == 0);

    if(read)
        flags |= 1<<3;
    if(write)
        flags |= 1<<4;
    if(exec)
        flags |= 1<<5;

    for(i=0;i<n_pages;i++, v_addr+=PAGE_SIZE){
        entry = kmalloc(sizeof(struct pt_entry));
        if(entry == NULL){
            return 1;
        }
        /* set entry fields */
        entry->hi = (int) v_addr;
        entry->hi &= curproc->pid & 0xFFF;
        entry->lo = 0;
        entry->lo |= flags;

        /* insert into PT */
        ptr = pagetable + pt_hash(v_addr);
        tmp = kmalloc(sizeof(struct pt));
        if(tmp == NULL){
            kfree(entry);
            return 1;
        }
        tmp->entry = entry;
        tmp->next = ptr->next;
        ptr->next = tmp;        
    }
    return 0;
}

/* delete all pages of this process from page table */
void pt_delete_PID(void){
    struct addrspace* as = proc_getas();
    unsigned int i;
    struct pt* tmp, *prev;
    vaddr_t addr;
    /* clean first segment */
    for(i=0, addr = as->as_vbase1;i<as->as_npages1;i++, addr+=PAGE_SIZE){
        prev = pagetable + pt_hash(addr);
        while(prev != NULL && prev->next != NULL &&
                PT_V_ADDR(prev->next->entry) != addr)
            prev = prev->next;
        if(prev == NULL)
            continue;
        tmp = prev->next;
        prev->next = tmp->next;
        kfree(tmp->entry);
        kfree(tmp);
    }
    /* clean second segment */
    for(i=0, addr = as->as_vbase2;i<as->as_npages2;i++, addr+=PAGE_SIZE){
        prev = pagetable + pt_hash(addr);
        while(prev != NULL && prev->next != NULL &&
                PT_V_ADDR(prev->next->entry) != addr)
            prev = prev->next;
        if(prev == NULL)
            continue;
        tmp = prev->next;
        prev->next = tmp->next;
        kfree(tmp->entry);
        kfree(tmp);
    }
    /* clean stack */
    addr = USERSTACK - STACKPAGES * PAGE_SIZE;
    for(i=0;i<STACKPAGES; i++, addr += PAGE_SIZE){
        prev = pagetable + pt_hash(addr);
        while(prev != NULL && prev->next != NULL &&
                PT_V_ADDR(prev->next->entry) != addr)
            prev = prev->next;
        if(prev == NULL)
            continue;
        tmp = prev->next;
        prev->next = tmp->next;
        kfree(tmp->entry);
        kfree(tmp);
    }
}