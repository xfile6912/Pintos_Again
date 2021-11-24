#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/vaddr.h"

#define VM_BIN 0    /*바이너리 파일로부터 데이터를 로드*/
#define VM_FILE 1   /*매핑된 파일로부터 데이터를 로드*/
#define VM_ANON 2   /*스왑 영역으로부터 데이터를 로드*/

struct vm_entry{
    uint8_t type; /*VM_BIN, VM_FILE, VM_ANON의 타입*/
    void *vaddr;  /*vm_entry가 관리하는 가상 페이지 번호*/
    bool writable;    /*True일 경우 해당 주소에 write 가능*/
    bool pinned;
    /*False일 경우 해당 주소에 write 불가능 */
    bool is_loaded;   /*물리메모리의 탑재 여부를 알려주는 플래그*/
    struct file* file;    /*가상 주소와 mapping된 파일*/

    /*memory mapped file에서 다룰 예정*/
    struct list_elem mmap_elem;   /*mmap 리스트 element*/

    size_t offset;    /*읽어야할 파일 오프셋*/
    size_t read_bytes;    /*가상페이지에 쓰여져 있는 데이터 크기*/
    size_t zero_bytes;    /*0으로 채울 남은 페이지의 바이트*/

    /*swapping에서 다룰 예정*/
    size_t swap_slot;     /*swap slot */

    /*vm_entry들을 위한 자료구조 부분에서 다룰 예정 */
    struct hash_elem elem;    /*hash table element*/
};

struct mmap_file{
    int mapid;  //mmap()성공시 리턴된 mapping id //mapid 0은 CLOSE_ALL 의미
    struct file * file; //맵핑하는 파일의 파일 오브젝트
    struct list_elem elem;  //mmap_file들의 리스트 연결을 위한 구조체
    struct list vme_list;   //mmap_file에 해당하는 모든 vm_entry들의 리스트
};

struct page{
    void *kaddr;    //페이지의 물리주소
    struct vm_entry *vme;   //물리 페이지가 매핑된 가상의 주소 vm_entry 포인터
    struct thread *thread;  //해당 물리 페이지를 사용중인 스레드의 포인터
    struct list_elem lru;   //list연결을 위한 필드
};

void vm_init(struct hash *vm);
bool insert_vme(struct hash *vm, struct vm_entry *vme);
bool delete_vme(struct hash *vm, struct vm_entry *vme);

struct vm_entry *find_vme(void *vaddr);
void vm_destroy(struct hash *vm);

bool load_file(void *kaddr, struct vm_entry *vme);

#endif