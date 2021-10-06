#include "vm/swap.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"

const size_t BLOCK_PER_PAGE=PGSIZE/BLOCK_SECTOR_SIZE;//한 페이지당 블럭수


//swap 영역 초기화
void swap_init()
{
    swap_disk=block_get_role(BLOCK_SWAP);
    if(swap_disk==NULL)
        printf("1\n");
    if(swap_disk)
    {
        swap_bitmap = bitmap_create(block_size(swap_disk)/BLOCK_PER_PAGE);
    }

    lock_init(&swap_lock);
}
//used_index의 swap slot에 저장된 데이터를 논리주소 kaddr로 복사
void swap_in(size_t used_index, void *kaddr)
{
    lock_acquire(&swap_lock);
    if(bitmap_test(swap_bitmap, used_index))//used_index에 해당되는 swap영역이 사용되고 있다면
    {
        int i;
        for(i=0; i<BLOCK_PER_PAGE; i++)
        {
            //(page당 block 개수 * used_index)(used_index전까지의 block 섹터 개수) + i
            block_read(swap_disk, BLOCK_PER_PAGE*used_index+i, BLOCK_SECTOR_SIZE*i+kaddr);
            //block들을 읽어서 물리 페이지에 로드함
        }
        bitmap_reset(swap_bitmap, used_index);
    }
    lock_release(&swap_lock);
}
//kaddr 주소가 가리키는 페이지를 스왑 파티션에 기록
size_t swap_out(void *kaddr)
{
    lock_acquire(&swap_lock);
    //first fit에 따라 가장 처음으로 false를 나타내는 index를 가져옴.
    size_t swap_index = bitmap_scan(swap_bitmap, 0, 1, false);
    if(BITMAP_ERROR != swap_index)
    {
        int i;
        for(i=0; i<BLOCK_PER_PAGE; i++)
        {
            block_write(swap_disk, BLOCK_PER_PAGE*swap_index+i, BLOCK_SECTOR_SIZE*i+kaddr);
        }
        bitmap_set(swap_bitmap, swap_index, true);
    }
    lock_release(&swap_lock);
    return swap_index;
}