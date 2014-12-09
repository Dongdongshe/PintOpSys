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
    if (flag & PAL_USER == 0) return NULL;
    void *frame = palloc_get_page (flag);
    if (frame == NULL) {
        /*No more frame available, evict */
        int count = 0;
        while(frame == NULL && count < 500) {
            frame = frame_evict(flag);
            count++;
        } // 5 HUNDRED TRIES!!
    }

    if(frame == NULL) {
        PANIC("NO FRAME/SWAP AVAILABLE");
        //return NULL; // won't reach here
    }

    frame_add(frame, spte);
    return frame;
}

bool frame_add(void *frame, struct spage_entry *spte) {
    struct frame_table_entry *fte = malloc(sizeof (struct frame_table_entry));
    fte->frame = frame;
    fte->t = thread_current();
//    printf("\n\n\nFAIL\n\n\n");
    fte->spte = spte;
    lock_acquire(&frame_table_lock);
    list_push_back(&frame_table, &fte->elem);
    lock_release(&frame_table_lock);
}

void* frame_evict(enum palloc_flags flag){
    lock_acquire(&frame_table_lock);
    struct list_elem *e = list_begin(&frame_table);
    for(;;) {
        struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
        if (fte->spte->is_pinned){
            /*the page is pinned, should not evict*/
        }else {
            struct thread *t = fte->t;
            if ( pagedir_is_accessed(t->pagedir, fte->spte->user_va) ) {
                /* the page was accessed, reset */
                pagedir_set_accessed(t->pagedir, fte->spte->user_va, false);
            }else {
                if ( pagedir_is_dirty(t->pagedir, fte->spte->user_va)  || fte->spte->spage_type == SPAGE_SWAP) {
                    /*the page is dirty or swappable */
                    if (fte->spte->spage_type == SPAGE_MMAP) {
                        file_write_at(fte->spte->file, fte->frame, fte->spte->read_bytes, fte->spte->offset);
                    }else {
                        /* set the tye to swap so that it can be swapped out again later*/
                        fte->spte->spage_type = SPAGE_SWAP;
                        fte->spte->swap_index = swap_out(fte->frame);
                    }
                }
                fte->spte->is_loaded = false;
                list_remove(&fte->elem);
                pagedir_clear_page(t->pagedir, fte->spte->user_va);
                palloc_free_page(fte->frame);
                free(fte);
                return palloc_get_page(flag);
            }

        }
        /*advance..*/
        e = list_next(e);
        if (e == list_end(&frame_table)) {
            e = list_begin(&frame_table);
        }
    }
}
void frame_free(void *frame){
    struct list_elem *e;
    lock_acquire(&frame_table_lock);
//    printf("\n\tbest guess\n");
    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
        struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
        if (frame == fte->frame) {
            list_remove(e); /*remove the frame table entry from the frame table*/
            free(fte);      /*free the memory space for frame table entry*/
            palloc_free_page(frame);    /*free the page for the frame*/
            break;
        }
    }
//    printf("\n\tbest guess wrong?\n\n");

    lock_release(&frame_table_lock);
}
