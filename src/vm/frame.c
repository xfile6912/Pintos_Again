#include "frame.h"

//clock알고리즘의 lru list를 이동하는 작업 수행
static struct list_elem* get_next_lru_clock()
{
    struct list_elem *elem;
    //lru list가 비어있는 경우
    if(list_empty(&lru_list))
        return NULL;

    //lru list가 비어있지 않고 lru_clock이 NULL인 경우
    if(lru_clock==NULL)
        return list_begin(&lru_list);

    //현재 lru리스트의 마지막 노드인 경우 NULL값을 반환
    if(list_next(lru_clock)==list_end(&lru_list))
        elem=NULL;
    //LRU list의 다음 위치를 반환
    else
        elem=list_next(lru_clock);

    return elem;
}

//lru와 관련된 자료구조 초기화
void lru_list_init(void)
{
    //lru_list 초기화
    list_init(&lru_list);
    //lru_list_lock 초기화
    lock_init(&lru_list_lock);
    //lru_clock값을 NULL로 설정
    lru_clock=NULL;
}
//lru list의 끝에 유저 페이지 추가
void add_page_to_lru_list(struct page* page)
{
    list_push_back(&lru_list, &(page->lru));
    if(lru_clock==NULL)//lru_clock이 null인 경우 lru_clock을 초기화 해줌
        lru_clock=get_next_lru_clock();
}
//lru list에서 유저 page 제거
void del_page_from_lru_list(struct page *page)
{
    if(lru_clock==&(page->lru))//lru clock이 가리키고 있는 page를 삭제하는 경우
        lru_clock=get_next_lru_clock(); //lru clock을 다음 것으로 바꾸어 줌
    list_remove(&(page->lru));
    if(lru_clock==NULL)//만약 lru clock을 할당한 것이 null인 경우 list의 end인 경우일 수 있으므로 다시한번 get_next_lru_clock을 통해 체크해줌
        lru_clock=get_next_lru_clock();
}
//물리페이지가 부족할 때 clock알고리즘을 이용하여 여유 메모리 확보
void try_to_free_pages(enum palloc_flags flags)
{
    struct page *page;
    struct page *victim;


    if(lru_clock==NULL)//lru_clock이 초기화가 안되어 있는 경우 초기화 시켜줌
       lru_clock=get_next_lru_clock();

    //victim을 고르는 과정
    page = list_entry(lru_clock, struct page, lru);

    while(pagedir_is_accessed(page->thread->pagedir, page->vme->vaddr))//최근에 access 된경우에는 victim이 될 수 없음
    {
        pagedir_set_accessed(page->thread->pagedir, page->vme->vaddr, false);//해당 page의 access 여부를 false로 바꾸어주고
        lru_clock=get_next_lru_clock(); //lru clock을 다음 것으로 바꾸어 줌
        if(lru_clock==NULL)//만약 lru clock을 할당한 것이 null인 경우 list의 end인 경우일 수 있으므로 다시한번 get_next_lru_clock을 통해 체크해줌
            lru_clock=get_next_lru_clock();
        page = list_entry(lru_clock, struct page, lru);
    }
    //victim이 설정됨
    victim=page;
    switch(victim->vme->type)
    {
        case VM_BIN:
            //dirty bit가 1이면
            if(pagedir_is_dirty(victim->thread->pagedir, victim->vme, victim->vme->vaddr))
            {
                //swap partition에 기록
                victim->vme->swap_slot = swap_out(victim->kaddr);
                //요구 페이징을 위해 type을 VM_ANON으로 변경
                victim->vme->type=VM_ANON;
            }
            break;
        case VM_FILE:
            //dirty bit가 1이면
            if(pagedir_is_dirty(victim->thread->pagedir, victim->vme, victim->vme->vaddr))
                //파일에 변경 내용을 저장
                file_write_at(victim->vme->file, victim->vme->vaddr, victim->vme->read_bytes, victim->vme->offset);
            break;
        case VM_ANON:
            //항상 스왑파티션에 기록
            victim->vme->swap_slot = swap_out(victim->kaddr);
            break;
    }
    //메모리에서 내리게 되므로 is_loaded 값을 false로 바꾸어줌
    victim->vme->is_loaded=false;

    //페이지 해제
    _free_page(victim);

}

struct page *alloc_page(enum palloc_flags flags)
{
    lock_acquire(&lru_list_lock);	//공유자원인 lru_list에 접근해야하므로
    //palloc_get_page()를 ㅌ오해 페이지 할당
    uint8_t *kpage = palloc_get_page(flags);
    while (kpage == NULL)
    {
        try_to_free_pages(flags);
        kpage=palloc_get_page(flags);
    }

    //page 구조체를 할당, 초기화
    struct page *page=malloc(sizeof(struct page));
    page->kaddr=kpage;
    page->thread=thread_current();

    //add_page_to_lru_list()를 통해 LRU 리스트에 page 구조체 삽입
    add_page_to_lru_list(page);
    lock_release(&lru_list_lock);
    //page 구조체의 주소를 리턴
    return page;
}
void free_page(void *kaddr)
{
    //물리 주소 kaddr에 해당하는 page 구조체를 lru 리스트에서 검색
    lock_acquire(&lru_list_lock);	//공유자원인 lru_list에 접근해야하므로
    struct list_elem *e;
    struct page *page=NULL;
    for(e=list_begin(&lru_list); e!=list_end(&lru_list); e=list_next(e))
    {
        struct page* candidate_page=list_entry(e, struct page, lru);
        if(candidate_page->kaddr==kaddr) {
            page = candidate_page;
            break;
        }
    }

    //매치하는 항목을 찾으면 _free_page()호출
    if(page!=NULL)
        _free_page(page);

    lock_release(&lru_list_lock);
}
void _free_page(struct page *page)
{
    //lru리스트에서 제거
    del_page_from_lru_list(page);
    //page 구조체에 할당받은 메모리 공간을 해제
    pagedir_clear_page (page->thread->pagedir, pg_round_down(page->vme->vaddr));//페이지 테이블에서 upage에 해당하는 주소의 엔트리를 제거
    palloc_free_page(page->kaddr);
    free(page);
}