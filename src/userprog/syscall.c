#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

	void
syscall_init (void) 
{
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_address(void *addr)
{
	uint32_t address=(uint32_t)addr;
	if(0x8048000>addr || 0xc0000000<=addr)
		exit(-1);
	if(!is_user_vaddr(addr))
		exit(-1);
	if(pagedir_get_page(thread_current()->pagedir, addr)==NULL)
		exit(-1);
	//포인터가 가리키는 주소가 유저영역의 주소인지 확인
	//잘못된 접근인 경우 프로세스 종료
}

void get_argument(void *esp, int *arg, int count)
{
	//유저 스택에 저장된 인저값들을 커널로 저장
	//인자가 저장된 위치가 유저영역인지 확인
	int i=0;
	esp=esp+4;//첫 argument가 저장된 위
	while(count--)
	{
		check_address(esp);
		arg[i++]=*(int*)esp;
		esp+=4;
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
	struct thread *current_thread=thread_current();
	//프로세스 디스크립터에 exit status 저장
	current_thread->exit_status=status;
	printf("%s: exit(%d)\n", current_thread->name, status);
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

tid_t exec(const *cmd_line)
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

int read(int fd, void *buffer, unsigned size)
{
	int i;
	if(fd==0)
	{
		for(i=0; i<size; i++)
		{
			((char*)buffer)[i]=input_getc();
			if(((char*)buffer)[i]=='\0')break;
		}
		if(i!=size)
			return -1;
		return size;
	}
	return -1;

}
int write(int fd, const void *buffer, unsigned size)
{
	if(fd==1)
	{
		putbuf(buffer, size);
		return size;
	}
	return -1;
}
	static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	int arg[5];
	void *esp;
	uint32_t syscall_number;

	//get stack pointer
	esp=f->esp;
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
			check_address((void *)arg[0]);//stack으로 부터 얻어온 filename의 주소가 올바른 주소인지 check
			f->eax = create((const char*)arg[0], (unsigned)arg[1]);
			break;
		case SYS_REMOVE:
			get_argument(esp, arg, 1);
			check_address((void *)arg[0]);//stack으로 부터 얻어온 filename의 주소가 올바른 주소인지 check
			f->eax=remove((const char *)arg[0]);
			break;
		case SYS_EXEC:
			get_argument(esp, arg, 1);
			check_address((void *)arg[0]);
			f->eax=exec((const char *)arg[0]);
			break;
		case SYS_WAIT:
			get_argument(esp, arg, 1);
			f->eax=wait((int)arg[0]);
			break;
		case SYS_READ:
			get_argument(esp, arg, 3);
			f->eax=read((int)arg[0], (void*)arg[1], (unsigned)arg[2]);
			break;
		case SYS_WRITE:
			get_argument(esp, arg, 3);
			f->eax=write((int)arg[0], (void*)arg[1], (unsigned)arg[2]);
			break;
	}
}
