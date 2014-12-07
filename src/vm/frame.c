//Utilities
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
//VM
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"

void frame_table_init (void)
{
  list_init(&frame_table);
  lock_init(&frame_table_lock);
}

void* frame_alloc(enum palloc_flags flag, struct spage_entry *spte) {
    void *frame = palloc_get_page (flag);
    if (frame == NULL) {
        /*No more frame available, evict required*/
        return NULL;    /*TODO: implement swap & evict */
    }
    frame_add(frame, spte);
    return frame;
}

bool frame_add(void *frame, struct spage_entry *spte) {
    struct frame_table_entry *fte = malloc(sizeof (struct frame_table_entry));
    fte->frame = frame;
    fte->t = thread_current();
    fte->spte = spte;
    lock_acquire(&frame_table_lock);
    list_push_back(&frame_table, &fte->elem);
    lock_release(&frame_table_lock);
}

void* frame_evict(enum palloc_flags flag){

}
void frame_free(void *frame){
    struct list_elem *e;
    lock_acquire(&frame_table_lock);

    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
        struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
        if (frame == fte->frame) {
            list_remove(e); /*remove the frame table entry from the frame table*/
            free(fte);      /*free the memory space for frame table entry*/
            palloc_free_page(frame);    /*free the page for the frame*/
            break;
        }
    }

    lock_release(&frame_table_lock);
}
