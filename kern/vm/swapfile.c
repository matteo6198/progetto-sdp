#include <swapfile.h>
#define HASH_SIZE 101
#define FREE_SIZE 288

struct openfile {
    struct vnode *vn;
    off_t offset;
    unsigned int countref;
}

struct hash_entry {
    pid_t pid;
    vaddr_t v_addr;
    int swap_offset;
    struct hash_entry* next;
}

struct openfile swapfile;

struct hash_entry **hash_table=NULL;

//create a bitmap as a char array to track the free space
char* freespace=NULL;

uint8_t vett[8]={1, 2, 4, 8, 16, 32, 64, 128};

//hash function that returns the row index of the hash table
int hash_swap(vaddr_t v_addr, pid_t pid) 
{
    int res = (int) (v_addr >> 12);
    return (res * pid) % HASH_SIZE;
}

//new file_write function specifically used on the swapfile
int swap_write(int offset, vaddr_t v_addr)
{
    struct iovec iov;
    struct uio u;
    int res;
    
    iov.iov_ubase=v_addr;
    iov.iov_len=PAGE_SIZE;

    u.uio_iov=&iov;
    u.uio_iovcnt=1;
    u.uio_resid=PAGE_SIZE;
    u.uio_offset=offset;
    u.uio_segflg=UIO_SYSSPACE;
    u.uio_rw=UIO_WRITE;
    u.uio_space=NULL;
    
    result=VOP_WRITE(swapfile->vn, &u);
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
    freespace=kcalloc(FREE_SIZE, sizeof(char));

    hash_table=kmalloc(HASH_SIZE*sizeof(struct hash_entry*));

    for(i=0; i<HASH_SIZE; i++) {
        hash_table[i]=kmalloc(sizeof(struct hash_entry));
        hash_table[i].next=NULL;
    }
    //open the swapfile
    result=vfs_open("swapfile.txt", 0, rw, &v);
    if(result) {
        panic("Error opening swapfile.txt\n");
    }
    
    //TODO
    /*of->lock=lock_create("file lock");*/
    
    swapfile.vn=v;
    swapfile.offset=0;
    swapfile.countref=1;

}

int swap_in(vaddr_t v_addr, pid_t pid, uint8_t store)
{
    struct iovec iov;
    struct uio u;
    int result, nwrite=0;
    int hash_ret;
    struct hash_entry* node=NULL;
    int i; 

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
            for(i=0; node->next!=NULL; node=node->next, i++) {
                if(node->next->pid==pid && node->next->v_addr==v_addr) {
                
                }
            }
            break;

        case SWAP_LOAD:
            
            break;
    }
    
}

void swap_out(vaddr_t v_addr, paddr_t p_addr, pid_t pid)
{
    struct iovec iov;
    struct uio u;
    int result, nwrite=0;
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
    node->next.v_addr=v_addr;
    node->next.pid=pid;
    //da debuggare
    node->next.swap_offset=foundROW*8+foundCOLUMN;
    
    if(swap_write(node->next.swap_offset, PADDR_TO_KVADDR(p_addr))!=PAGE_SIZE) {
        panic("Error while writing on the swapfile.\n");
    }

}
