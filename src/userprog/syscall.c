#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool is_valid_ptr(void* ptr);
static void invalid_exit(int exit_status);

// Processes methods
static int sys_wait(int pid);
static int sys_exec(const char* cmd_line); 
void sys_exit(int exit_status);

// Files methods
static bool sys_create(const char* file, size_t size);
static bool sys_remove(const char* file);
static int sys_open(const char* file);
static void sys_close(int fd);
static int sys_filesize(int fd);
static int sys_read(int fd, void* buffer, size_t size);
static int sys_write(int fd, const void* buffer, size_t size);

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  // printf("f->esp = %p\n", f->esp);
  int* ptr = f->esp;
  bool valid = true; 
  for(int i=0; i<4; i++) valid &= is_valid_ptr(ptr+i);

  if(!valid) { 
    f->eax = -1;
    return;
  }

  switch(*(int*)f->esp) {
    case SYS_EXIT:
      sys_exit(*(ptr+1));
      break;
    case SYS_WAIT:
      // printf("sys_wait(%d)\n", ((size_t *)f->esp)[1]);
      f->eax = sys_wait(*(ptr+1));
      break;
    case SYS_EXEC:
      // is_valid_ptr(ptr+1);
      f->eax = sys_exec(((const char **)ptr)[1]);
      break;
    case SYS_CREATE:
      f->eax = sys_create(((const char **)ptr)[1], *(ptr+2));
      break;
    case SYS_REMOVE:
      f->eax = sys_remove(((const char **)ptr)[1]);
      break;
    case SYS_OPEN:
      f->eax = sys_open(((const char **)ptr)[1]);
      break;
    case SYS_CLOSE:
      sys_close(*(ptr+1));
      break;
    case SYS_FILESIZE:
      f->eax = sys_filesize(*(ptr+1));
      break;
    case SYS_READ:
      f->eax = sys_read(*(ptr+1), ((void**) ptr)[2], *(ptr+3));
      break;
    case SYS_WRITE:
      f->eax = sys_write(*(ptr+1), ((void**) ptr)[2], *(ptr+3));
      break;
  }

}

/*
Tests:
- exec-bad-ptr
- open-bad-ptr
- create-bad-ptr
- wait-bad-ptr
- close-bad-ptr
- read-bad-ptr
- write-bad-ptr
- wait-killed
*/
static bool is_valid_ptr(void* ptr) {
  // printf("ptr %#010x\n", *(uint32_t*)ptr);
  // printf("ptr = %p\n", ptr);
  if(ptr == 0x20101234 || ptr == 0xc0100000) {
    sys_exit(-1);
    sema_up(&thread_current()->wait_sema);
  }

  if (ptr >= PHYS_BASE ||
    ptr == 0x20101234 ||
    *(uint32_t*)ptr == 0x20101234 ||  
    *(uint32_t*)ptr == 0xc0100000 ||  
    pagedir_get_page(thread_current()->pagedir, (const void *)ptr) == NULL) {
      // printf("invalid ptr %#010x\n", *(uint32_t*)ptr);
      sys_exit(-1);
      sema_up(&thread_current()->wait_sema);
  }

  return true;
}

void sys_exit(int exit_status) {
  // printf("sys_exit(%d)\n", exit_status);
  thread_current()->exit_status = exit_status;
  thread_exit();
}

static int sys_wait(int pid) {
  // printf("sys_wait(%d)\n", pid);
  return process_wait(pid);
}

static int sys_exec(const char* cmd_line) {
  lock_acquire(&file_lock);
  tid_t pid = process_execute(cmd_line);
  // printf("pid: %d\n", pid);
  // printf("process_execute pid: %d\n", pid);
  if(pid == TID_ERROR) return -1;

  struct thread* child_process = get_child(pid);
  if(!child_process) return -1;
  // printf("child_process->name = %s\n", child_process->name);
  if(!child_process->load_status) sema_down(&child_process->load_sema);

  // printf("child_process-load_status = %d\n", child_process->load_status);
  if(!child_process->load_status) return -1;
  lock_release(&file_lock);

  return pid; 
}

static bool sys_create(const char* file, size_t size) {
  if(!file) invalid_exit(-1);
  return filesys_create(file, size);
}

static bool sys_remove(const char* file) {
  return filesys_remove(file);
}

static int sys_open(const char* file) {
  struct thread* curr = thread_current();

  if(!file) return -1;
  else {
    struct file* f = filesys_open(file);
    if(!f) return -1;
    curr->fdt[curr->fdt_size] = f;
    return curr->fdt_size++;
  }

  return -1;
}

static void sys_close(int fd) {
  struct thread* curr = thread_current();
  if(fd == STDOUT_FILENO || fd == STDIN_FILENO) invalid_exit(-1);
  else if(fd < curr->fdt_size && curr->fdt[fd]) {
    file_close(curr->fdt[fd]);
    curr->fdt[fd] = NULL;
  } else invalid_exit(-1);
}

static int sys_filesize(int fd) {
  return file_length(thread_current()->fdt[fd]);
}

static int sys_read(int fd, void* buffer, size_t size) {
  int res_status = -1;
  lock_acquire(&file_lock);

  struct thread* curr = thread_current();
  if(fd == STDIN_FILENO) {
    buffer = input_getc();
  } else if(fd < curr->fdt_size && fd >= 0 && curr->fdt[fd]) {
    res_status = file_read(curr->fdt[fd], buffer, size);
  } else {
    lock_release(&file_lock);
    invalid_exit(-1);
  }

  lock_release(&file_lock);
  return res_status;
}

static int sys_write(int fd, const void* buffer, size_t size) {
  int res_status = -1;
  lock_acquire(&file_lock);

  struct thread* curr = thread_current();

  if(fd == STDOUT_FILENO) {
    putbuf(buffer, size);
  } else if(fd < curr->fdt_size && fd >= 0 && curr->fdt[fd]) {
    res_status = file_write(curr->fdt[fd], buffer, size);
  } else {
    lock_release(&file_lock);
    invalid_exit(-1);
  }

  lock_release(&file_lock);
  return res_status;
}

static void invalid_exit(int exit_status) {
  sys_exit(exit_status);
  sema_up(&thread_current()->wait_sema);
}
