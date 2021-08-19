#include <pt.h>

pt_entry* pagetable;
static int nClusters = 0;
static int start_cluster = 0;
//static int hash_mask = 0;
struct spinlock pt_lock = SPINLOCK_INITIALIZER;

/* bootstrap for the page table */
void pt_bootstrap(int first_free){
    first_free = (first_free + CLUSTER_SIZE) / CLUSTER_SIZE;
    nClusters = ((ram_getsize() / PAGE_SIZE) - (first_free * CLUSTER_SIZE)) / CLUSTER_SIZE;
    start_cluster = first_free;
    // build mask
    int cnt = 1;
    // allocating page table
    pagetable = kmalloc(nClusters * CLUSTER_SIZE * sizeof(pt_entry));
    if(pagetable == NULL)
        panic("Error allocating pagetable: out of memory.");
    // init page table
    for(cnt = 0; cnt < nClusters * CLUSTER_SIZE; cnt++){
        pagetable[cnt] = 0;
        pageSetUsed(cnt+start_cluster * CLUSTER_SIZE);
    }
}

/* returns the index of the page at address v_addr in the pagetable using an hash function */
static int pt_hash(vaddr_t v_addr){
    int res = (int) (v_addr >> 12);
    return res % nClusters;
}

/* returns the entry corresponding to the page associated to the address v_addr (if that page is not in memory it will be loaded)
*/
int pt_get_page(vaddr_t v_addr){
    v_addr &= PAGE_FRAME;
    pid_t pid = curproc->pid;
    int i, write, exec, first_free = -1;
    struct addrspace* as = proc_getas();

        // get segment of v_addr to get flags
    if(v_addr >= as->as_vbase1 && v_addr < as->as_vbase1 + as->as_npages1 * PAGE_SIZE){
        // segment 1
        write = as->as_flags & 0x2;
        exec = as->as_flags & 0x1;
    }else if(v_addr >= as->as_vbase2 && v_addr < as->as_vbase2 + as->as_npages2 * PAGE_SIZE){
        // segment 2
        write = as->as_flags & 0x10;
        exec = as->as_flags & 0x8;
    }else if(v_addr >= USERSTACK - STACKPAGES * PAGE_SIZE && v_addr < USERSTACK){
        // stack 
        exec = 0;
        write = 1;
    }else{
        // page out of segments
        return ERR_CODE;
    }

    // ricerca nella PT
    pt_entry* ptr = pagetable + pt_hash(v_addr) * CLUSTER_SIZE;
    spinlock_acquire(&pt_lock);
    for(i=0;i<CLUSTER_SIZE;i++){
        if(PT_V_ADDR(*(ptr+i)) == v_addr && PT_PID(*(ptr + i)) == pid){
            spinlock_release(&pt_lock);
            tlb_insert(v_addr, PT_P_ADDR((ptr-pagetable)+i+ start_cluster * CLUSTER_SIZE));
            return 0;
        }else if(first_free < 0 && *(ptr + i) == 0){
            first_free = i;
        }
    }

    if(i>= CLUSTER_SIZE && first_free < 0){
        // swap out
        i = random() % CLUSTER_SIZE;
        // swap out physical page i
    }else{
        i = first_free;
    }
    *(ptr + i) = v_addr | pid;

    spinlock_release(&pt_lock);

    tlb_insert(v_addr, PT_P_ADDR((ptr-pagetable)+i+ start_cluster * CLUSTER_SIZE));     // la TLB deve essere settata prima di fare le operazioni di lettura
    
    bzero((void *)v_addr, PAGE_SIZE);

    //if(!swap_in(v_addr, pid)){
        if(load_page(v_addr, exec)){
            // stop processo corrente 
            return ERR_CODE;
        }
    //}

    if(!write){
        uint32_t pos = tlb_probe(v_addr, PT_P_ADDR((ptr-pagetable)+i+ start_cluster * CLUSTER_SIZE));
        tlb_write(v_addr, PT_P_ADDR((ptr-pagetable)+i+ start_cluster * CLUSTER_SIZE) | TLBLO_VALID, pos);
    }

    return 0;
}

/* delete all pages of this process from page table */
void pt_delete_PID(struct addrspace *as, pid_t pid){
    unsigned int i, j, found=0;
    pt_entry* ptr;
    vaddr_t addr;

    /* clean first segment */
    for(i=0, addr = as->as_vbase1;i<as->as_npages1;i++, addr+=PAGE_SIZE){
        ptr = pagetable + pt_hash(addr) * CLUSTER_SIZE;
        spinlock_acquire(&pt_lock);
        for(j=0;j<CLUSTER_SIZE;j++){
            if(PT_V_ADDR(*(ptr+j)) == addr && PT_PID(*(ptr + j)) == pid){
                *(ptr+j) = 0;
                found = 1;
                break;
            }
        }
        spinlock_release(&pt_lock);
        if(!found){
            // remove from swap if present

        }
    }
    /* clean second segment */
    found = 0;
    for(i=0, addr = as->as_vbase2;i<as->as_npages2;i++, addr+=PAGE_SIZE){
        ptr = pagetable + pt_hash(addr) * CLUSTER_SIZE;
        spinlock_acquire(&pt_lock);
        for(j=0;j<CLUSTER_SIZE;j++){
            if(PT_V_ADDR(*(ptr+j)) == addr && PT_PID(*(ptr + j)) == pid){
                *(ptr+j) = 0;
                found = 1;
                break;
            }
        }
        spinlock_release(&pt_lock);
        if(!found){
            // remove from swap if present

        }
    }
    /* clean stack */
    addr = USERSTACK - STACKPAGES * PAGE_SIZE;
    for(i=0;i<STACKPAGES; i++, addr += PAGE_SIZE){
        ptr = pagetable + pt_hash(addr) * CLUSTER_SIZE;
        spinlock_acquire(&pt_lock);
        for(j=0;j<CLUSTER_SIZE;j++){
            if(PT_V_ADDR(*(ptr+j)) == addr && PT_PID(*(ptr + j)) == pid){
                *(ptr+j) = 0;
                found = 1;
                break;
            }
        }
        spinlock_release(&pt_lock);
        if(!found){
            // remove from swap if present

        }
    }

}

void pt_getkpages(uint32_t n){
    unsigned int i;
    n = (n + CLUSTER_SIZE) / CLUSTER_SIZE;
    spinlock_acquire(&pt_lock);
    for(i=start_cluster * CLUSTER_SIZE; i < (start_cluster + n) * CLUSTER_SIZE; i++){
        if(*(pagetable + i) != 0){
            // swap out
        }
    }
    start_cluster += n;
    nClusters -= n;
    spinlock_release(&pt_lock);

    for(i=(start_cluster - n)* CLUSTER_SIZE; i < (unsigned int)start_cluster * CLUSTER_SIZE; i++)
        free_ppage(i * PAGE_SIZE);
}

int pt_stats(void){
    int i, pfree = 0;

    spinlock_acquire(&pt_lock);
    for(i=0;i<nClusters*CLUSTER_SIZE;i++){
        if(pagetable[i] == 0){
            pfree++;
            kprintf("F ");
        }else{
            kprintf("U ");
        }
    }
    spinlock_release(&pt_lock);
    return pfree;
}