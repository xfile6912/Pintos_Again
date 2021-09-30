#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);

	void
syscall_init (void) 
{
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	//filesys_lock 초기화
	lock_init(&filesys_lock);
}

struct vm_entry * check_address(void *addr, void *esp)
{
	//포인터가 가리키는 주소가 유저영역의 주소인지 확인
	//잘못된 접근인 경우 프로세스 종료
	if(0x8048000>addr || 0xc0000000<=addr) {
		exit(-1);
	}

	//addr이 vm_entry에 존재하면 vm_entry를 반환하도록 코드 수정
	//find_Vme()사용
	return find_vme(addr);

}

//buffer의 유효성을 검사하는 함수
void check_valid_buffer(void *buffer, unsigned size, void *esp, bool to_write)
{
	void *ptr=pg_round_down(buffer);
	//인자로 받은 buffer로부터 buffer + size까지의 크기가 한 페이지의 크기를 넘길 수 도 있음
	for(;ptr<buffer+size; ptr+=PGSIZE)
	{
		//check_address를 이용해서 주소의 유저영역 여부를 검사함과 동시에 vm_entry 구조체를 얻음
		struct vm_entry * vme = check_address(ptr, esp);
		//해당 주소에 대한 vm_entry의 존재 여부와 vm_entry의 writable 멤버가 true인지 검사
		if(vme == NULL || !(vme->writable))
		{
			exit(-1);
		}
		//위 내용을 buffer부터 buffer+size까지의 주소에 포함되는 vm_entry들에 대해 적용

	}


}
//system call에서 사용할 인자의 문자열의 주소값이 유효한 가상 주소인지 검사하는 함수
void check_valid_string(const void *str, void *esp)
{
	//str에 대한 vm_entry의 존재 여부를 확인
	//check address를 사용
	struct vm_entry *vme = check_address(str, esp);
	if(vme == NULL)
		exit(-1);

	int size=0;//string의 사이즈를 저장하는 변수
	while(((char *)str)[size] != '\0')//string의 사이즈 측정
		size++;
	void *ptr=pg_round_down(str);
	//인자로 받은 str도 str+ size까지의 크기가 한 페이지의 크기를 넘길 수 도 있음
	for(;ptr<str+size; ptr+=PGSIZE)
	{
		//check_address를 이용해서 주소의 유저영역 여부를 검사함과 동시에 vm_entry 구조체를 얻음
		vme = check_address(ptr, esp);
		//해당 주소에 대한 vm_entry의 존재 여부와 검사
		if(vme == NULL)
			exit(-1);
	}

}


void get_argument(void *esp, int *arg, int count)
{
	//유저 스택에 저장된 인저값들을 커널로 저장
	//인자가 저장된 위치가 유저영역인지 확인
	int i=0;
	void *ptr=esp+4;//첫 argument가 저장된 위치
	while(count--)
	{
		check_address(ptr, esp);
		arg[i++]=*(int*)ptr;
		ptr+=4;
	}
}
//terminate the pintos
void halt(void)
{
	shutdown_power_off();
}
//terminate current process
void exit(int status)
{
	struct thread *cur=thread_current();
	//프로세스 디스크립터에 exit status 저장
	cur->exit_status=status;
	printf("%s: exit(%d)\n", cur->name, status);
	thread_exit();
}
//create file
bool create(const char *file, unsigned initial_size)
{
	return filesys_create(file, initial_size);
	//return true if success, else false
}
//remove file
bool remove(const char *file)
{
	return filesys_remove(file);
	//return true if success, else false
}

tid_t exec(const char* cmd_line)
{
	//process_execute함수를 호출하여 자식 프로세스 생성
	tid_t tid=process_execute(cmd_line);
	//생성된 자식프로세스의 프로세스 디스크립터를 검색
	struct thread * child = get_child_process(tid);
	//자식프로세스의 프로그램이 적재될 때까지 대기
	sema_down(&(child->load));
	//프로그램 적재 실패시 -1 리턴
	if(!(child->is_loaded))
		return -1;
	//프로그램 적재 성공시 자식 프로세스의 tid리턴
	return tid;
}

int wait(tid_t tid)
{
		//자식프로세스가 종료될 때까지 대기
		return process_wait(tid);
}

int open(const char *file)
{
	struct thread* cur=thread_current();
	//파일을 open
	struct file* fp=filesys_open(file);
	//해당 파일이 존재하지 않으면 -1 리턴
	if(fp==NULL)
		return -1;
	//해당 파일 객체에 파일 디스크립터 부여
	cur->fd_table[cur->new_fd]=fp;
	//파일 디스크립터 리턴
	return cur->new_fd++;
}
int filesize(int fd)
{
	struct thread* cur=thread_current();
	//파일 디스크립터를 이용하여 파일 객체 검색
	struct file* fp = cur->fd_table[fd];
	//해당 파일이 존재하지 않으면 -1 리턴
	if(fp==NULL)
		return -1;
	//해당 파일의 길이를 리턴
	return file_length(fp);
}

int read(int fd, void *buffer, unsigned size)
{
	struct thread * cur=thread_current();
	int read_length=0;
	//파일에 동시접근이 일어날 수 있으므로 lock 사용
	lock_acquire(&filesys_lock);
	int i;
	//파일 디스크립터가 0일 경우, 키보드에 입력을 버퍼에 저장 후
	//버퍼에 저장한 크기를 리턴
	if(fd==0)
	{
		for(i=0; i<size; i++)
		{
			((char*)buffer)[i]=input_getc();
			read_length++;
			if(((char*)buffer)[i]=='\0')
				break;
		}
	}
	//파일 디스크립터가 0이 아닐 경우 파일의 데이터를 크기만큼 저장후
	//읽은 바이트 수를 리턴
	else {
		//파일 디스크립터를 이용하여 파일 객체 검색
		struct file *fp = cur->fd_table[fd];
		if(fp!=NULL)
			read_length=file_read(fp, buffer, size);
	}
	lock_release(&filesys_lock);
	return read_length;

}
int write(int fd, const void *buffer, unsigned size)
{
	struct thread * cur=thread_current();
	int write_length=0;
	//파일에 동시접근이 일어날 수 있으므로 lock 사용
	lock_acquire(&filesys_lock);
	//파일 디스크립터가 1일 경우 버퍼에 저장된 값을 화면에 출력
	if(fd==1)
	{
		putbuf(buffer, size);
		write_length=size;
	}
	//파일 디스크립터가 1이 아닐 경우 버퍼에 저장된 데이터를 크기만큼 파일에 기록후
	//기록한 바이트 수를 리턴
	else
	{
		//파일 디스크립터를 이용하여 파일 객체 검색
		struct file *fp = cur->fd_table[fd];
		if(fp!=NULL)
			write_length=file_write(fp, buffer, size);
	}
	lock_release(&filesys_lock);
	return write_length;
}

void seek(int fd, unsigned position)
{
	struct thread * cur=thread_current();
	//파일 디스크립터를 이용하여 파일 객체 검색
	struct file *fp = cur->fd_table[fd];
	//해당 열린 파일의 위치를 포지션만큼 이동
	file_seek(fp, position);
}
unsigned tell(int fd)
{
	struct thread * cur=thread_current();
	//파일 디스크립터를 이용하여 파일 객체 검색
	struct file *fp = cur->fd_table[fd];
	//해당 열린 파일의 위치를 반환
	return file_tell(fp);
}
void close(int fd)
{
	struct thread* cur=thread_current();
	//file descriptor에 해당하는 파일을 닫음
	file_close(cur->fd_table[fd]);
	//파일 디스크립터 테이블의 해당 entry를 초기화
	cur->fd_table[fd]=NULL;
}
//요구 페이징에 의해 파일 데이터를 메모리로 로드, 성공 시 mapping id를 리턴, 실패시 에러코드(-1)리턴
int mmap(int fd, void *addr)
{
	int mapid;
	struct mmap_file *m_file;

	//인자들을 체크하여 Valid하지 않을 시 에러 반환
	if(process_get_file(fd)==NULL || !is_user_vaddr(addr) || pg_ofs(addr) !=0 || !addr )
		return -1;

	//addr의 공간에 이미 다른 vm_entry가 있다면 이는 올릴 수 없음
	if(find_vme(addr))
		return -1;

	//file_reopen
	struct file *reopened_file = file_reopen(process_get_file(fd));

	//mapid 할당
	mapid=thread_current()->next_mapid++;

	//mmap_file 생성 및 초기화
	m_file=malloc(sizeof(struct mmap_file));
	if(m_file==NULL)
		return -1;
	m_file->mapid = mapid;
	m_file->file = reopened_file;
	list_push_back(&(thread_current()->mmap_list), &(m_file->elem));  //mmap_file들의 리스트 연결을 위한 구조체
	list_init(&(m_file->vme_list));

	//vm_entry 생성 및 초기화
	int read_bytes = file_length(m_file->file);//읽어야할 바이트의 수
	int ofs=0;
	while (read_bytes > 0)
	{

		/* Calculate how to fill this page.
		   We will read PAGE_READ_BYTES bytes from FILE
		   and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		//vm entry 생성(malloc 사용)
		struct vm_entry *vme = malloc(sizeof(struct vm_entry));
		//vm_entry 멤버들 설정, 가상페이지가 요구될 때 읽어야할 파일의 오프셋과 사이즈, 마지막에 패딩할 제로바이트 등등
		vme->type = VM_FILE;
		vme->vaddr = addr;
		vme->writable = true;
		vme->is_loaded = false;
		vme->file = m_file->file;
		vme->offset = ofs;
		vme->read_bytes =page_read_bytes;
		vme->zero_bytes =page_zero_bytes;
		list_push_back(&(m_file->vme_list), &vme->mmap_elem);
		//insert_vme()함수를 사용해서 생성한 vm_entry를 해시 테이블에 추가
		insert_vme(&thread_current()->vm, vme);

		/* Advance. */
		read_bytes -= page_read_bytes;
		//옵셋에 대한 정보도 담아야하므로 옵셋 정보 갱신 필요
		ofs += page_read_bytes;
		addr += PGSIZE;
	}

	//return map_id
	return mapid;
}
//mmap_list내에서 mapping에 해당하는 mapid를 갖는 모든 vmentry를 해제
void munmap(int mapid)
{

	struct thread * cur= thread_current();


	if(mapid==0)//CLOSE_ALL인 경우
	{
		struct list_elem *e;
		for (e = list_begin(&cur->mmap_list); e != list_end(&cur->mmap_list);) {
			//다음 elem 백업
			struct list_elem *next_e = list_next(e);

			struct mmap_file *m_file = list_entry(e, struct mmap_file, elem);
			do_munmap(m_file);

			//다음 elem 복원
			e = next_e;
		}
	}
	else {
		struct mmap_file *m_file=NULL;
		//mmap_list 순회
		struct list_elem *e;
		for (e = list_begin(&cur->mmap_list); e != list_end(&cur->mmap_list); e = list_next(e)) {
			struct mmap_file *check_mmap_file = list_entry(e, struct mmap_file, elem);
			//mapid에 해당하는 mmap file을 찾은 경우
			if (check_mmap_file->mapid == mapid) {
				m_file = check_mmap_file;
				break;
			}
		}
		//mapid에 해당하는 mmap_file을 못찾은 경우
		if (m_file == NULL)
			return;

		//vm_entry 제거
		//페이지 테이블 entry 제거
		//mmap_file 제거
		//file_close
		do_munmap(m_file);
	}
}



	static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	int arg[5];
	void *esp;
	uint32_t syscall_number;

	//get stack pointer
	esp=f->esp;

	check_address(esp, f->esp);/*인자값 변경*/

	//get syscall number
	syscall_number=*(uint32_t*)esp;
	//printf("\n");
	//hex_dump(esp, esp, 100, true);
	switch(syscall_number)
	{

		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			get_argument(esp, arg, 1);
			exit((int)arg[0]);
			break;
		case SYS_CREATE:
			get_argument(esp, arg, 2);
			check_valid_string((void *)arg[0], f->esp);//stack으로 부터 얻어온 filename의 주소가 올바른 주소인지 check
			f->eax = create((const char*)arg[0], (unsigned)arg[1]);
			break;
		case SYS_REMOVE:
			get_argument(esp, arg, 1);
			check_valid_string((void *)arg[0], f->esp);//stack으로 부터 얻어온 filename의 주소가 올바른 주소인지 check
			f->eax=remove((const char *)arg[0]);
			break;
		case SYS_EXEC:
			get_argument(esp, arg, 1);
			check_valid_string((void *)arg[0], f->esp);
			f->eax=exec((const char *)arg[0]);
			break;
		case SYS_WAIT:
			get_argument(esp, arg, 1);
			f->eax=wait((int)arg[0]);
			break;
		case SYS_READ:
			get_argument(esp, arg, 3);
			//기존의 check_address함수는 삭제
			//check_address((void *)arg[1]);
			//check_valid_buffer함수 구현
			check_valid_buffer((void *)arg[1], (unsigned)arg[2], esp, true);
			f->eax=read((int)arg[0], (void*)arg[1], (unsigned)arg[2]);
			break;
		case SYS_WRITE:
			get_argument(esp, arg, 3);
			check_valid_string((void *)arg[1], f->esp);
			f->eax=write((int)arg[0], (void*)arg[1], (unsigned)arg[2]);
			break;
		case SYS_OPEN:
			get_argument(esp, arg, 1);
			check_valid_string((void *)arg[0], f->esp);//stack으로 부터 얻어온 filename의 주소가 올바른 주소인지 check
			f->eax=open((const char *)arg[0]);
			break;
		case SYS_FILESIZE:
			get_argument(esp, arg, 1);
			f->eax=filesize((int)arg[0]);
			break;
		case SYS_SEEK:
			get_argument(esp, arg, 2);
			seek((int)arg[0], (unsigned)arg[1]);
			break;
		case SYS_TELL:
			get_argument(esp, arg, 1);
			f->eax=tell((int)arg[0]);
			break;
		case SYS_CLOSE:
			get_argument(esp, arg, 1);
			close((int)arg[0]);
			break;
		case SYS_MMAP:
			get_argument(esp, arg, 2);
			f->eax = mmap((int)arg[0], (void*)arg[1]);
			break;
		case SYS_MUNMAP:
			get_argument(esp, arg, 1);
			munmap((int)arg[0]);
			break;
	}
}
