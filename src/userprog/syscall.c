#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);
void exit (int status);

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

  int i;

  switch(sys_num){
    case SYS_HALT: //0
    {
      //halt();
      power_off();
      break;
    }

    case SYS_EXIT: //1
    {
      check_valid_pointer((f->esp) + 4); //fd
      int status = (int)*(uint32_t *)((f->esp) + 4);

      exit(status);

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
      check_valid_pointer((f->esp) + 4); //fd
      f->eax = process_wait((tid_t)fd);
      // process_wait(thread_tid());
      break;
    }

    case SYS_CREATE: //4
    {
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      // if(fd == NULL)
      // {
      //   exit(-1);
      // }      
      check_valid_pointer((f->esp) + 4); //fd
      check_valid_pointer((f->esp) + 8); //buffer

      f->eax = filesys_create((const char *)fd, (int32_t)(buffer));
      break;
    }

    case SYS_REMOVE: //5
    {
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      // if(fd == NULL)
      // {
      //   exit(-1);
      // }
      check_valid_pointer((f->esp) + 4); //fd
      f->eax = filesys_remove((const char *)fd);
      break;
    }

    case SYS_OPEN: //6
    { 
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      // if(fd == NULL)
      // {
      //   exit(-1);
      // }
      check_valid_pointer((f->esp) + 4); //fd
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
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      // if(fd == NULL)
      // {
      //   exit(-1);
      // }
      check_valid_pointer((f->esp) + 4); //fd
      f->eax = file_length(thread_current()->f_d[fd]);
      break;
    }

    case SYS_READ: //8 
    {
      // int fd = *((int *)((f->esp) + 4));
      // void *buffer = *((void **)((f->esp) + 8));
      // unsigned length = *((unsigned*)((f->esp) + 12));
      check_valid_pointer((f->esp) + 4); //fd
      check_valid_pointer((f->esp) + 8); //buffer
      check_valid_pointer((f->esp) + 12); //length
      int i;
      if (fd == 0)  //stdin: keyboard input from input_getc()
      {
        for(i = 0; i < length; ++i)
        {
          if(((char *)buffer)[i] == NULL)
          {
            break; //remember i
          }
        }
      }
      else if(fd > 2)  //not stdin
      {
        if(thread_current()->f_d[fd] == NULL)
        {
          exit(-1);
        }
        f->eax = file_read(thread_current()->f_d[fd], buffer, length);
        break; //end read
      }
      f->eax = i;
      break;
    }

    case SYS_WRITE: //9
    {
      check_valid_pointer((f->esp) + 4); //fd
      check_valid_pointer((f->esp) + 8); //buffer
      check_valid_pointer((f->esp) + 12); //length      
      if(fd == 1)  //stdout: onsole io
      {
        putbuf(buffer, length);
        f->eax = length;
        break; //end write
      }
      else if(fd > 2)  //not stdout
      {
        if(thread_current()->f_d[fd] == NULL)
        {
          exit(-1);
        }
        f->eax = file_write(thread_current()->f_d[fd], buffer, length);
        break;  //end write
      }
      f->eax = -1;
      break;
    }

    case SYS_SEEK: //10
    {
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd
      check_valid_pointer((f->esp) + 8); //buffer
      file_seek(thread_current()->f_d[fd], (unsigned)buffer);
      break;
    }

    case SYS_TELL: //11
    {
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd
      file_tell(thread_current()->f_d[fd]);
      break; 
    }

    case SYS_CLOSE: //12
    {
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd
      file_close(thread_current()->f_d[fd]);
      break;
    }
  }
  //thread_exit ();  //initial
}


void exit (int status)
{
  int i;
  thread_current()->exit_status = status;
  for(i = 3; i < 128; ++i)
  {
    if(thread_current()->f_d[i] != NULL)
    {
      file_close(thread_current()->f_d[i]);
    }
  }
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();  //FIXME: parent wait blah blah~
}