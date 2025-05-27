#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/console.h"

static void syscall_handler (struct intr_frame *);
static void check_user_vaddr (const void *vaddr);
static struct file *get_file (int fd);

/* System call functions */
static void halt (void);
static void exit (int status);
static pid_t exec (const char *cmd_line);
static int wait (pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

static struct lock file_lock;

void
syscall_init (void) 
{
  lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  void *sp = f->esp;
  check_user_vaddr (sp);
  
  int syscall_num = *(int*)sp;
  
  switch (syscall_num)
  {
    case SYS_HALT:
      halt ();
      break;
      
    case SYS_EXIT:
      check_user_vaddr (sp + 4);
      exit (*(int*)(sp + 4));
      break;
      
    case SYS_EXEC:
      check_user_vaddr (sp + 4);
      check_user_vaddr (*(void**)(sp + 4));
      f->eax = exec (*(const char**)(sp + 4));
      break;
      
    case SYS_WAIT:
      check_user_vaddr (sp + 4);
      f->eax = wait (*(pid_t*)(sp + 4));
      break;
      
    case SYS_CREATE:
      check_user_vaddr (sp + 4);
      check_user_vaddr (sp + 8);
      check_user_vaddr (*(void**)(sp + 4));
      f->eax = create (*(const char**)(sp + 4), *(unsigned*)(sp + 8));
      break;
      
    case SYS_REMOVE:
      check_user_vaddr (sp + 4);
      check_user_vaddr (*(void**)(sp + 4));
      f->eax = remove (*(const char**)(sp + 4));
      break;
      
    case SYS_OPEN:
      check_user_vaddr (sp + 4);
      check_user_vaddr (*(void**)(sp + 4));
      f->eax = open (*(const char**)(sp + 4));
      break;
      
    case SYS_FILESIZE:
      check_user_vaddr (sp + 4);
      f->eax = filesize (*(int*)(sp + 4));
      break;
      
    case SYS_READ:
      check_user_vaddr (sp + 4);
      check_user_vaddr (sp + 8);
      check_user_vaddr (sp + 12);
      check_user_vaddr (*(void**)(sp + 8));
      f->eax = read (*(int*)(sp + 4), *(void**)(sp + 8), *(unsigned*)(sp + 12));
      break;
      
    case SYS_WRITE:
      check_user_vaddr (sp + 4);
      check_user_vaddr (sp + 8);
      check_user_vaddr (sp + 12);
      check_user_vaddr (*(void**)(sp + 8));
      f->eax = write (*(int*)(sp + 4), *(const void**)(sp + 8), *(unsigned*)(sp + 12));
      break;
      
    case SYS_SEEK:
      check_user_vaddr (sp + 4);
      check_user_vaddr (sp + 8);
      seek (*(int*)(sp + 4), *(unsigned*)(sp + 8));
      break;
      
    case SYS_TELL:
      check_user_vaddr (sp + 4);
      f->eax = tell (*(int*)(sp + 4));
      break;
      
    case SYS_CLOSE:
      check_user_vaddr (sp + 4);
      close (*(int*)(sp + 4));
      break;
      
    default:
      exit (-1);
      break;
  }
}

static void
check_user_vaddr (const void *vaddr)
{
  if (!is_user_vaddr (vaddr) || vaddr < (void*)0x08048000)
    exit (-1);
}

static struct file *
get_file (int fd)
{
  struct thread *cur = thread_current ();
  if (fd < 0 || fd >= 128)
    return NULL;
  return cur->fd[fd];
}

static void
halt (void)
{
  /* Set special exit code to prevent exit message printing */
  thread_current()->exit_code = -2;
  shutdown_power_off ();
}

static void
exit (int status)
{
  struct thread *cur = thread_current ();
  cur->exit_code = status;
  thread_exit ();
}

static pid_t
exec (const char *cmd_line)
{
  if (cmd_line == NULL)
    exit (-1);
  return process_execute (cmd_line);
}

static int
wait (pid_t pid)
{
  return process_wait (pid);
}

static bool
create (const char *file, unsigned initial_size)
{
  if (file == NULL)
    exit (-1);
  
  lock_acquire (&file_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&file_lock);
  return success;
}

static bool
remove (const char *file)
{
  if (file == NULL)
    exit (-1);
    
  lock_acquire (&file_lock);
  bool success = filesys_remove (file);
  lock_release (&file_lock);
  return success;
}

static int
open (const char *file)
{
  if (file == NULL)
    exit (-1);
    
  lock_acquire (&file_lock);
  struct file *f = filesys_open (file);
  if (f == NULL)
    {
      lock_release (&file_lock);
      return -1;
    }
    
  struct thread *cur = thread_current ();
  int fd;
  for (fd = 3; fd < 128; fd++)
    {
      if (cur->fd[fd] == NULL)
        {
          cur->fd[fd] = f;
          
          /* Deny writes to executable files */
          if (strcmp (cur->name, file) == 0)
            file_deny_write (f);
            
          lock_release (&file_lock);
          return fd;
        }
    }
    
  file_close (f);
  lock_release (&file_lock);
  return -1;
}

static int
filesize (int fd)
{
  struct file *f = get_file (fd);
  if (f == NULL)
    exit (-1);
    
  lock_acquire (&file_lock);
  off_t size = file_length (f);
  lock_release (&file_lock);
  return size;
}

static int
read (int fd, void *buffer, unsigned size)
{
  if (buffer == NULL)
    exit (-1);
    
  if (fd == 0)
    {
      /* Read from stdin */
      unsigned i;
      uint8_t *buf = (uint8_t *) buffer;
      for (i = 0; i < size; i++)
        {
          buf[i] = input_getc ();
          if (buf[i] == '\r')
            buf[i] = '\n';
        }
      return size;
    }
  else
    {
      struct file *f = get_file (fd);
      if (f == NULL)
        exit (-1);
        
      lock_acquire (&file_lock);
      off_t bytes_read = file_read (f, buffer, size);
      lock_release (&file_lock);
      return bytes_read;
    }
}

static int
write (int fd, const void *buffer, unsigned size)
{
  if (buffer == NULL)
    exit (-1);
    
  if (fd == 1)
    {
      /* Write to stdout */
      putbuf (buffer, size);
      return size;
    }
  else
    {
      struct file *f = get_file (fd);
      if (f == NULL)
        exit (-1);
        
      lock_acquire (&file_lock);
      off_t bytes_written = file_write (f, buffer, size);
      lock_release (&file_lock);
      return bytes_written;
    }
}

static void
seek (int fd, unsigned position)
{
  struct file *f = get_file (fd);
  if (f == NULL)
    exit (-1);
    
  lock_acquire (&file_lock);
  file_seek (f, position);
  lock_release (&file_lock);
}

static unsigned
tell (int fd)
{
  struct file *f = get_file (fd);
  if (f == NULL)
    exit (-1);
    
  lock_acquire (&file_lock);
  off_t position = file_tell (f);
  lock_release (&file_lock);
  return position;
}

static void
close (int fd)
{
  struct file *f = get_file (fd);
  if (f == NULL)
    exit (-1);
    
  struct thread *cur = thread_current ();
  cur->fd[fd] = NULL;
  
  lock_acquire (&file_lock);
  file_close (f);
  lock_release (&file_lock);
}
