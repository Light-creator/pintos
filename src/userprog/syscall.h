#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock file_lock;

void sys_exit(int exit_status);
void syscall_init (void);

#endif /* userprog/syscall.h */
