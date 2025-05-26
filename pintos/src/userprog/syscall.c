#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void sys_exit (int status);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Reads a byte from user virtual address UADDR.
   Returns the byte value if successful, -1 if a segfault occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Reads a word (4 bytes) from user space safely. */
static int
get_user_word (const uint32_t *uaddr)
{
  int i;
  if (!is_user_vaddr (uaddr))
    return -1;
  
  /* Check all 4 bytes are valid */
  for (i = 0; i < 4; i++)
    {
      if (get_user ((uint8_t *)uaddr + i) == -1)
        return -1;
    }
  
  return *uaddr;
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t *args = ((uint32_t *) f->esp);
  
  /* Validate the system call number */
  int syscall_num = get_user_word (args);
  if (syscall_num == -1)
    {
      sys_exit (-1);
      return;
    }
  
  switch (syscall_num) 
    {
    case SYS_EXIT:
      {
        int status = get_user_word (args + 1);
        if (status == -1)
          status = -1;  /* Default to -1 on invalid memory access */
        sys_exit (status);
        break;
      }
    
    /* TODO: Implement other system calls for Problem 3 */
    default:
      printf ("system call!\n");
      thread_exit ();
    }
}

static void
sys_exit (int status)
{
  struct thread *cur = thread_current ();
  
#ifdef USERPROG
  /* Set exit status for the thread */
  cur->exit_status = status;
  
  /* Print termination message if this is a user process */
  if (cur->is_user_process)
    printf ("%s: exit(%d)\n", cur->process_name, status);
#endif
  
  thread_exit ();
}