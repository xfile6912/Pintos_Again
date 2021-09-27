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

## Pintos1(User Program)
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

## Pintos2(Threads)
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
