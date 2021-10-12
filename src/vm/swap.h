#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include <list.h>
#include <bitmap.h>
struct block *swap_disk;//swap_disk
struct bitmap *swap_bitmap;//swap영역의 특정 index영역이 사용중인지를 나타내는 비트맵

void swap_init();
void swap_in(size_t used_index, void *kaddr);
size_t swap_out(void *kaddr);

#endif