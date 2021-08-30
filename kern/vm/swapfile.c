#include <swapfile.h>
#define HASH_SIZE 101
#define FREE_SIZE (SWAP_FILESIZE / PAGE_SIZE / 8)

#define HASH_ROW_INDEX(entry) ((entry->swap_offset / PAGE_SIZE) / 8)
#define HASH_COL_INDEX(entry) ((entry->swap_offset / PAGE_SIZE) % 8)

#define SWAP_ENTRYPID(entry) ((int)(entry->v_addr & 0x7FF))
#define SWAP_ENTRYVADDR(entry) (entry->v_addr & PAGE_FRAME)

struct hash_entry
{
    vaddr_t v_addr;
    //    pid_t pid;
    uint32_t swap_offset;
    struct hash_entry *next;
};

struct vnode *swapfile;

struct hash_entry **hash_table = NULL;

//create a bitmap as a char array to track the free space
char *freespace = NULL;

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

    //allocate 288 entries for freespace, each entry has 8 bit,
    /*
    n = 56
    i = 56 / 8
    j = 56 % 8
    isAllocated = freespace[i] & (1 << j)
    */
    //freespace[i]<<7==0-->the last block of the i-th page is free in the swapfile
    freespace = kmalloc(FREE_SIZE);

    if (freespace == NULL)
    {
        panic("swap_bootstrap: Error allocating freespace bitmap\n");
    }

    for (i = 0; i < FREE_SIZE; i++)
    {
        freespace[i] = 0;
    }

    hash_table = kmalloc(HASH_SIZE * sizeof(struct hash_entry *));

    if (hash_table == NULL)
    {
        panic("swap_bootstrap: Error allocating hash table\n");
    }

    for (i = 0; i < HASH_SIZE; i++)
    {
        hash_table[i] = NULL;
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
    int hash_ret;
    struct hash_entry *node = NULL;
    struct hash_entry *node_temp = NULL;

    hash_ret = hash_swap(v_addr, pid);
    node = hash_table[hash_ret];

    if (node == NULL)
    {
        return 0;
    }

    if (SWAP_ENTRYPID(node) == pid && SWAP_ENTRYVADDR(node) == v_addr)
    {
        hash_table[hash_ret] = node->next;
        node_temp = node;
    }
    else
    {
        for (; (SWAP_ENTRYPID(node->next) != pid || SWAP_ENTRYVADDR(node->next) != v_addr) || node->next != NULL; node = node->next)
            ;

        if (node->next != NULL)
        {
            node_temp = node->next;
            node->next = node_temp->next;
        }
        else
        {
            return 0;
        }
    }

    if (store == SWAP_LOAD && swap_read(node_temp->swap_offset, SWAP_ENTRYVADDR(node_temp)) != 0)
    {
        panic("Error while reading on the swapfile.\n");
    }

    freespace[HASH_ROW_INDEX(node_temp)] &= ~(1 << HASH_COL_INDEX(node_temp));
    kfree(node_temp);

    return 1;
}

void swap_out(vaddr_t v_addr, paddr_t p_addr, pid_t pid)
{
    int foundROW = -1;
    int foundCOLUMN = -1;
    int i, j;
    int hash_ret = -1;
    struct hash_entry *node = NULL;

    v_addr &= PAGE_FRAME;
    v_addr |= pid;

    for (i = 0; i < FREE_SIZE && foundROW == -1; i++)
    {
        if (freespace[i] == (char)0xFF)
            continue;
        for (j = 0; j < 8; j++)
        {
            if ((freespace[i] & (1 << j)) == 0)
            {
                foundROW = i;
                foundCOLUMN = j;
                //set at 1 freespace[i] at the j-th bit
                freespace[i] |= (1 << j);
                break;
            }
        }
    }

    if (foundROW == -1 && foundCOLUMN == -1)
    {
        panic("Out of swap space");
    }

    hash_ret = hash_swap(v_addr, pid);
    node = hash_table[hash_ret];

    if (node == NULL)
    {
        node = kmalloc(sizeof(struct hash_entry));
        if (node == NULL)
        {
            panic("swap_bootstrap: Error allocating hash table entry\n");
        }
    }
    else
    {
        while (node->next != NULL)
        {
            node = node->next;
        }

        node->next = kmalloc(sizeof(struct hash_entry));
        if (node->next == NULL)
        {
            panic("swap_bootstrap: Error allocating hash table entry\n");
        }
        node = node->next;
    }

    node->next = NULL;
    node->v_addr = v_addr;
    //da debuggare
    node->swap_offset = (foundROW * 8 + foundCOLUMN) * PAGE_SIZE;

    if (swap_write(node->swap_offset, PADDR_TO_KVADDR(p_addr)))
    {
        panic("Error while writing on the swapfile.\n");
    }
}
