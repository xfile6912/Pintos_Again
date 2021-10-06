#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include <string.h>

//vm_entry의 vaddr을 인자값으로 hash_int() 함수를 사용하여 해시값 반환
static unsigned vm_hash_func(const struct hash_elem *e, void *aux)
{
	//hash_entry()로 element에 대한 vm_entry 구조체 검색
	struct vm_entry * vme = hash_entry(e, struct vm_entry, elem);
	//hash_int()를 이용해서 vm_entry의 vaddr에 대한 hash값을 구하고 반환
	return hash_int(vme->vaddr);
}
//입력된 두 hash_elem의 vaddr 값 비교
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	//hash_entry()로 각각의 element에 대한 vm_entry 구조체를 얻은 후
	struct vm_entry *vme_a=hash_entry(a, struct vm_entry, elem);
	struct vm_entry *vme_b=hash_entry(b, struct vm_entry, elem);
	//vaddr 비교
	return vme_a->vaddr < vme_b->vaddr;
}

static void vm_destroy_func(struct hash_elem *e, void *aux)
{
	struct vm_entry *vme=hash_entry(e, struct vm_entry, elem);
	free(vme);
}

//hash_init함수를 이용하여 해시 테이블 초기화
void vm_init(struct hash *vm)
{
	//hash_init을 이용하여 해시테이블 초기화
	hash_init (vm, vm_hash_func, vm_less_func, NULL);
}
//hash_insert()함수를 사용하여 vm_Entry를 해시테이블에 삽입
bool insert_vme(struct hash *vm, struct vm_entry *vme)
{
	struct hash_elem* elem = hash_insert (vm, &(vme->elem));
	//hash_insert는 삽입 성공시에 null을 반환함
	if(elem ==NULL)
		return true;
	else
		return false;
}
//hash_delete()함수를 이용하여 vm_entry를 해시테이블에서 제거
bool delete_vme(struct hash *vm, struct vm_entry *vme)
{
	struct hash_elem *elem =hash_delete(vm, &(vme->elem));

	//hash_delete는 해당하는 element가 hash table에 없는경우 null을 반환
	//삭제하기 위해서는 hash_table에 있어야하므로 null이 반환되면 실패한 경우임
	if(elem ==NULL)//삭제에 실패한 경우
		return false;
	else//삭제에 성공한 경우
	{
		free(vme);//vme를 free해줌
		return true;
	}
}

//인자로 받은 vaddr에 해당하는 vm_entry 검색후 반환
struct vm_entry *find_vme(void *vaddr)
{
	struct thread *cur=thread_current();
	struct vm_entry search_entry;

	//pg_round_down()으로 vaddr의 페이지 번호를 얻음
	search_entry.vaddr = pg_round_down(vaddr);

	//hash_find()함수를 이용해서 hash_elem 구조체 얻음
	struct hash_elem* e=hash_find(&(cur->vm), &(search_entry.elem));

	if(e!=NULL)//hash_entry()로 해당 hash_elem의 vm_entry 구조체 리턴
		return hash_entry(e, struct vm_entry, elem);
	else//만약 존재 하지 않는다면 NULL 리턴
		return NULL;

}
//hash_destroy()함수를 사용하여 해시테이블의 버킷 리스트와 vm_entry들을 제거
void vm_destroy(struct hash *vm)
{
	hash_destroy (vm, vm_destroy_func);
}

bool load_file(void *kaddr, struct vm_entry *vme)
{
	//using file_read() + file_seek()
	//오프셋을 vm_entry에 해당하는 오프셋으로 설정
	file_seek(vme->file, vme->offset);
	//file_read로 물리페이지에 read_bytes만큼 데이터를 씀
	if(file_read (vme->file, kaddr, vme->read_bytes) != (int)(vme->read_bytes))
	{
		return false;
	}
	//zero_bytes만큼 남는 부분을 0으로 패딩
	memset (kaddr + vme->read_bytes, 0, vme->zero_bytes);
	//file_read 여부 반환
	return true;
}
