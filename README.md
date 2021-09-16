# Pintos_Again
- Description
  - x86 아키텍처를 위한 교육용 운영체제
  - Kernel Threads, Loading and Runing user programs, filesystem 등을 지원
  - Bochs x86 시뮬레이터 사용
  <img width="800" alt="image" src="https://user-images.githubusercontent.com/57051773/133548974-a18b7632-2c37-423d-ac40-240362e5d437.png">

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

