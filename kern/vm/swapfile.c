#include <swapfile.h>


void swap_bootstrap(void)
{

}

int swap_in(vaddr_t v_addr, pid_t pid, uint8_t store)
{
    (void) v_addr;
    (void) pid;
    (void) store;
    return 0;
}

void swap_out(vaddr_t v_addr, pid_t pid)
{
    (void) v_addr;
    (void) pid;
}