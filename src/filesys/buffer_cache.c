#include "filesys/buffer_cache.h"
#include <stdlib.h>
#include <string.h>
#define BUFFER_CACHE_ENTRY_NB 64 // buffer cache entry의 개수(32kb)


static uint8_t *p_buffer_cache;//buffer cache 메모리 영역을 가리킴
static struct buffer_head buffer_head[BUFFER_CACHE_ENTRY_NB];//buffer head 배열
static struct buffer_head *clock_hand;//victim entry 선정 시 clock 알고리즘을 위한 변수
static struct lock head_lock;//buffer_head리스트를 clock_hand가 순회하기 때문에, buffer_head에 대한 lock

//buffer cache 영역 할당 및 buffer_head 자료구조 초기화
void bc_init(void)
{
    int i;
    //allocation buffer cache in memory(32kb)
    //p_buffer_cache가 buffer cache 영역 포인팅
    p_buffer_cache = (uint8_t*)malloc(sizeof(uint8_t) * BUFFER_CACHE_ENTRY_NB * BLOCK_SECTOR_SIZE);

    //전역변수 buffer_head 자료구조 초기화
    for(i=0; i<BUFFER_CACHE_ENTRY_NB; i++)
    {
        memset(&buffer_head[i], 0, sizeof(struct buffer_head));
        //i번째 buffer_head의 데이터 포인터가, 자신에 해당되는 buffer cache entry를 가리키도록 함
        buffer_head[i].data = p_buffer_cache + i * BLOCK_SECTOR_SIZE;
        lock_init(&(buffer_head[i].lock));
    }
    //victim의 첫 후보를 buffer_head로 해둠
    clock_hand = buffer_head;

    lock_init(&head_lock);
}

//buffer cache에서 데이터를 읽어 유저 buffer에 저장
bool bc_read(block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
    struct buffer_head *head;
    //sector_idx를 buffer_head에서 검색(bc_lookup함수 이용)
    head = bc_lookup(sector_idx);
    //검색 결과가 없을경우, 디스크 블록을 캐싱할 buffer_entry의 buffer_head를 구함(bc_select_victim함수 이용)
    if(head==NULL)
    {
        head = bc_select_victim();
        if(head==NULL)
            return false;

        lock_acquire(&head->lock);

        //head의 sector번호를 sector_idx로 설정해줌
        head->sector=sector_idx;
        head->used=true;

        //block_read함수를 이용해, 디스크 블록 데이터를 buffer cache로 read
        block_read(fs_device, sector_idx, head->data);

        lock_release(&head->lock);
    }

    lock_acquire(&head->lock);


    //memcpy 함수를 통해, buffer에 디스크 블록 데이터를 복사
    memcpy(buffer+bytes_read, head->data + sector_ofs, chunk_size);

    //buffer_head의 clock bit를 setting
    head->clock_bit=true;

    lock_release(&head->lock);

    return true;
}

//buffer의 데이터를 buffer cache에 기록
bool bc_write(block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunk_size, int sector_ofs)
{
    struct buffer_head *head;
    //sector_idx를 buffer_head에서 검색하여 buffer에 복사(구현)
    head = bc_lookup(sector_idx);
    if(head==NULL)
    {
        //검색 결과가 없을경우, 디스크 블록을 캐싱할 buffer_entry의 buffer_head를 구함(bc_select_victim함수 이용)
        head = bc_select_victim();
        if(head==NULL)
            return false;

        lock_acquire(&head->lock);

        //head의 sector번호를 sector_idx로 설정해줌
        head->sector=sector_idx;
        head->used=true;

        //block_read함수를 이용해, 디스크 블록 데이터를 buffer cache로 read
        block_read(fs_device, sector_idx, head->data);
        lock_release(&head->lock);

    }

    lock_acquire(&head->lock);

    //write 했으므로 dirty = true
    head->dirty=true;
    //접근했으므로 clock_bit = true
    head->clock_bit=true;
    //실제로 써주는 작업
    memcpy(head->data + sector_ofs,buffer+bytes_written, chunk_size);

    lock_release(&head->lock);

    return true;
}

//buffer cache에 캐싱된 데이터를 디스크 블록으로 flush
void bc_term(void)
{
    //bc_flush_all_entries 함수를 호출하여 모든 buffer cache entry를 디스크로 flush
    bc_flush_all_entries();
    //buffer cache 영역 할당 해제
    free(p_buffer_cache);
}

//clock 알고리즘을 통해, buffer cache에서 victim entry 선택
struct buffer_head* bc_select_victim(void)
{
    struct buffer_head * victim = NULL;
    //clock 알고리즘을 사용하여 victim entry를 선택
    //buffer_head 전역변수를 순회하며 clock_bit 변수를 검사

    lock_acquire(&head_lock);
    while(victim==NULL)
    {
        lock_acquire(&clock_hand->lock);
        //현재 사용중이지 않은것을 clock_hand가 가리키거나, clock_bit(reference bit)가 0인경우 victim으로 선정
        if(!clock_hand->used || !clock_hand->clock_bit)
        {
            victim = clock_hand;
        }
        clock_hand->clock_bit=false;
        lock_release(&clock_hand->lock);

        //만약 clock_hand가 현재 buffer_head배열의 끝이면, clock_hand를 buffer_head배열의 처음으로 옮겨줌
        if(clock_hand==buffer_head + (BUFFER_CACHE_ENTRY_NB-1))
            clock_hand = buffer_head;
        else
            clock_hand++;

    }

    lock_acquire(&victim->lock);
    //선택된 victim entry가 dirty일 경우, 디스크로 flush
    if(victim->dirty)
    {
        bc_flush_entry(victim);
    }

    lock_release(&victim->lock);

    lock_release(&head_lock);
    //victim entry를 return
    return victim;
}

//buffer_head를 순회하며 디스크 블록의 캐싱 여부 검사
struct buffer_head* bc_lookup(block_sector_t sector)
{
    int i;
    struct buffer_head *candidate=NULL;
    lock_acquire(&head_lock);
    //buffer_head를 순회하며, 전달받은 sector값과 동일한 sector값을 갖는 buffer cache entry가 있는지 확인
    for(i=0; i<BUFFER_CACHE_ENTRY_NB; i++)
    {
        lock_acquire(&buffer_head[i].lock);
        if(buffer_head[i].sector==sector && buffer_head[i].used)
        {
            candidate = &buffer_head[i];
        }
        lock_release(&buffer_head[i].lock);

    }

    lock_release(&head_lock);
    //성공: 찾은 buffer_head반환, 실패: NULL
    return candidate;
}

//buffer cache 데이터를 디스크로 flush
void bc_flush_entry(struct buffer_head *p_flush_entry)
{
    //block_write을 호출하여, 인자로 전달받은 buffer cache entry의 데이터를 디스크로 flush
    block_write(fs_device, p_flush_entry->sector, p_flush_entry-> data);
    //buffer_head의 dirty값 update
    p_flush_entry->dirty=false;
}

//buffer_head를 순회하며 dirty인 entry의 데이터를 디스크로 flush
void bc_flush_all_entries(void)
{
    int i;
    lock_acquire(&head_lock);
    //전역변수 buffer_head를 순회하며, dirty인 entry는 block_write 함수를 호출하여 디스크로 flush
    for(i=0; i<BUFFER_CACHE_ENTRY_NB; i++)
    {
        lock_acquire(&buffer_head[i].lock);
        if(buffer_head[i].dirty)
        {
            block_write(fs_device, buffer_head[i].sector, buffer_head[i].data);
        }
        //디스크로 flush한 후, buffer_head의 dirty값 update
        buffer_head[i].dirty=false;
        lock_release(&buffer_head[i].lock);
    }
    lock_release(&head_lock);
}