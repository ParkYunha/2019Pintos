#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void check_valid_pointer(const void *vaddr)
{
  if(!is_user_vaddr(vaddr))
  {
    printf("%s: exit(%d)\n", thread_name(), -1);
    thread_exit();
  }

}
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//TODO: check valid pointer
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
  //printf ("system call handler!\n");
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


  // printf("sys_num : %d\n", sys_num);
  // printf("f->esp: %x\n", f->esp);
  // hex_dump(f->esp, f->esp, 100, false);

  int fd = *((int *)((f->esp) + 4));
  void *buffer = *((void **)((f->esp) + 8));
  unsigned length = *((unsigned*)((f->esp) + 12));

  switch(sys_num){
    case SYS_HALT: //0
    {
      //halt();
      power_off();
      break;
    }

    case SYS_EXIT: //1
    {
      check_valid_pointer((f->esp) + 4);
      int status = (int)*(uint32_t *)((f->esp) + 4);
      printf("%s: exit(%d)\n", thread_name(), status);
      thread_exit();  //FIXME: parent wait blah blah~
      break;  
    }

    case SYS_EXEC: //2
    {
      check_valid_pointer((f->esp) + 4);
      process_execute(*(char **)((f->esp) + 4)); 
      break;
    }

    case SYS_WAIT: //3   //FIXME:
    {
      check_valid_pointer((f->esp) + 4);
      f->eax = process_wait((tid_t)fd);
      // process_wait(thread_tid());
      break;
    }

    case SYS_CREATE: //4
    {
      check_valid_pointer((f->esp) + 4);
      check_valid_pointer((f->esp) + 8);
      f->eax = filesys_create((const char *)fd, (int32_t)(buffer));
      break;
    }

    case SYS_REMOVE: //5
    {
      check_valid_pointer((f->esp) + 4);
      f->eax = filesys_remove((const char *)fd);
      break;
    }

    case SYS_OPEN: //6
    { 
      check_valid_pointer((f->esp) + 4);
      int i;
      struct file* fp = filesys_open((const char *)fd);
     
      if(fp == NULL)  //file could not opened
      {
        f->eax = -1;
      }
      else
      {
        for(i = 3; i < 128; ++i)
        {
          if(thread_current()->f_d[i] == NULL)
          {
            thread_current()->f_d[i] = fp;
            f->eax = i;
            break;
          }
        }
      }
      f->eax = -1; 
      break;
    }

    case SYS_FILESIZE: //7
    {
      check_valid_pointer((f->esp) + 4);
      f->eax = file_length(thread_current()->f_d[fd]);
      break;
    }

    case SYS_READ: //8 
    {
      // int fd = *((int *)((f->esp) + 4));
      // void *buffer = *((void **)((f->esp) + 8));
      // unsigned length = *((unsigned*)((f->esp) + 12));
      check_valid_pointer((f->esp) + 12);
      int i;
      if (fd == 0)  //keboard input from input_getc()
      {
        for(i = 0; i < length; ++i)
        {
          if(((char *)buffer)[i] == NULL)
            break;
        }
        f->eax = i;
      }
      else
      {
        f->eax = -1;
      }
      break;
    }

    case SYS_WRITE: //9
    {
      // int fd = *((int *)((f->esp) + 4));
      // void *buffer = *((void **)((f->esp) + 8));
      // unsigned length = *((unsigned*)((f->esp) + 12));
      check_valid_pointer((f->esp) + 12);
      if(fd == 1)  //console io
      {
        putbuf(buffer, length);
        f->eax = length;
      }
      else f->eax = -1; //TODO: 일단 fd==1인 경우만 해둔 거임

      // f->eax = write(*((int *)((f->esp) + 4)), *((void **)((f->esp) + 8)), *((unsigned*)((f->esp) + 12)));
      //f->eax = write(valid_fd, (const void)*vaild_buffer_addr, (unsigned)*valid_length);
      //f->eax = write((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
      //f->eax = write(1, (void *)*(uint32_t *)(f->esp + 24), (unsigned int)(PHYS_BASE - (f->esp + 28)));
      //f->eax = write((int)*(uint32_t *)(f->esp + 20), (void *)*(uint32_t *)(f->esp + 24), (unsigned)*(uint32_t *)(f->esp + 28));
      break;
    }

    case SYS_SEEK: //10
    {
      check_valid_pointer((f->esp) + 4);
      check_valid_pointer((f->esp) + 8);
      file_seek(thread_current()->f_d[fd], (unsigned)buffer);
      break;
    }

    case SYS_TELL: //11
    {
      check_valid_pointer((f->esp) + 4);
      file_tell(thread_current()->f_d[fd]);
      break; 
    }

    case SYS_CLOSE: //12
    {
      check_valid_pointer((f->esp) + 4);
      file_close(thread_current()->f_d[fd]);
      break;
    }
  }
  //thread_exit ();  //initial
}