#include <swapfile.h>
#define HASH_SIZE 101
#define FREE_SIZE 288

struct openfile {
    struct vnode *vn;
    off_t offset;
    unsigned int countref;
};

struct hash_entry {
    pid_t pid;
    vaddr_t v_addr;
    int swap_offset;
    struct hash_entry* next;
    //these two are for indexing into the freespace vector
    int char_indx;
    int bit_indx;
};

struct openfile swapfile;

struct hash_entry **hash_table=NULL;

//create a bitmap as a char array to track the free space
char* freespace=NULL;

uint8_t vett[8]={1, 2, 4, 8, 16, 32, 64, 128};


/*  hash_swap
    pid_t        pid: pid del processo
    vaddr_t   v_addr: indirizzo logico della pagina da cercare 
                      nell'hash table
    Ritorna il valore hash associato ai valori in input per
    la ricerca della pagina nello swap file
*/
static int hash_swap(vaddr_t v_addr, pid_t pid) 
{
    int res = (int) (v_addr >> 12);
    return (res * pid) % HASH_SIZE;
}


/*  swap_write
    int       offset: è l'offset a cui scriviamo all'interno dello swapfile
    vaddr_t    vaddr: indirizzo logico della pagina da scrivere nello swapfile
    Ritorna il numero di byte scritti sullo swapfile
*/
static int swap_write(int offset, vaddr_t v_addr)
{
    struct iovec iov;
    struct uio u;
    int result;
    
    iov.iov_ubase= (userptr_t)v_addr;
    iov.iov_len=PAGE_SIZE;

    u.uio_iov=&iov;
    u.uio_iovcnt=1;
    u.uio_resid=PAGE_SIZE;
    u.uio_offset=offset;
    u.uio_segflg=UIO_SYSSPACE;
    u.uio_rw=UIO_WRITE;
    u.uio_space=NULL;
    
    result=VOP_WRITE(swapfile.vn, &u);
    if(result) {
        return result;
    }
    return PAGE_SIZE;
}

/*  swap_read
    int       offset: è l'offset a cui scriviamo all'interno dello swapfile
    vaddr_t    vaddr: indirizzo logico della pagina da scrivere nello swapfile
    Ritorna il numero di byte letti dallo swapfile
*/
static int swap_read(int offset, vaddr_t v_addr)
{
    struct iovec iov;
    struct uio u;
    int result;
    
    iov.iov_ubase=(userptr_t)v_addr;
    iov.iov_len=PAGE_SIZE;

    u.uio_iov=&iov;
    u.uio_iovcnt=1;
    u.uio_resid=PAGE_SIZE;
    u.uio_offset=offset;
    u.uio_segflg=UIO_USERISPACE;
    u.uio_rw=UIO_READ;
    u.uio_space=proc_getas();
    
    result=VOP_READ(swapfile.vn, &u);
    if(result) {
        return result;
    }
    return PAGE_SIZE;
}

void swap_bootstrap(void)
{
    
    int result;
    struct vnode *v;
    int i;
    
    //allocate 288 entries for freespace (2308/sizeof(char), to have 2308 bit)
    //freespace[i]<<7==0-->the last block of the i-th page is free in the swapfile
    freespace=kmalloc(FREE_SIZE * sizeof(char));

    if(freespace == NULL){
        kprintf("%d\n", (int)freespace);
        panic("swap_bootstrap: Error allocating space\n");
    }

    for(i=0;i<FREE_SIZE;i++){
        freespace[i] = 0;
    }

    hash_table=kmalloc(HASH_SIZE*sizeof(struct hash_entry*));
    
    if(hash_table == NULL){
        panic("swap_bootstrap: Error allocating space\n");
    }

    for(i=0; i<HASH_SIZE; i++) {
        hash_table[i]=kmalloc(sizeof(struct hash_entry));
        hash_table[i]->next=NULL;
    }
    //open the swapfile
    // "/SWAPFILE" because kernel proc has no cwd
    result=vfs_open((char *)"/SWAPFILE", O_RDWR | O_CREAT | O_TRUNC, 0, &v);
    if(result) {
        panic("Error opening SWAPFILE\n");
    }
    
    //TODO
    /*of->lock=lock_create("file lock");*/
    
    swapfile.vn=v;
    swapfile.offset=0;
    swapfile.countref=1;

}

int swap_in(vaddr_t v_addr, pid_t pid, uint8_t store)
{
    int hash_ret;
    struct hash_entry* node=NULL;
    struct hash_entry* node_temp=NULL;
    int i;
    int flagJumpFor=0;

    hash_ret=hash_swap(pid, v_addr);
    node=hash_table[hash_ret];

    if(node==NULL) {
        return 0;
    }
    switch(store) {
        case SWAP_DISCARD:
            //in this case, delete the realtive node in the hash table and set as free
            //the bit relative to that page in the freespace structure
            //first, look for the page in the swapfile through the hash table
            //two different cases: if the node we're looking for is the first, break asap
            //else, go on and cycle through the others
            
            if(node->pid==pid && node->v_addr==v_addr) {
                hash_table[hash_ret]=node->next;
                flagJumpFor=1;

                freespace[node->char_indx] ^ vett[node->bit_indx];

                kfree(node);

            }

            for(; node->next!=NULL && flagJumpFor!=1; node=node->next) {
                if(node->next->pid==pid && node->next->v_addr==v_addr) {
                    node_temp=node->next;
                    node->next=node->next->next;

                    freespace[node_temp->char_indx] ^ vett[node_temp->bit_indx];
                    
                    kfree(node_temp);

                }
            }
            break;

        case SWAP_LOAD:
            //prima faccio la scrittura in memoria,
            //dopo libero il nodo dall'hash table e il relativo bit da freespace

            for(; node->next!=NULL; node=node->next) {
                if(node->next->pid==pid && node->next->v_addr==v_addr) {
                    break;
                }
            }

            if(swap_read(node->next.swap_offset, node->next.v_addr)!=PAGE_SIZE) {
                panic("Error while reading on the swapfile.\n");
            }
            
            node_temp=node->next;
            node->next=node->next->next;
                    
            //da inserire una funzione che libera il nodo (tipo kfree)

            freespace[node_temp->char_indx] ^ vett[node_temp->bit_indx];

            kfree(node_temp);
            
            break;
    }
    return 0;
}

void swap_out(vaddr_t v_addr, paddr_t p_addr, pid_t pid)
{
    int foundROW=-1;
    int foundCOLUMN=-1;
    int i, j;
    int hash_ret=-1;
    struct hash_entry* node=NULL;
    

    for(i=0; i<FREE_SIZE; i++) {
        for(j=0; j<8; j++) {
            if((freespace[i] & vett[j])==0) {
                foundROW=i;
                foundCOLUMN=j;
                //set at 1 freespace[i] at the j-th bit
                freespace[i] |= vett[j];
                break;
            }
        }
    }

    if(foundROW==-1 && foundCOLUMN==-1) {
        panic("Out of swap space");
    }

    hash_ret=hash_swap(pid, v_addr);
    node=hash_table[hash_ret];

    while(node->next!=NULL) {
        node=node->next;
    }

    node->next=kmalloc(sizeof(struct hash_entry));
    node->next->v_addr=v_addr;
    node->next->pid=pid;
    //da debuggare
    node->next->swap_offset=foundROW*8+foundCOLUMN;
    node->next->char_indx=foundROW;
    node->next->bit_indx=foundCOLUMN;

    if(swap_write(node->next->swap_offset, PADDR_TO_KVADDR(p_addr))!=PAGE_SIZE) {
        panic("Error while writing on the swapfile.\n");
    }

}
