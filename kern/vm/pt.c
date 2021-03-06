#include <pt.h>

pt_entry *pagetable;
static int nClusters = 0;
static int start_cluster = 0;
struct spinlock pt_lock = SPINLOCK_INITIALIZER;

/* bootstrap for the page table */
void pt_bootstrap(int first_free)
{
    int cnt = 1;
    first_free = (first_free + CLUSTER_SIZE) / CLUSTER_SIZE;
    nClusters = ((ram_getsize() / PAGE_SIZE) - (first_free * CLUSTER_SIZE)) / CLUSTER_SIZE;
    start_cluster = first_free;
    // allocating page table
    pagetable = kmalloc(nClusters * CLUSTER_SIZE * sizeof(pt_entry));
    if (pagetable == NULL)
        panic("Error allocating pagetable: out of memory.");
    // init page table
    for (cnt = 0; cnt < nClusters * CLUSTER_SIZE; cnt++)
    {
        pagetable[cnt] = 0;
        pageSetUsed(cnt + start_cluster * CLUSTER_SIZE);
    }
}

/* returns the index of the page at address v_addr in the pagetable using an hash function */
static int pt_hash(vaddr_t v_addr, pid_t pid)
{
    int res = (int)(v_addr >> 12);
    return (res * pid) % nClusters;
}

/* returns the entry corresponding to the page associated to the address v_addr (if that page is not in memory it will be loaded)
*/
int pt_get_page(vaddr_t v_addr)
{
    v_addr &= PAGE_FRAME;
    pid_t pid = curproc->pid;
    int i, write, exec, first_free = -1;
    struct addrspace *as = proc_getas();

    // get segment of v_addr to get flags
    if (v_addr >= as->as_vbase1 && v_addr < as->as_vbase1 + as->as_npages1 * PAGE_SIZE)
    {
        // segment 1
        write = as->as_flags & 0x2;
        exec = as->as_flags & 0x1;
    }
    else if (v_addr >= as->as_vbase2 && v_addr < as->as_vbase2 + as->as_npages2 * PAGE_SIZE)
    {
        // segment 2
        write = as->as_flags & 0x10;
        exec = as->as_flags & 0x8;
    }
    else if (v_addr >= USERSTACK - STACKPAGES * PAGE_SIZE && v_addr < USERSTACK)
    {
        // stack
        exec = 0;
        write = 1;
    }
    else
    {
        // page out of segments
        return ERR_CODE;
    }

    // ricerca nella PT
    pt_entry *ptr = pagetable + pt_hash(v_addr, pid) * CLUSTER_SIZE;
    spinlock_acquire(&pt_lock);
    for (i = 0; i < CLUSTER_SIZE; i++)
    {
        if (PT_V_ADDR(ptr[i]) == v_addr && PT_PID(ptr[i]) == pid)
        {
            spinlock_release(&pt_lock);
            tlb_insert(v_addr, PT_P_ADDR((ptr - pagetable) + i + start_cluster * CLUSTER_SIZE));
            // update stats
            vms_update(VMS_RELOAD);
            return 0;
        }
        else if (first_free < 0 && ptr[i] == 0)
        {
            first_free = i;
        }
    }

    if (i >= CLUSTER_SIZE && first_free < 0)
    {
        // swap out
        i = random() % CLUSTER_SIZE;
        if (PT_DIRTY(ptr[i]))
        {
            // swap out physical page i
            spinlock_release(&pt_lock);
            //inside swap_out we acqurie another spinlock
            swap_out(PT_V_ADDR(ptr[i]), PT_P_ADDR((ptr - pagetable) + i + start_cluster * CLUSTER_SIZE), PT_PID(ptr[i])); 
            spinlock_acquire(&pt_lock);
        }
        // invalid tlb entry
        int j = tlb_probe(PT_V_ADDR(ptr[i]), 0);
        if (j >= 0)
        {
            tlb_write(TLBHI_INVALID(j), TLBLO_INVALID(), j);
        }
    }
    else
    {
        i = first_free;
    }
    ptr[i] = v_addr | (pid << 1);
    if (write)
        ptr[i] |= 1;

    spinlock_release(&pt_lock);

    tlb_insert(v_addr, PT_P_ADDR((ptr - pagetable) + i + start_cluster * CLUSTER_SIZE)); // la TLB deve essere settata prima di fare le operazioni di lettura

    if (!swap_in(v_addr, pid, SWAP_LOAD))
    {
        if (load_page(v_addr, exec))
        {
            // stop processo corrente
            return ERR_CODE;
        }
    }

    if (!write)
    {
        uint32_t pos = tlb_probe(v_addr, 0);
        tlb_write(v_addr, PT_P_ADDR((ptr - pagetable) + i + start_cluster * CLUSTER_SIZE) | TLBLO_VALID, pos);
    }

    return 0;
}

/* delete all pages of this process from page table */
void pt_delete_PID(struct addrspace *as, pid_t pid)
{
    unsigned int i, j, found = 0;
    pt_entry *ptr;
    vaddr_t addr;

    /* clean first segment */
    for (i = 0, addr = as->as_vbase1; i < as->as_npages1; i++, addr += PAGE_SIZE)
    {
        ptr = pagetable + pt_hash(addr, pid) * CLUSTER_SIZE;
        spinlock_acquire(&pt_lock);
        for (j = 0; j < CLUSTER_SIZE; j++)
        {
            if (PT_V_ADDR(*(ptr + j)) == addr && PT_PID(*(ptr + j)) == pid)
            {
                *(ptr + j) = 0;
                found = 1;
                break;
            }
        }
        spinlock_release(&pt_lock);
        if (!found)
        {
            // remove from swap if present
            swap_in(addr, pid, SWAP_DISCARD);
        }
    }
    /* clean second segment */
    found = 0;
    for (i = 0, addr = as->as_vbase2; i < as->as_npages2; i++, addr += PAGE_SIZE)
    {
        ptr = pagetable + pt_hash(addr, pid) * CLUSTER_SIZE;
        spinlock_acquire(&pt_lock);
        for (j = 0; j < CLUSTER_SIZE; j++)
        {
            if (PT_V_ADDR(*(ptr + j)) == addr && PT_PID(*(ptr + j)) == pid)
            {
                *(ptr + j) = 0;
                found = 1;
                break;
            }
        }
        spinlock_release(&pt_lock);
        if (!found)
        {
            // remove from swap if present
            swap_in(addr, pid, SWAP_DISCARD);
        }
    }
    /* clean stack */
    addr = USERSTACK - STACKPAGES * PAGE_SIZE;
    for (i = 0; i < STACKPAGES; i++, addr += PAGE_SIZE)
    {
        ptr = pagetable + pt_hash(addr, pid) * CLUSTER_SIZE;
        spinlock_acquire(&pt_lock);
        for (j = 0; j < CLUSTER_SIZE; j++)
        {
            if (PT_V_ADDR(*(ptr + j)) == addr && PT_PID(*(ptr + j)) == pid)
            {
                *(ptr + j) = 0;
                found = 1;
                break;
            }
        }
        spinlock_release(&pt_lock);
        if (!found)
        {
            // remove from swap if present
            swap_in(addr, pid, SWAP_DISCARD);
        }
    }
}

paddr_t pt_getkpages(uint32_t n_pages)
{
    unsigned int i, tmp_start_cluster, tmp_nClusters;
    paddr_t paddr;
    unsigned int n_cluster_to_allocate = (n_pages + CLUSTER_SIZE) / CLUSTER_SIZE;
    spinlock_acquire(&pt_lock);

    paddr = getFreePages(n_pages);

    if(paddr != 0){
        spinlock_release(&pt_lock);
        return paddr;
    }

    if(nClusters - (int)n_cluster_to_allocate < 0){
        n_cluster_to_allocate = nClusters;
    }
    tmp_start_cluster = start_cluster;
    tmp_nClusters = nClusters;

    start_cluster += n_cluster_to_allocate;
    nClusters -= n_cluster_to_allocate;

    tlb_invalidate();
    for (i = (tmp_start_cluster) * CLUSTER_SIZE; i < (unsigned int)(tmp_start_cluster + n_cluster_to_allocate) * CLUSTER_SIZE; i++){
        free_ppage(i * PAGE_SIZE);
    }
    paddr = getFreePages(n_pages);
    if(paddr == 0){
        spinlock_release(&pt_lock);
        panic("Out of memory!\n");
    }
    
    for (i = 0; i < (unsigned int)tmp_nClusters * CLUSTER_SIZE; i++)
    {
        pt_entry entry = pagetable[i];
        pagetable[i] = 0;
        if (entry != 0 && PT_DIRTY(entry))
        {
            // swap out
            spinlock_release(&pt_lock);
            swap_out(PT_V_ADDR(entry), PT_P_ADDR(i + tmp_start_cluster * CLUSTER_SIZE), PT_PID(entry));
            spinlock_acquire(&pt_lock);
        }
    }
    spinlock_release(&pt_lock);
    return paddr;
}

void pt_freekpages(uint32_t page)
{
    unsigned int i, tmp_start_cluster, n_clusters;
    spinlock_acquire(&pt_lock);

    n_clusters = return_mem(page);
    if(nClusters == 0){
        spinlock_release(&pt_lock);
        return;
    }
    start_cluster -= n_clusters;
    nClusters += n_clusters;
    tmp_start_cluster = start_cluster;
    for (i = 0; i < (unsigned int)nClusters * CLUSTER_SIZE; i++)
    {
        pt_entry entry = pagetable[i];
        pagetable[i] = 0;
        if (entry != 0 && PT_DIRTY(entry))
        {
            // swap out
            spinlock_release(&pt_lock);
            swap_out(PT_V_ADDR(entry), PT_P_ADDR(i + tmp_start_cluster * CLUSTER_SIZE), PT_PID(entry));
            spinlock_acquire(&pt_lock);
        }
    }
    tlb_invalidate();
    spinlock_release(&pt_lock);
}

int pt_stats(void)
{
    int i, pfree = 0;

    spinlock_acquire(&pt_lock);
    for (i = 0; i < nClusters * CLUSTER_SIZE; i++)
    {
        if (pagetable[i] == 0)
        {
            pfree++;
            kprintf("F ");
        }
        else
        {
            kprintf("U ");
        }
    }
    spinlock_release(&pt_lock);
    return pfree;
}