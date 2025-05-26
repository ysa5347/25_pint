#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num = *(int*) f->esp;
  switch (syscall_num)
  {
  case SYS_HALT: {
    /* halt 시스템 콜인 경우 종료 메시지를 출력하지 않고 바로 종료 */
    shutdown_power_off();
    break;
    }

  case SYS_EXIT: {
    int status = *((int*)f->esp + 1);
    struct thread *cur = thread_current ();
    cur->exit_status = status;
    thread_exit (); 
    break;
    }
    
  default: {
    printf ("system call!\n");
    thread_exit ();
    break;
    }
  }
}
