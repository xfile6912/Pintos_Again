#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
	char *fn_copy;
	tid_t tid;
	char *command=(char*)malloc(sizeof(char)*PGSIZE);//file_name은 사실상 command_line
	char *real_file_name;//command에서 실제 파일 name을 가져와 저장
	char *next;
	struct file *file=NULL;
	strlcpy(command, file_name, PGSIZE);
	real_file_name=strtok_r(command, " ", &next);

	/* Make a copy of FILE_NAME.
	   Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);



	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (real_file_name, PRI_DEFAULT, start_process, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);

	free(command);
	return tid;
}

//save argvs in user stack
void argument_stack(char **argv, int argc, void **esp)
{
	int i;
	uint32_t *stack_address[256];//argument들의 stack상의 주소를 담고 있음
	//argument들을 stack에 넣어줌
	for(i=argc-1; i>=0; i--)
	{
		//argument의 길이
		int arg_length=strlen(argv[i])+1;
		*esp=*esp-arg_length;
		strlcpy(*esp, argv[i], arg_length);//stack에 argument 저장
		stack_address[i]=*esp;//argument의 stack상의 주소 저장

		//word alighment
		while(((int)(*esp))%4!=0)
		{
			*esp=*esp-1;
			**(uint8_t**)esp=0;
		}
	}

	//stack에 argument들의 stack상의 주소를 넣어줌
	*esp=*esp-4;
	**(uint32_t**)esp=0;

	for(i=argc-1; i>=0; i--)
	{
		*esp=*esp-4;
		**(uint32_t**)esp=stack_address[i];
	}

	//argv의 주소넣어줌
	*esp=*esp-4;
	**(uint32_t**)esp=(uint32_t*)(*esp+4);

	//argc 넣어줌
	*esp=*esp-4;
	**(uint32_t**)esp=argc;

	//return address(fake address)넣어줌.
	*esp=*esp-4;
	**(uint32_t**)esp=0;
}


/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
	char *file_name = file_name_;
	struct intr_frame if_;
	bool success;
	char *command=(char*)malloc(sizeof(char)*PGSIZE);//file_name은 사실상 전체 커맨드를 담고 있음
	char *real_file_name;
	char *next;
	char *argv[256];//argument들 저장
	int argc=0;//argument의 수 저장
	struct thread *current_thread=thread_current();
	strlcpy(command, file_name,PGSIZE);

	real_file_name=strtok_r(command, " ", &next);
	argv[argc++]=real_file_name;
	while(1)
	{
		if(argv[argc-1]==NULL)
		{
			argc--;//마지막에 들어간 null은 인자가 아니기 때문에 -1을 해주는 것이 필요.
			break;
		}

		argv[argc++]=strtok_r(NULL," ", &next);
	}

	///vm_init()함수를 이용하여 해시테이블을 초기화
	vm_init(&(current_thread->vm));

	/* Initialize interrupt frame and load executable. */
	memset (&if_, 0, sizeof if_);
	if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
	if_.cs = SEL_UCSEG;
	if_.eflags = FLAG_IF | FLAG_MBS;


	success = load (real_file_name, &if_.eip, &if_.esp);

	/* If load failed, quit. */
	free_page (file_name);
	if (!success) {
		current_thread->is_loaded=false;
		sema_up(&(current_thread->load));
		thread_exit();
	}
	current_thread->is_loaded=true;

	//메모리 적재 완료시 부모 프로세스 다시 진행
	sema_up(&(current_thread->load));

	argument_stack(argv, argc, &if_.esp);//userstack에 인자 저장
	//hex_dump(if_.esp, if_.esp, PHYS_BASE-if_.esp, true);

	free(command);
	/* Start the user process by simulating a return from an
	   interrupt, implemented by intr_exit (in
	   threads/intr-stubs.S).  Because intr_exit takes all of its
	   arguments on the stack in the form of a `struct intr_frame',
	   we just point the stack pointer (%esp) to our stack frame
	   and jump to it. */
	asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
	NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED)
{
	//자식 프로세스의 프로세스 디스크립터 검색
	struct thread* child=get_child_process(child_tid);
	int exit_status;
	//예외처리 발생시 -1 리턴
	if(child==NULL)
		return -1;
	//자식프로세스가 종료될 때까지 부모 프로세스 대기(세마포어 이용)
	sema_down(&(child->exit));
	//자식프로세스 디스크립터 삭제
	exit_status=child->exit_status;
	remove_child_process(child);
	//자식프로세스의 exit status 리턴
	return exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
	struct thread *cur = thread_current ();
	uint32_t *pd;

	//실행중인 파일 close
	file_close(cur->run_file);

	//mmap_list내에서 mapping에 해당하는 mapid를 갖는 모든 vm_entry를 해제하도록 코드 수정(CLOSE_ALL)
	struct list_elem *e;
	for (e = list_begin(&cur->mmap_list); e != list_end(&cur->mmap_list);) {
		//다음 elem 백업
		struct list_elem *next_e = list_next(e);

		struct mmap_file *m_file = list_entry(e, struct mmap_file, elem);
		do_munmap(m_file);

		//다음 elem 복원
		e = next_e;
	}

	//프로세스에 열린 모든 파일을 닫음
	//파일 디스크립터 테이블의 최대값을 이용해 파일 디스크립터의 최소값인 2가 될때까지 파일을 닫음
	//파일 디스크립터 테이블 메모리 해제(정적 배열로 선언해주었으므로 따로 메모리 해제 필요 없음)
	int fd;
	for(fd=cur->new_fd-1; fd>=2 ; fd--)
	{
		if(cur->fd_table[fd]!=NULL)
			process_close_file(fd);
	}
	cur->new_fd=2;


	//vm_entry들을 제거하는 코드 추가
	vm_destroy(&cur->vm);

	/* Destroy the current process's page directory and switch back
	   to the kernel-only page directory. */
	pd = cur->pagedir;
	if (pd != NULL)
	{
		/* Correct ordering here is crucial.  We must set
		   cur->pagedir to NULL before switching page directories,
		   so that a timer interrupt can't switch back to the
		   process page directory.  We must activate the base page
		   directory before destroying the process's page
		   directory, or our active page directory will be one
		   that's been freed (and cleared). */
		cur->pagedir = NULL;
		pagedir_activate (NULL);
		pagedir_destroy (pd);
	}
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
	struct thread *t = thread_current ();

	/* Activate thread's page tables. */
	pagedir_activate (t->pagedir);

	/* Set thread's kernel stack for use in processing
	   interrupts. */
	tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
	unsigned char e_ident[16];
	Elf32_Half    e_type;
	Elf32_Half    e_machine;
	Elf32_Word    e_version;
	Elf32_Addr    e_entry;
	Elf32_Off     e_phoff;
	Elf32_Off     e_shoff;
	Elf32_Word    e_flags;
	Elf32_Half    e_ehsize;
	Elf32_Half    e_phentsize;
	Elf32_Half    e_phnum;
	Elf32_Half    e_shentsize;
	Elf32_Half    e_shnum;
	Elf32_Half    e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
	Elf32_Word p_type;
	Elf32_Off  p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
						  uint32_t read_bytes, uint32_t zero_bytes,
						  bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
	struct thread *t = thread_current ();
	struct Elf32_Ehdr ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pagedir = pagedir_create ();
	if (t->pagedir == NULL)
		goto done;
	process_activate ();

	//락 획득
	lock_acquire(&filesys_lock);

	/* Open executable file. */
	file = filesys_open (file_name);
	if (file == NULL)
	{
		lock_release(&filesys_lock);
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}
	//thread 구조체의 run file을 현재 실행할 파일로 초기화
	t->run_file=filesys_open(file_name);
	//file_deny_write을 이용하여 파일에 대한 write을 거부
	file_deny_write(t->run_file);
	//락 해제
	lock_release(&filesys_lock);


	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
		|| memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
		|| ehdr.e_type != 2
		|| ehdr.e_machine != 3
		|| ehdr.e_version != 1
		|| ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
		|| ehdr.e_phnum > 1024)
	{
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++)
	{
		struct Elf32_Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;

		file_ofs += sizeof phdr;
		switch (phdr.p_type)
		{
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file))
				{
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint32_t file_page = phdr.p_offset & ~PGMASK;
					uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint32_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0)
					{
						/* Normal segment.
						   Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
									  - read_bytes);
					}
					else
					{
						/* Entirely zero.
						   Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
									   read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}


	/* Set up stack. */
	if (!setup_stack (esp))
		goto done;




	/* Start address. */
	*eip = (void (*) (void)) ehdr.e_entry;

	success = true;

	done:
	/* We arrive here whether the load is successful or not. */
	return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (Elf32_Off) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

   - READ_BYTES bytes at UPAGE must be read from FILE
   starting at offset OFS.

   - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
			  uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Calculate how to fill this page.
		   We will read PAGE_READ_BYTES bytes from FILE
		   and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* 물리 페이지를 할당하고 맵핑하는 부분 삭제 */
//		/* Get a page of memory. */
//		uint8_t *kpage = palloc_get_page (PAL_USER);
//		if (kpage == NULL)
//			return false;
//
//		/* Load this page. */
//		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
//		{
//			palloc_free_page (kpage);
//			return false;
//		}
//		memset (kpage + page_read_bytes, 0, page_zero_bytes);
//
//		/* Add the page to the process's address space. */
//		if (!install_page (upage, kpage, writable))
//		{
//			palloc_free_page (kpage);
//			return false;
//		}

		//vm entry 생성(malloc 사용)
		struct vm_entry *vme = malloc(sizeof(struct vm_entry));
		//vm_entry 멤버들 설정, 가상페이지가 요구될 때 읽어야할 파일의 오프셋과 사이즈, 마지막에 패딩할 제로바이트 등등
		vme->type = VM_BIN;
		vme->vaddr = upage;
		vme->writable = writable;
		vme->is_loaded = false;
		vme->file = file;
		vme->offset = ofs;
		vme->read_bytes =page_read_bytes;
		vme->zero_bytes =page_zero_bytes;

		//insert_vme()함수를 사용해서 생성한 vm_entry를 해시 테이블에 추가
		insert_vme(&thread_current()->vm, vme);

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		//옵셋에 대한 정보도 담아야하므로 옵셋 정보 갱신 필요
		ofs += page_read_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
	struct page *kpage;
	void *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
	bool success = false;

	kpage = alloc_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL)
	{
		success = install_page (upage, kpage->kaddr, true);
		if (success)
			*esp = PHYS_BASE;
		else
			free_page (kpage);
	}
	if(success) {
		//vm_entry 생성
		struct vm_entry *vme = malloc(sizeof(struct vm_entry));
		//vm_entry 멤버들 설정
		vme->type = VM_ANON;
		vme->vaddr = upage;
		vme->writable = true;
		vme->is_loaded = true;

		kpage->vme=vme;
		//insert_vme()함수로 해시테이블에 추가
		insert_vme(&(thread_current()->vm), vme);
	}
	return true;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	   address, then map our page there. */
	return (pagedir_get_page (t->pagedir, upage) == NULL
			&& pagedir_set_page (t->pagedir, upage, kpage, writable));
}
//파일 객체를 File Descriptor 테이블에 추가
int process_add_file(struct file *f)
{
	struct thread *t= thread_current();
	//파일 객체를 파일 디스크립터 테이블에 추가

	t->fd_table[t->new_fd]=f;
	//파일 디스크립터 반환 및 파일 디스크립터의 최댓값 1 증가
	return t->new_fd++;
}
//파일 디스크립터 값에 해당하는 파일 객체 반환
struct file *process_get_file(int fd)
{
	struct thread *t=thread_current();
	//파일 디스크립터에 해당하는 파일 객체를 리턴
	//없을 시 NULL 리턴(처음 thread 초기화 및 파일 삭제시 NULL로 설정하도록 할 것이므로 그냥 반환하면 됨)
	return t->fd_table[fd];
}
//파일 디스크립터에 해당하는 파일을 닫음
void process_close_file(int fd)
{
	struct thread* t=thread_current();
	//file descriptor에 해당하는 파일을 닫음
	file_close(t->fd_table[fd]);
	//파일 디스크립터 테이블의 해당 entry를 초기화
	t->fd_table[fd]=NULL;
}
//addr 주소를 포함하도록 스택을 확장
bool expand_stack(void *addr)
{
	//alloc_page()를 통해 메모리 할당
	struct page *kpage = alloc_page(PAL_USER | PAL_ZERO);
	//vm_entry 의 할당 및 초기ㅗ하
	struct vm_entry *vme = malloc(sizeof(struct vm_entry));
	if(vme==NULL)
		return false;

	vme->type=VM_ANON;
	vme->vaddr=pg_round_down(addr);
	vme->writable=true;
	vme->is_loaded=true;
	insert_vme(&thread_current()->vm, vme);
	kpage->vme=vme;
	//install_page()호출하여 페이지 테이블 설정
	if(!install_page(vme->vaddr, kpage->kaddr, vme->writable))
	{
		_free_page(kpage);
		free(vme);
		return false;
	}

	//성공시 true를 리턴
	return true;
}
bool verify_stack(void*fault_addr, void *esp)
{
	void *maximum_limit = PHYS_BASE-8*1024*1024;
	//esp 주소가 포함되어 있는지 확인하는 함수
	return is_user_vaddr(pg_round_down(fault_addr)) && fault_addr>=esp - 32 && fault_addr >= maximum_limit;
}

bool handle_mm_fault(struct vm_entry *vme)
{

	//palloc_get_page()를 이용해서 물리 메모리 할당
	struct page *kpage = alloc_page (PAL_USER);
	if (kpage == NULL)
		return false;
	kpage->vme=vme;
	//switch문으로 vm_entry의 타입별 처리
	switch(vme->type)
	{
		case VM_BIN:
			//VM_BIN일 경우 load_file 함수를 이용해서 물리 메모리에 로드
			if(!load_file(kpage->kaddr, vme))
			{
				free_page(kpage);
				return false;
			}
			break;
		case VM_FILE:
			//vm_FILE시 데이터를 로드할 수 있도록 수정
			if(!load_file(kpage->kaddr, vme))
			{
				free_page(kpage);
				return false;
			}
			break;
		case VM_ANON:
			//swap_in하는 코드 삽입
			swap_in(vme->swap_slot, kpage->kaddr);
			break;
	}

	//install_page를 이용해서 물리페이지와 가상 페이지 맵핑
	if (!install_page (vme->vaddr, kpage->kaddr, vme->writable))
	{
		free_page (kpage);
		return false;
	}
	//로드에 성공하였으면 vme->is_loaded를 true로 바꾸어줌
	vme->is_loaded=true;

	//로드 성공여부 반환
	return true;

}
//mmap_file의 vme_list에 연결된 모든 vm_entry들을 제거
void do_munmap(struct mmap_file* mmap_file)
{
	//mmap_file의 vme_list에 연결된 모든 vm_entry들을 제거
	struct list_elem *e;
	struct file *file;
	for(e=list_begin(&mmap_file->vme_list);e!=list_end(&mmap_file->vme_list); )
	{

		//다음 주소 백업해둠
		struct list_elem *next_e=list_next(e);

		struct vm_entry *vme=list_entry(e, struct vm_entry, mmap_elem);
		file=vme->file;
		//vm_entry가 가리키는 가상 주소에 대한 물리 페이지가 존재하고 dirty하면 디스크에 메모리 내용을 기록
		if(vme->is_loaded && pagedir_is_dirty(thread_current()->pagedir, vme->vaddr)) {
			lock_acquire(&filesys_lock);
			file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
			lock_release(&filesys_lock);
		}
		vme->is_loaded = false;
		//vme_list에서 vme 제거
		list_remove(e);
		//free page
		free_page(pagedir_get_page(thread_current()->pagedir,vme->vaddr));

		//페이지 테이블 entry 제거
		delete_vme(&thread_current()->vm, vme);
		//다음 주소 복원
		e=next_e;
	}


	//mmap_file 제거
	list_remove(&mmap_file->elem);
	free(mmap_file);
}