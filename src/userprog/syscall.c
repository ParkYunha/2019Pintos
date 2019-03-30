#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// (void *)valid_pointer(void *p)
// {
//   if(is_user_vaddr(p))
//     return *p;
//   return -1;
// }

static void
syscall_handler (struct intr_frame *f) 
{
  /*TODO: validate every pointers*/
  printf ("system call handler!\n");
  ASSERT(f!=NULL); 
  ASSERT(f->esp != NULL);
  ASSERT(pagedir_get_page(thread_current()->pagedir, f->esp)!=NULL);
  
  int sys_num  = *(uint32_t *)(f->esp);
  // int valid_fd = (int)vaild_pointer((void *)(f->esp + 4));
  // int vaild_buffer_addr = (int)vaild_pointer((void *)(f->esp + 8));
  // int vaild_buffer = (int)vaild_pointer((void *)*vaild_buffer_addr);
  // int valid_length = (int)vaild_pointer((void *)(f->esp + 12));

  // int valid_fd = (int)(f->esp + 4);
  // int *vaild_buffer_addr = (int)(f->esp + 8);
  // //int vaild_buffer = (int)*vaild_buffer_addr;
  // int *valid_length = (int)(f->esp + 12);

  printf("sys_num : %d\n", sys_num);
  printf("esp: %x\n", f->esp);
  hex_dump(f->esp, f->esp, 100, true);
  switch(*(uint32_t *)(f->esp)){
    case SYS_HALT: //0
      //halt();
      break;
    case SYS_EXIT: //1
      exit((int)*(uint32_t *)(f->esp + 4));
      break;
    case SYS_EXEC: //2
      break;
    case SYS_WAIT: //3
      break;
    case SYS_CREATE: //4
      break;
    case SYS_REMOVE: //5
      break;
    case SYS_OPEN: //6
      break;
    case SYS_FILESIZE: //7
      break;
    case SYS_READ: //8 
      break;
    case SYS_WRITE: //9
      //f->eax = write(valid_fd, (const void)*vaild_buffer_addr, (unsigned)*valid_length);
      f->eax = write((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
      //f->eax = write(1, (void *)*(uint32_t *)(f->esp + 24), (unsigned int)(PHYS_BASE - (f->esp + 28)));
      //f->eax = write((int)*(uint32_t *)(f->esp + 20), (void *)*(uint32_t *)(f->esp + 24), (unsigned)*(uint32_t *)(f->esp + 28));
      break;
    case SYS_SEEK: //10
      break;
    case SYS_TELL: //11
      break; 
    case SYS_CLOSE: //12
      break;
  }
  //thread_exit (); 
}


void exit(int status)
{
  printf("%s: exit(%d)\n",thread_name(), status);
  thread_exit();
}

int write (int fd, const void *buffer, unsigned length)
{
  if(fd == 1)
  {
    putbuf(buffer, length);
    return length;
  }
  return -1;
}
//TODO: 일단 fd==1인 경우만 해둔 거임