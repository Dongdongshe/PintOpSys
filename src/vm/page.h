#include "lib/kernel/hash.h"

#define MAX_STACK (1 << 23)

#define SPAGE_FILE 0
#define SPAGE_SWAP 1
#define SPAGE_MMAP 2
#define SPAGE_HASH_ERROR 3

struct spage_entry {
    void *user_va;          /* user virtual address */
    int spage_type;         /* page type to identify whether it is file, swap, or mmap */

    bool is_pinned;         /* cannot be evicted if pinned */
    bool writable;          /* is readonly or not */
    bool is_loaded;         /* is the page loaded or not, remove if loaded */

    /** Files */
    struct file *file;
    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    /** Swap */
    size_t swap_index;

    struct hash_elem elem;
};

void spage_table_init (struct hash *spt);
void spage_table_destroy (struct hash *spt);

struct spage_entry* spage_getEntry (void *user_va);
bool spage_grow_stack (void *user_va);

bool spage_load_page (struct spage_entry *spte);
bool spage_load_mmap (struct spage_entry *spte);
bool spage_load_swap (struct spage_entry *spte);
bool spage_load_file (struct spage_entry *spte);

bool spage_add_file (struct file *file, int32_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes,
                        bool writable);
bool spage_add_mmap (struct file *file, int32_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes);


