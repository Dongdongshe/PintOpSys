
#include "vm/swap.h"

#include "threads/thread.h"

void swap_init (void) {
    swap = block_get_role (BLOCK_SWAP);
    if (swap == NULL) return;
    swap_bitmap = bitmap_create( block_size(swap) / SECTORS_PER_PAGE );
    if (swap_bitmap == NULL) return;
    bitmap_set_all(swap_bitmap, SWAP_FREE);
    lock_init(&swap_bitmap_lock);
}


void swap_in (size_t used_index, void* frame)
{
    if (swap == NULL || swap_bitmap == NULL)  return;

    lock_acquire(&swap_bitmap_lock);

    if (bitmap_test(swap_bitmap, used_index) == SWAP_FREE)
        PANIC ("SWAPPING INTO FREE BITTY");

    bitmap_flip(swap_bitmap, used_index);

    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++) {
        block_read(swap, used_index * SECTORS_PER_PAGE + i, (uint8_t *) (frame + (i * BLOCK_SECTOR_SIZE)));
    }

    lock_release(&swap_bitmap_lock);
}

size_t swap_out (void *frame)
{
    if (swap == NULL || swap_bitmap == NULL)
        PANIC("SWAP CAN'T BE DONE CUZ SWAP MAP UNAVAILABLE");

    lock_acquire(&swap_bitmap_lock);
    size_t free_index = bitmap_scan_and_flip(swap_bitmap, 0, 1, SWAP_FREE);

    if (free_index == BITMAP_ERROR)
        PANIC("SWAP BITTY FULL");

    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++) {
        block_write(swap, free_index * SECTORS_PER_PAGE + i, (uint8_t *) (frame + (i * BLOCK_SECTOR_SIZE)));
    }
    lock_release(&swap_bitmap_lock);
    return free_index;
}
