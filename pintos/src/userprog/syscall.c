#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t *esp = f->esp;
  int syscall_num = *esp;

  switch (syscall_num) 
    {
      case SYS_EXIT:
      {
        /* 사용자 프로세스가 넘긴 exit(status)에서 status 읽어오기 */
        int status = *((int *)esp + 1);

        /* thread_current()->name: process_execute()에 넘긴 전체 문자열 */
        char *name = thread_current()->name;
        /* 첫 공백 위치를 찾아 프로그램 이름만 분리 */
        char *sp = strchr(name, ' ');
        size_t len = sp ? (size_t)(sp - name) : strlen(name);

        /* "progname: exit(status)\n" 출력 */
        printf("%.*s: exit(%d)\n", len, name, status);

        /* 프로세스 종료 */
        thread_exit ();
        break;
      }

      /* 그 외 시스템콜은 아직 구현 안 함(특히 halt() 시 메시지 출력 제외) */
      default:
        /* 필요에 따라 다른 분기를 구현하세요. */
        break;
    }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
