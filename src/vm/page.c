
#include <string.h>
#include <stdbool.h>
#include <hash.h>

#include "threads/thread.h"
#include "threads/vaddr.h"

#include "threads/malloc.h"
#include "threads/palloc.h"

#include "threads/interrupt.h"

#include "filesys/file.h"
#include "filesys/filesys.h"

#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

/* supplemental page entry to hash */
static unsigned spage_hash_func (const struct hash_elem *e, void *aux UNUSED) {
    struct spage_entry *spte = hash_entry(e, struct spage_entry, elem);
    return hash_int((int) spte->user_va);
}

/* less function for user virtual addreesses of supplemental page entries */
static bool spage_less_func (const struct hash_elem *a,
			    const struct hash_elem *b,
			    void *aux UNUSED) {
    struct spage_entry *a_ = hash_entry(a, struct spage_entry, elem);
    struct spage_entry *b_ = hash_entry(b, struct spage_entry, elem);

    if (a_->user_va < b_->user_va) {
        return true;
    }
    return false;
}

/* free up resources for supplemental page entry that is loaded */
static void spage_action_func (struct hash_elem *e, void *aux UNUSED) {
    struct spage_entry *spte = hash_entry(e, struct spage_entry, elem);
    if (spte->is_loaded) {
        frame_free(pagedir_get_page(thread_current()->pagedir, spte->user_va));
        pagedir_clear_page(thread_current()->pagedir, spte->user_va);
    }
    free(spte);
}

/* initialize supplemental page table */
void spage_table_init (struct hash *spt) {
    hash_init (spt, spage_hash_func, spage_less_func, NULL);
}

/* retrieve supplemental page entry from the user virtual address
    returns spage_entry pointer if the entry is present in the table,
    NULL if not
*/
struct spage_entry* spage_getEntry (void *user_va) {
    struct spage_entry* spte = malloc(sizeof(struct spage_entry));

    spte->user_va = pg_round_down(user_va); /*should it be round up?*/
    struct hash_elem *e = hash_find(&thread_current()->spt, &spte->elem);
    //printf("\n\t Hash result: %s\n\n", e);
//    printf("\n\nHASH FIND RESULT:\n\n\s", e);
    if (e == NULL){ /* entry not found in the hash table */
        //printf("\n\tHash entry not found\n");
        return NULL;
    }
    return hash_entry (e, struct spage_entry, elem); /* entry found in hash table */
}

/* load the page into the memory */
bool spage_load_page (struct spage_entry *spte) {
    /* should i unpin before return ? */
    spte->is_pinned = true;  /* pin the page */
    if (spte->is_loaded)       /* if the page was loaded before, then return fail */
        return false;

    bool success = false;
    switch (spte->spage_type) { /* determine which page it is */
        case SPAGE_FILE: success = spage_load_file(spte); break;
        case SPAGE_SWAP: success = spage_load_swap(spte); break;
        case SPAGE_MMAP: success = spage_load_file(spte); break;//success = spage_load_mmap(spte); break;
        default: break;
    }
    return success;
}

bool spage_load_file (struct spage_entry *spte) {
    enum palloc_flags flags = PAL_USER;
    if ( spte->read_bytes == 0 ) {
        flags = PAL_USER | PAL_ZERO;    /* zero out the frame if nothing to read */
    }
    /* allocate frame and add it to the frame table */
//    printf("\n\n\tallocating frame\n\n");
    uint8_t *frame = frame_alloc(flags, spte);
//    printf("\n\nFrame is allocated ? %p\n\n", frame);
    if (frame == NULL)  return false;  // no frame available


        //printf("\n\t STEP A\n");
        //lock_acquire(&file_lock); // list fails? not initialized?
//        if (((int32_t)(spte->read_bytes)) != file_read_at(spte->file, frame, spte->read_bytes, spte->offset)) {
//            //lock_release(&file_lock);
//            frame_free(frame);
//            return false;
//        }
        /* Load this page. */
        //file_seek (spte->file, spte->offset);
//        printf("\n\tHASH ELEM: %p", &spte->file);
//        printf("\n\t\tGOT YOUR THIS FILE: %p", spte->file);
//            printf("\n\t\tInsert - File: %p, offset: %d, uva: %p, rbyte: %d, zbyte: %d",
//           spte->file, spte->offset, spte->user_va, spte->read_bytes, spte->zero_bytes);
//        struct file *f = file_reopen(spte->file);
//        if (f == NULL) return false;

        int read = file_read_at (spte->file, frame, spte->read_bytes, spte->offset);
        if ( read != (int) spte->read_bytes){
//            printf("\n\n\treading file wanted: %d, actual: %d", spte->read_bytes, read);
//            printf("\ncontent?: %x", frame);
//            file_close(f);
            frame_free(frame);
            return false;
        }
        memset (frame + spte->read_bytes, 0, spte->zero_bytes);
//        file_close(f);
        //printf("\n\t STEP B\n");
        //file_read_at(spte->file, frame, spte->read_bytes, spte->offset);
        //printf("\n\t STEP C\n");
        //lock_release(&file_lock);
//        memset (kpage + page_read_bytes, 0, page_zero_bytes);
        //memset(spte->read_bytes + frame, 0, spte->zero_bytes);


//    printf("\n\nbefore installing\n\n");
    if (!install_page(spte->user_va, frame, spte->writable)) {
//        printf("\n\t STEP F\n");
        frame_free(frame);
        return false;
    }
//    printf("\n\t STEP D\n");
//    printf("\n\nAFTER installing\n\n");
    spte->is_loaded = true;
    return true;
}

/* add file into the supplemental page table */
bool spage_add_file (struct file *file, int32_t ofs, uint8_t *upage,
			     uint32_t read_bytes, uint32_t zero_bytes,
			     bool writable) {
    struct spage_entry *spte = malloc(sizeof(struct spage_entry));
    if (spte == NULL) /* if memory is not available for the entry structure */
        return false;

    /* Assign the file paramter values */
    spte->file          = file;
    spte->offset        = ofs;
    spte->user_va       = upage;
    spte->read_bytes    = read_bytes;
    spte->zero_bytes    = zero_bytes;
    spte->writable      = writable;
    /* initialize the spage_entry structure */
    spte->spage_type    = SPAGE_FILE;
    spte->is_loaded     = false;
    spte->is_pinned     = false;
    /* insert the spage entry into supplemental page table */
//    printf("\n\n\n\t spage add_file begin \n\n");
    struct hash_elem *e = hash_insert(&thread_current()->spt, &spte->elem);
//    printf("\n\t\tInsert - File: %p, offset: %d, uva: %p, rbyte: %d, zbyte: %d",
//           spte->file, spte->offset, spte->user_va, spte->read_bytes, spte->zero_bytes);
//    printf("\n\tINSERTED HASH ELEM: %p", &spte->file);
//    printf("\n\n\n\t spage add_file end \n\n");
    return e == NULL;   // NULL is success
}

/* load swap page */
bool spage_load_swap (struct spage_entry *spte) {
    uint8_t *frame = frame_alloc (PAL_USER, spte);
    if (frame == NULL)     /* No frame available */
        return false;

    if (!install_page(spte->user_va, frame, spte->writable)) {
        frame_free(frame);
        return false;
    }
    swap_in(spte->swap_index, spte->user_va);
    spte->is_loaded = true;
    return true;
}


/* Add mmap file into the mmap page structure, use mmapfd  */
bool spage_add_mmap(struct file *file, int32_t ofs, uint8_t *upage,
			     uint32_t read_bytes, uint32_t zero_bytes, int mmapfd) {

    struct spage_entry *spte = malloc(sizeof(struct spage_entry));
    if (spte == NULL ) return false;
    /* Assign the file paramter values */
    spte->file          = file;
    spte->offset        = ofs;
    spte->user_va       = upage;
    spte->read_bytes    = read_bytes;
    spte->zero_bytes    = zero_bytes;
    spte->writable      = true;
    /* initialize the spage_entry structure */
    spte->spage_type    = SPAGE_MMAP;
    spte->is_loaded     = false;
    spte->is_pinned     = false;

    struct thread *t = thread_current();
    if (t->mmaptable[mmapfd] != NULL)  {
        free(spte);
        return false;
    }

    if (hash_insert(&t->spt, &spte->elem)) {
        spte->spage_type = SPAGE_HASH_ERROR;
        return false;
    }
    return true;
}

/* given the user virtual addreess, grow the stack, returns true if success */
bool spage_grow_stack (void *user_va) {
    if ( ((size_t) (PHYS_BASE - pg_round_down(user_va))) > MAX_STACK)
        return false;   /* cannot grow any more */

    struct spage_entry *spte = malloc(sizeof(struct spage_entry));
    if (spte == NULL)
        return false;

    spte->is_pinned = true;            // need to be pinned so that stack doesn't get evicted?
    spte->is_loaded = true;
    spte->writable = true;
    spte->user_va = pg_round_down(user_va);
    spte->spage_type = SPAGE_SWAP;          // swap should be right

    uint8_t *frame = frame_alloc (PAL_USER, spte);
    if (frame == NULL) {    /* no frame is available */
        free(spte);
        return false;
    }

    if (!install_page(spte->user_va, frame, spte->writable)) {
        free(spte);
        frame_free(frame);
        return false;
    }

    if (intr_context()) {
        spte->is_pinned = false;
    }

    /* insert the spage entry into supplemental page table */
    struct hash_elem *e = hash_insert(&thread_current()->spt, &spte->elem);
    return e == NULL;   // NULL is success
}

/* destory the whole supplental page table */
void spage_table_destroy (struct hash *spt) {
    hash_destroy (spt, spage_action_func);
}
