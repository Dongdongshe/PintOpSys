/**
* Author: Gueho Choi
* Date: November 22, 2014
*/

#include "threads/synch.h"
#include "lib/kernel/list.h"


/**
    The frame table contains one entry for each frame that contains a user page.
    Each entry in the frame table contains a pointer to the page, if any, that currently occupies it,
    and other data
*/

static struct list frame_table;
static struct lock frame_table_lock;    /** Whenever frame table is accessed, lock is acquired */

/**
    On every cache hit, move the entry to the BACK of the list,
    Then the most front one is one to be evicted.
*/
struct frame_table_entry {
    void *frame;
    struct thread *t; /*Thread holindg the frame table entry *user thread (not kernel)* */
    struct spage_entry *spte; /*pointer to page table entry*/
    struct list_elem elem;
};

void frame_table_init(void);
void* frame_alloc(enum palloc_flags flag, struct spage_entry *spte);
bool frame_add(void *frame, struct spage_entry *spte);
void* frame_evict(enum palloc_flags flag);
void frame_free(void *frame);
