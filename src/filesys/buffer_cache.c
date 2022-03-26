#include "filesys/buffer_cache.h"

#define BUFFER_CACHE_ENTRY_NB 64 // buffer cache entry의 개수(32kb)


static uint8_t *p_buffer_cache;//buffer cache 메모리 영역을 가리킴
static struct buffer_head buffer_head[BUFFER_CACHE_ENTRY_NB];//buffer head 배열
static struct buffer_head *clock_hand;//victim entry 선정 시 clock 알고리즘을 위한 변수

//buffer cache 영역 할당 및 buffer_head 자료구조 초기화
void bc_init(void)
{
    //allocation buffer cache in memory
    //p_buffer_cache가 buffer cache 영역 포인팅
    //전역변수 buffer_head 자료구조 초기화
}

//buffer cache에서 데이터를 읽어 유저 buffer에 저장
bool bc_read(block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
    //sector_idx를 buffer_head에서 검색(bc_lookup함수 이용)
    //검색 결과가 없을경우, 디스크 블록을 캐싱할 buffer_entry의 buffer_head를 구함(bc_select_victim함수 이용)
    //block_read함수를 이용해, 디스크 블록 데이터를 buffer cache로 read
    //memcpy 함수를 통해, buffer에 디스크 블록 데이터를 복사
    //buffer_head의 clock bit를 setting
}

//buffer의 데이터를 buffer cache에 기록
bool bc_write(block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunck_size, int sector_ofs)
{
    bool success = false;

    //sector_idx를 buffer_head에서 검색하여 buffer에 복사(구현)
    //update buffer head(구현)

    return success;
}

//buffer cache에 캐싱된 데이터를 디스크 블록으로 flush
void bc_term(void)
{
    //bc_flush_all_entries 함수를 호출하여 모든 buffer cache entry를 디스크로 flush
    //buffer cache 영역 할당 해제
}

//clock 알고리즘을 통해, buffer cache에서 victim entry 선택
struct buffer_head* bc_select_victim(void)
{
    //clock 알고리즘을 사용하여 victim entry를 선택
    //buffer_head 전역변수를 순회하며 clock_bit 변수를 검사
    //선택된 victim entry가 dirty일 경우, 디스크로 flush
    //victim entry에 해당하는 buffer_head값 update
    //victim entry를 return
}

//buffer_head를 순회하며 디스크 블록의 캐싱 여부 검사
struct buffer_head* bc_lookup(block_sector_t sector)
{
    //buffer_head를 순회하며, 전달받은 sector값과 동일한 sector값을 갖는 buffer cache entry가 있는지 확인
    //성공: 찾은 buffer_head반환, 실패: NULL
}

//buffer cache 데이터를 디스크로 flush
void bc_flush_entry(struct buffer_head *p_flush_entry)
{
    //block_write을 호출하여, 인자로 전달받은 buffer cache entry의 데이터를 디스크로 flush
    //buffer_head의 dirty값 update
}

//buffer_head를 순회하며 dirty인 entry의 데이터를 디스크로 flush
void bc_flush_all_entries(void)
{
    //전역변수 buffer_head를 순회하며, dirty인 entry는 block_write 함수를 호출하여 디스크로 flush
    //디스크로 flush한 후, buffer_head의 dirty값 update
}