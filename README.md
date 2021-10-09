# Pintos_Again
### 더 깊은 이해를 위해 다시 해보는 Pintos
- Description
  - x86 아키텍처를 위한 교육용 운영체제
  - Kernel Threads, Loading and Runing user programs, filesystem 등을 지원
  - Bochs x86 시뮬레이터 사용
  <img width="800" alt="image" src="https://user-images.githubusercontent.com/57051773/133548974-a18b7632-2c37-423d-ac40-240362e5d437.png">
- 참고자료
  - 한양대학교 Pintos_all PPT
  - 서강대학교 Pintos Project 과제 PPT
---------

## Pintos1, 2(User Program)
#### - 구현항목
  -  Command Line Parsing
      - 커맨드 라인의 문자열을 토큰으로 분리
      - 프로그램의 이름과 인자를 구분하여 스택에 저장하고, 인자를 프로그램에 전달
  -  System Call
      - system call handler 및 system call(halt, exit, create, remove)구현
  -  Hierarchical Process Structure
      - struct thread에 부모와 자식 필드를 추가하고, 이를 관리하는 함수들을 구현
      - exec(), wait() system call 구현(using semaphore)
        - 자식 프로세스가 load되기 전, terminate 되기 전에 부모 프로세스가 terminate되지 않도록 함
  -  File Descriptor
      - 파일 디스크립터 및 관련 시스템 콜 구현
      - stack pointer가 가리키는 주소에서 page fault가 발생한 경우에 대한 구현
  -  Denying Write to Executable
      - 메모리에 적재된 프로그램(실행중인 프로그램)의 디스크 상의 파일이 중간에 변경되지 않도록 구현

#### - 추가함수
  -  Command Line Parsing
      ``` 
      //함수 호출 규약에 따라 유저 스택에 프로그램 이름과 인자들을 저장
      void argument_stack(char **parse, int count, void **esp)
      ```
  -  System Call
      ``` 
      //주소값이 유저 영역에서 사용하는 주소값인지 확인하는 함수
      void check_address(void *addr)
      //유저 스택(esp)에 있는 인자들을 arg에 저장하는 함수
      void get_argument(void *esp, int *arg, int count)
      ```
  -  Hierarchical Process Structure
      ``` 
      //자식리스트를 검색하여 pid에 해당하는 프로세스 디스크립터의 주소를 리턴
      struct thread *get_child_process(int pid)
      //프로세스 디스크립터를 자식 리스트에서 제거하고 메모리를 해제
      void remove_child_process(struct thread *cp)
      ```
  -  File Descriptor
      ``` 
      //파일 객체에 대한 파일 디스크립터 생성
      int process_add_file(struct file *f)
      //프로세스의 파일 디스크립터 테이블을 검색하여 파일 객체의 주소를 리턴
      struct file *process_get_file(int fd)
      //파일 디스크립터에 해당하는 파일을 닫고 해당 엔트리 초기화
      void process_close_file(int fd)
      ```
#### - 결과
  - <img width="516" alt="image" src="https://user-images.githubusercontent.com/57051773/133548503-f984223b-6908-498f-b14d-0c7a5bbddaa4.png">
---------

## Pintos3(Threads)
#### - 구현항목
  -  Alarm System Call
      - Alarm: 호출한 프로세스를 정해진 시간 후에 다시 시작하는 커널 내부 함수
      - 기존의 Busy waiting을 이용하여 구현된 Alarm System Call을 Sleep/Wake up 방식을 이용하여 구현
        - Busy waiting: 프로세스가 CPU를 점유하면서 대기하는 상태로, CPU자원이 낭비되게 됨.
  -  Priority Scheduling
      - 기존의 라운드 로빈 스케줄링(Round Robin Scheduling)으로 구성되어있는 pintos를 우선순위 스케줄링(Priority Scheduling)하도록 구현
      - 현재 CPU를 점유하는 thread의 우선순위보다 ready_list에 새로 추가된 thread의 우선순위가 높으면 기존의 thread를 밀어내고 CPU를 점유하는 선점형 스케줄링으로 구현
  -  Priority Scheduling and Synchronization
      - 여러 thread들이 동시에 lock, semaphore, condition variable을 얻기위해 기다리는 경우
      - 우선순위가 가장 높은 thread가 해당 lock, semaphore, condition variable을 점유하도록 구현
      - 해당 lock, semaphore, condition variable를 기다리는 thread들의 list인 waiter_list를 우선순위에 따라 관리
  -  Priority Inversion Problem
      - 우선순위가 높은 thread가 우선순위가 낮은 thread를 기다리는 현상을 해결하도록 Priority Donation, Multiple Donation, Nested Donation 구현
        - 해결: high thread의 priority를 low thread에 donation해줌
 
  - Multi-Level Feedback Queue Scheduler
      - BSD Scheduler와 유사한 Multi-Level Feedback Queue 스케줄러 구현
      - Priority: PRI_MAX - (recent_cpu / 4) - (nice * 2)
        - All Thread: 1초(100ticks)마다 Priority 재계산
        - Current Thread: 4 clock ticks마다 Priority 재계산
      - Nice: -20 ~ 20
      - Recent CPU: (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
        - thread가 최근에 얼마나 많은 CPU time을 사용했는지를 표현
        - timer interrupt마다 1씩 증가, 매 1초마다 재계산
      - Load Average: (59 / 60) * load_avg + (1 / 60) * ready_threads
        - 최근 1분 동안 수행 가능한 프로세스의 평균 개수
        - ready_threads: ready_list에 있는 thread들과 실행중인 thread의 개수
      - [참고사이트] https://poalim.tistory.com/25

#### - 추가함수
  -  Alarm System Call
      ``` 
      /* 실행 중인 thread를 sleep으로 만듬 */
      void thread_sleep(int64_t ticks); 
      /* sleep_list에서 깨워야할 thread를 깨움 */ 
      void thread_awake(int64_t ticks); 
      /*최소 틱을 가진 thread(깨워야할 thread)의 tick을 next_tick_to_awake 변수에  저장 */
      void update_next_tick_to_awake(int64_t ticks); 
       /* thread.c의 next_tick_to_awake 반환 */
      int64_t get_next_tick_to_awake(void);
      ```
  -  Priority Scheduling
      ``` 
      /* 현재 CPU를 점유하는 thread와 ready_list중 가장 높은 우선순위를 가지는 thread의 우선순위를 비교하여 스케줄링 */
      void test_max_priority (void);
      /* 인자로 주어진 thread들의 우선순위를 비교 */
      bool cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
      ```
  -  Priority Scheduling and Synchronization
      ``` 
      /* 해당 condition variable을 기다리는 semaphore들을, 각 semaphore의 waiter_list 중 가장 높은 우선순위를 가지는 thread의 우선순위에 따라 비교 */
      bool cmp_sem_priority (const struct list_elem *a, const struct list_elem *b, void *aux);
      ```
  -  Priority Inversion Problem
      ``` 
      /* 현재 thread의 우선순위를 기다리고 있는 lock과 nested로 연결된 모든 thread들에 기부 */
      void donate_priority(void);
      /* thread가 lock을 해지 했을 때 thread의 donation리스트에서 해당 lock을 기다리는 thread들을 삭제 */
      void remove_with_lock(struct lock *lock);
      /* thread의 우선순위가 변경되었을 때, donation을 고려하여 우선순위를 다시 결정 */
      void refresh_priority(void);
      ```   
  - Multi-Level Feedback Queue Scheduler
      - fixed_point.h 구현(각종 floating point 연산에 관한 함수들)
      - mlfqs 관련 함수들
        ``` 
        /* recent_cpu와 nice값을 이용하여 priority 계산 */
        void mlfqs_priority (struct thread *t); 
        /* recent_cpu값 계산 */
        void mlfqs_recent_cpu (struct thread *t);
        /* load_avg값 계산 */
        void mlfqs_load_avg (void);
        /* 현재 thread의 recent_cpu값 1 증가 */
        void mlfqs_increment (void);
        /* 모든 thread의 recent_cpu와 priority값 재계산*/
        void mlfqs_recalc (void);
        ```   

#### - 결과
  - <img width="280" alt="image" src="https://user-images.githubusercontent.com/57051773/134620593-9fe89110-99ab-4733-8cf9-61b7aa372a39.png">
  - 특이사항
    - thread_get_load_avg()와 thread_get_recent_cpu()의 반환값에 2를 곱해주었을 때 결과가 잘 나옴.

## Pintos4(Virtual Memory)
#### - 구현항목
  -  VM Entry, 가상 주소공간 초기화 및 유효성 검사
      - 물리페이지 할당시에 파일의 Offset에 대한 정보를 읽을 필요가 있으므로 Page Fault가 일어날 때마다, 이와 관련된 정보를 가지고 있는 가상 주소에 해당하는 VM Entry를 탐색하도록 구현
      - 탐색이 빠른 해시로 VM Entry를 관리하고, Virtual Address 값으로 해시 값을 추출
      - 프로세스 생성시 프로세스의 VM Entry들을 관리하는 해시테이블을 만들고, Page Fault 발생시 해시 테이블에서 해당 VM Entry를 탐색, 프로세스 종료시 해시테이블과 VM Entry들 제거
      - 기존의 프로그램 로드시 메모리에 물리 페이지를 할당하고 맵핑하는 부분을 제거하고, VM Entry 구조체를 만들어 해시테이블에 삽입하는 코드를 삽입
      - 기존의 Stack Setup시 해당 부분에 대한 VM Entry 구조체를 만들어 해시 테이블에 삽입하는 코드를 삽입
      - 주소 유효성 검사: 가상주소에 해당하는 VM Entry가 존재하는지 검사
  -  요구 페이징
      - 기존의 Page Fault 발생시 Segmentation Fault를 발생시키고 Kill(-1)하였던 상황에서, Fault Address의 유효성을 검사하고 Page Fault를 적절히 처리해줄 수 있도록 구현 
      - VM Entry를 프로세스의 해시 테이블에서 검색하고 이를 바탕으로, 물리메모리를 할당 후, 실제 디스크의 파일을 물리메모리로 Load하도록 구현
  -  Memory Mapped File
      - 기존의 mmap()과 munmap()함수가 구현되어 있지 않는 핀토스에서, mmap()과 munmap()함수를 구현
      - mmap() : 요구페이징에 의해 파일 데이터를 메모리로 로드하는 함수
      - munmap() : 파일 매핑을 제거하는 함수 
      - Memory Mapped File: disk block을 memory 안의 page에 mapping함으로써 File I/O를 일반적인 Memory Access인 것처럼 다루는 것
  -  Swapping
      - 물리 페이지가 부족할 때, Victim Page를 선정하여 디스크로 Swap Out함으로써 여유 메모리 확보
      - Victim으로 선정된 페이지가 프로세스의 데이터영역 혹은 스택에 포함될 때, 이를 스왑 영역에 저장하도록 구현
      - Swap Out된 페이지가 다시 필요한 경우, 요구 페이징에 의해 스왑영역으로부터 다시 메모리로 로드
      - LRU 기반 페이지 교체 알고리즘 사용(Clock 알고리즘)
        - 페이지 테이블의 Access Bit는 페이지가 참조될 때마다 하드웨어에 의해 1로 설정됨
        - 하드웨어는 Access Bit를 다시 0으로 만들지 않음
        - 현재 lru clock 포인터가 가리키고 있는 페이지의 Access Bit를 검사
          -  0인 경우 해당 페이지를 victim으로 설정
          -  1인 경우 Access Bit를 0으로 설정하고 다음 노드로 포인터를 옮겨 다시 검사
      - Dirty bit가 1인 페이지가 Victim으로 설정된 경우, 변경내용은 항상 디스크에 저장해야 함

#### - 추가함수
  -  VM Entry, 가장 주소공간 초기화 및 유혀성 검사, 요구 페이징
      ``` 
      /* 해시테이블 초기화 */
      void vm_init(struct hash* vm);
      /* 해시테이블 제거 */
      void vm_destroy(struct hash *vm);
      /* 현재 프로세스의 주소공간에서 vaddr에 해당하는 vm_entry를 검색 */
      struct vm_entry* find_vme(void *vaddr);
      /* 해시테이블에 vm_entry 삽입 */
      bool insert_vme(struct hash *vm, struct vm_entry *vme);
      /* 해시 테이블에서 vm_entry삭제 */
      bool delete_vme(struct hash *vm, struct vm_entry *vme);
      /* 해시테이블에 vm_entry 삽입 시 어느 위치에 넣을 지 계산(해시함수) */
      unsigned vm_hash_func(const struct hash_elem *e, void *aux UNUSED);
      /* 입력된 두 hash_elem의 주소 값 비교를 통한 크기 비교 */
      bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
      /* vm_entry의 메모리 제거 */
      void vm_destroy_func(struct hash_elem *e, void *aux UNUSED);
      /* 페이지 폴트 발생시 물리페이지를 할당 */
      bool handle_mm_fault(struct vm_entry *vme);
      /* vme의 <파일,offset>로부터 한 페이지를 kaddr로 읽어들이는 함수 */
      bool load_file(void* kaddr, struct vm_entry *vme);
      /* buffer내 vm_entry가 유효한지 확인하는 함수 */
      void check_valid_buffer(void* buffer, unsigned size, void* esp, bool to_write);
      /* 문자열의 주소값이 유효한지 확인하는 함수 */
      void check_valid_string(const void* str, void* esp);
      ```
  -  Memory Mapped File
      ``` 
      /* 요구페이징에 의해 파일 데이터를 메모리로 로드 */
      int mmap(int fd, void *addr);
      /* mmap_list내에서 mapping에 해당하는 mapid를 갖는 모든 vm_entry을 해제 */
      void munmap(mapid_t mapid);
      /* mmap_file의 vme_list에 연결된 모든 vm_entry들을 제거 */
      void do_munmap(struct mmap_file* mmap_file);
      ```
  - Swapping
      ``` 
      /* LRU list를 초기화 */
      void lru_list_init(void);
      /* LRU list의 끝에 유저 페이지 삽입 */
      void add_page_to_lru_list(struct page* page);
      /* LRU list에 유저 페이지 제거 */
      void del_page_from_lru_list(struct page* page);
      /* 페이지 할당 */
      struct page* alloc_page(enum palloc_flags flags;
      /* 물리 주소 kaddr에 해당하는 page해제 */
      void free_page(void *kaddr);
      /* LRU list 리스트 내 page 해제 */
      void _free_page(struct page* page);
      /* LRU list 리스트의 next 리스트를 반환 */
      static struct list_elem* get_next_lru_clock();
      /* Clock 알고리즘을 이용하여 victim page를 선정하고 방출 */
      void try_to_free(enum palloc_free_page);
      /* swap 영역 초기화 */
      void swap_init(void);
      /* 메모리의 내용을 디스크의 스왑 영역으로 방출 */
      size_t swap_out(void *kaddr);
      /* swap-out된 페이지를 다시 메모리로 적재 */
      void swap_in(size_t used_index, void *kaddr);
      ```

#### - 결과
  - <img width="400" alt="image" src="https://user-images.githubusercontent.com/57051773/136649114-4edcc542-340c-492a-bd1d-151f21287fe4.png">
