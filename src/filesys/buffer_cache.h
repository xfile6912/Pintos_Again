#ifndef VM_BUFFER_CACHE_H
#define VM_BUFFER_CACHE_H


#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

struct buffer_head{
    bool dirty; //해당 entry가 dirty인지를 나타내는 flag
    bool used; //해당 entry의 사용 여부를 나타내는 flag
    block_sector_t sector; //해당 entry의 disk sector주소
    bool clock_bit; //clock알고리즘을 위한 clock bit
    struct lock lock; //lock 변수
    void *data; //buffer cache entry를 가리키기 위한 데이터 포인터

};

void bc_init(void);
bool bc_read(block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs);
bool bc_write(block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunck_size, int sector_ofs);
void bc_term(void);
struct buffer_head* bc_select_victim(void);
struct buffer_head* bc_lookup(block_sector_t sector);
void bc_flush_entry(struct buffer_head *p_flush_entry);
void bc_flush_all_entries(void);


#endif