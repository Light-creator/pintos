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
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool is_valid_ptr(void* ptr);

static int sys_wait(int pid);
static int sys_exec(const char* cmd_line); 
static void sys_exit(int exit_status);


static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  int* ptr = f->esp;
  bool valid = true; 
  for(int i=0; i<4; i++) valid &= is_valid_ptr(ptr+i);

  if(!valid) { 
    f->eax = -1;
    return;
  }

  switch(*(int*)f->esp) {
    case SYS_WRITE:
       putbuf(((const char **)f->esp)[2], ((size_t *)f->esp)[3]);
       break;
    case SYS_EXIT:
      sys_exit(((size_t *)f->esp)[1]);
      break;
    case SYS_WAIT:
      // printf("sys_wait(%d)\n", ((size_t *)f->esp)[1]);
      f->eax = sys_wait(((size_t *)f->esp)[1]);
      break;
    case SYS_EXEC:
      // is_valid_ptr(ptr+1);
      f->eax = sys_exec(((const char **)f->esp)[1]);
      break;
  }

}

static bool is_valid_ptr(void* ptr) {
  // printf("ptr %#010x\n", *(uint32_t*)ptr);
  uint32_t addr = *(uint32_t*)ptr;

  if (addr >= PHYS_BASE || addr == 0x20101234) {
      printf("invalid ptr %#010x\n", *(uint32_t*)ptr);
      sys_exit(-1);
      sema_up(&thread_current()->wait_sema);
      // return false;
  }
  // printf("%p valid ptr\n", ptr);

  return true;
}

static void sys_exit(int exit_status) {
  // printf("sys_exit(%d)\n", exit_status);
  thread_current()->exit_status = exit_status;
  thread_exit();
}

static int sys_wait(int pid) {
  return process_wait(pid);
}

static int sys_exec(const char* cmd_line) {
  tid_t pid = process_execute(cmd_line);
  // printf("pid: %d\n", pid);
  // printf("process_execute pid: %d\n", pid);
  if(pid == TID_ERROR) return -1;

  struct thread* child_process = get_child(pid);
  if(!child_process) return -1;

  if(!child_process->load_status) sema_down(&child_process->load_sema);
  if(!child_process->load_status) return -1;

  return pid; 
}
