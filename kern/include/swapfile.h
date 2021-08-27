#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include <types.h>

#define SWAP_DISCARD    0
#define SWAP_LOAD       1

#define SWAP_FILESIZE   9*1024*1024

/* swap_bootstrap
    Alloca le strutture dati utili per la gestione dello
    SWAPFILE
 */
void swap_bootstrap(void);

/*  swap_in
    vaddr_t v_addr: indirizzo logico della pagina
                    da caricare in memoria
    pid_t      pid: pid del processo
    uint8_t  store: SWAP_DISCARD per scartarla, SWAP_LOAD per
                    copiarla in memoria
    Prende la pagina dallo swapfile e la carica in memoria,
    se non trova la pagina non fa nulla.
    Ritorna 0 se NON trova la pagina nello SWAPFILE, altrimenti
    ritorna un valore diverso da 0.
*/
int swap_in(vaddr_t v_addr, pid_t pid, uint8_t store);

/*  swap_out
    vaddr_t   v_addr:           indirizzo logico della pagina
                                da inserire nello SWAPFILE
    pid_t        pid:           pid del processo
    paddr_t      p_addr:        physical address della pagina da 
                                inserire nello SWAPFILE
    Prende una pagina dalla memoria e la inserisce nello SWAPFILE
    (no return code)
*/
void swap_out(vaddr_t v_addr, paddr_t p_addr, pid_t pid);

/*  hash_swap
    pid_t        pid: pid del processo
    vaddr_t   v_addr: indirizzo logico della pagina da cercare 
                      nell'hash table
    Ritorna il valore hash associato ai valori in input per
    la ricerca della pagina nello swap file
*/

int hash_swap(pid_t pid, vaddr_t vaddr);

/*  swap_write
    int       offset: è l'offset a cui scriviamo all'interno dello swapfile
    vaddr_t    vaddr: indirizzo logico della pagina da scrivere nello swapfile
    Ritorna il numero di byte scritti sullo swapfile
*/

int swap_write(int offset, vaddr_t vaddr);

#endif /* _SWAPFILE_H_ */
