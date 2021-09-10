#include <swapfile.h>
#define HASH_SIZE (SWAP_FILESIZE/PAGE_SIZE)

#define SWAP_ENTRYPID(entry) ((int)(entry & 0x7FF))
#define SWAP_ENTRYVADDR(entry) (entry & PAGE_FRAME)

typedef uint32_t hash_entry;

struct vnode *swapfile;

struct hash_entry *hash_table = NULL;



/*  hash_swap
    pid_t        pid: pid del processo
    vaddr_t   v_addr: indirizzo logico della pagina da cercare 
                      nell'hash table
    Ritorna il valore hash associato ai valori in input per
    la ricerca della pagina nello swap file
*/
static int hash_swap(vaddr_t v_addr, pid_t pid)
{
    int res = (int)(v_addr >> 12);
    return (res * pid) % HASH_SIZE;
}

/*  swap_write
    int       offset: è l'offset a cui scriviamo all'interno dello swapfile
    vaddr_t    vaddr: indirizzo logico della pagina da scrivere nello swapfile
    Ritorna 0 in caso di successo
*/
static int swap_write(int offset, vaddr_t v_addr)
{
    struct iovec iov;
    struct uio u;
    int result;

    uio_kinit(&iov, &u, (void *)v_addr, PAGE_SIZE, offset, UIO_WRITE);

    result = VOP_WRITE(swapfile, &u);
    if (result)
    {
        return result;
    }
    return 0;
}

/*  swap_read
    int       offset: è l'offset a cui scriviamo all'interno dello swapfile
    vaddr_t    vaddr: indirizzo logico della pagina da scrivere nello swapfile
    Ritorna 0 in caso di successo
*/
static int swap_read(int offset, vaddr_t v_addr)
{
    struct iovec iov;
    struct uio u;
    int result;

    iov.iov_ubase = (userptr_t)v_addr;
    iov.iov_len = PAGE_SIZE;

    u.uio_iov = &iov;
    u.uio_iovcnt = 1;
    u.uio_resid = PAGE_SIZE;
    u.uio_offset = offset;
    u.uio_segflg = UIO_USERISPACE;
    u.uio_rw = UIO_READ;
    u.uio_space = proc_getas();

    result = VOP_READ(swapfile, &u);
    if (result)
    {
        return result;
    }
    return 0;
}

void swap_bootstrap(void)
{

    int result;
    int i;

    
    hash_table = kmalloc(HASH_SIZE*sizeof(hash_entry));

    if (hash_table == NULL)
    {
        panic("swap_bootstrap: Error allocating hash table\n");
    }

    
    //open the swapfile
    // "/SWAPFILE" because kernel proc has no cwd
    result = vfs_open((char *)"/SWAPFILE", O_RDWR | O_CREAT | O_TRUNC, 0, &swapfile);
    if (result)
    {
        panic("Error opening SWAPFILE\n");
    }

    //TODO
    /*of->lock=lock_create("file lock");*/
}

int swap_in(vaddr_t v_addr, pid_t pid, uint8_t store)
{
    uint32_t res;
    int hash_ret;
    struct hash_entry *node = NULL;
    struct hash_entry *node_temp = NULL;
    int i, j;

    hash_ret = hash_swap(v_addr, pid);
    
    for(i=0, j=hash_ret; i<HASH_SIZE; i++, j=(hash_ret+i)%HASH_SIZE) {
        if(SWAP_ENTRYVADDR(hash_table[j])==v_addr && SWAP_ENTRYPID(hash_table[j])==pid) {
            break;
        }

        else if(hash_table[j]==0) {
            return 0;
        }
    }

    //offset=j*PAGE_SIZE
    if (store == SWAP_LOAD && swap_read(j*PAGE_SIZE, SWAP_ENTRYVADDR(hash_table[j])) != 0)
    {
        panic("Error while reading on the swapfile.\n");
    }

    hash_entry[j]=-1;

    return 1;
}

void swap_out(vaddr_t v_addr, paddr_t p_addr, pid_t pid)
{
    int i, j;
    int hash_ret = -1;
    
    v_addr &= PAGE_FRAME;
    v_addr |= pid;


    hash_ret = hash_swap(v_addr, pid);

    for(i=0, j=hash_ret; i<HASH_SIZE; i++, j=(hash_ret+i)%HASH_SIZE) {
        if(hash_table[j]==0 || hash_table[j]==-1) {
            break;
        }
    }

    
    if (i==HASH_SIZE)
    {
        panic("Out of swap space");
    }

    hash_table[j]=v_addr;

    if (swap_write(j*PAGE_SIZE, PADDR_TO_KVADDR(p_addr)))
    {
        panic("Error while writing on the swapfile.\n");
    }

}
