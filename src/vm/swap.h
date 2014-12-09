// Physical drive API
#include "devices/block.h"
// Utilities
#include "threads/vaddr.h"
#include "threads/synch.h"
// Bitmap for mapping the free spaces
#include "lib/kernel/bitmap.h"

#define SWAP_FREE 0
#define SWAP_USED 1

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct block *swap;

struct lock swap_bitmap_lock;
struct bitmap *swap_bitmap;

void swap_init (void);
void swap_in (size_t used_index, void* frame);
size_t swap_out (void *frame);
