#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);
void exit (int status);

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
/* Check null, unmapped */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

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

static void
syscall_handler (struct intr_frame *f) 
{
  // ASSERT(f!= NULL); 
  // ASSERT(f->esp != NULL);
  // ASSERT(pagedir_get_page(thread_current()->pagedir, f->esp) != NULL);
  
  //sc-bad-sp
  check_valid_pointer(f->esp);
  if(get_user((uint8_t *)f->esp) == -1)
  {
    exit(-1);
  }

  int sys_num  = *(uint32_t *)(f->esp);

  int first = *((int *)((f->esp) + 4));  //fd or file or pid
  void *second = *((void **)((f->esp) + 8));
  unsigned third = *((unsigned*)((f->esp) + 12));

  int i;

  switch(sys_num){
    //syscall0 (SYS_HALT);
    case SYS_HALT: //0
    {
      //halt();
      power_off();
      break;
    }

    //syscall1 (SYS_EXIT, status);
    case SYS_EXIT: //1
    {
      check_valid_pointer((f->esp) + 4); //status
      int status = (int)*(uint32_t *)((f->esp) + 4);

      exit(status);
      break;  
    }

    //syscall1 (SYS_EXEC, file);
    case SYS_EXEC: //2
    {
      check_valid_pointer((f->esp) + 4); //file = first
      f->eax = process_execute(*(const char **)(f->esp+4));
      //process_execute(*(char **)((f->esp) + 4)); 
      break;
    }

    //syscall1 (SYS_WAIT, pid);
    case SYS_WAIT: //3   //FIXME:
    {
      check_valid_pointer((f->esp) + 4); //pid = tid = first
      f->eax = process_wait((tid_t)first);
      // process_wait(thread_tid());
      break;
    }

    //syscall2 (SYS_CREATE, file, initial_size);
    case SYS_CREATE: //4
    {
      if(first == NULL)
      {
        exit(-1);
      }      
      check_valid_pointer((f->esp) + 4); //file = first
      check_valid_pointer((f->esp) + 8); //initial_size = second
      check_valid_pointer(second); //also a pointer 

      f->eax = filesys_create((const char *)first, (int32_t)(second));
      break;
    }

    //syscall1 (SYS_REMOVE, file);
    case SYS_REMOVE: //5
    {
      if(first == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //file = first
      f->eax = filesys_remove((const char *)first);
      break;
    }

    //syscall1 (SYS_OPEN, file);
    case SYS_OPEN: //6
    { 
      if(first == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //file = first
      check_valid_pointer(*(char **)(f->esp + 4)); //also a pointer
      // if(get_user((uint8_t *)(f->esp + 4)) == -1) //check if null or unmapped
      // {
      //   exit(-1);
      // }
      struct file* fp = filesys_open(*(char **)(f->esp + 4));
     
      if(fp == NULL)  //file could not opened
      {
        f->eax = -1;
      }
      else
      {
        f->eax = -1;
        for(i = 3; i < 128; ++i)
        {
          if(thread_current()->f_d[i] == NULL)
          {
            thread_current()->f_d[i] = fp;
            f->eax = i;
            break;  //end for loop
          }
        }
      }
      break;  //end open
    }

    //syscall1 (SYS_FILESIZE, fd);
    case SYS_FILESIZE: //7
    {
      if(thread_current()->f_d[first] == NULL)
      {
        exit(-1);
      }
      // if(fd == NULL)
      // {
      //   exit(-1);
      // }
      check_valid_pointer((f->esp) + 4); //fd = first
      f->eax = file_length(thread_current()->f_d[first]);
      break;
    }

    //syscall3 (SYS_READ, fd, buffer, size);
    case SYS_READ: //8 
    {
      check_valid_pointer((f->esp) + 4); //fd = first
      check_valid_pointer((f->esp) + 8); //buffer = second
      check_valid_pointer((f->esp) + 12); //size = third

      if(get_user((uint8_t *)(f->esp + 4)) == -1) //check if null or unmapped
      {
        exit(-1);
      }

      int i;
      if (first == 0)  //stdin: keyboard input from input_getc()
      {
        for(i = 0; i < third; ++i)
        {
          if(((char *)second)[i] == NULL)
          {
            break; //remember i
          }
        }
      }
      else if(first > 2)  //not stdin
      {
        if(thread_current()->f_d[first] == NULL)
        {
          exit(-1);
        }
        f->eax = file_read(thread_current()->f_d[first], second, third);
        break; //end read
      }
      f->eax = i;
      break;
    }

    //syscall3 (SYS_WRITE, fd, buffer, size);
    case SYS_WRITE: //9
    {
      check_valid_pointer((f->esp) + 4); //fd = first
      check_valid_pointer((f->esp) + 8); //buffer = second
      check_valid_pointer((f->esp) + 12); //size = third

      int fd = first;
      if(fd == 1)  //stdout: onsole io
      {
        putbuf(second, third);
        f->eax = third;
        break; //end write
      }
      else if(fd > 2)  //not stdout
      {
        if(thread_current()->f_d[fd] == NULL)
        {
          exit(-1);
        }
        f->eax = file_write(thread_current()->f_d[fd], second, third);
        break;  //end write
      }
      f->eax = -1;
      break;
    }

    //syscall2 (SYS_SEEK, fd, position);
    case SYS_SEEK: //10
    {
      int fd = first;
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd = first
      check_valid_pointer((f->esp) + 8); //buffer = second
      check_valid_pointer(second); //also a pointer

      file_seek(thread_current()->f_d[fd], (unsigned)second);
      break;
    }

    //return syscall1 (SYS_TELL, fd);
    case SYS_TELL: //11
    {
      int fd = first;
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd = first
      file_tell(thread_current()->f_d[fd]);
      break; 
    }

    //syscall1 (SYS_CLOSE, fd);
    case SYS_CLOSE: //12
    {
      int fd = first;
      if(thread_current()->f_d[fd] == NULL)
      {
        exit(-1);
      }
      check_valid_pointer((f->esp) + 4); //fd = first
      file_close(thread_current()->f_d[fd]);
      thread_current()->f_d[fd] = NULL;  //file closed -> make it NULL
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
    if(thread_current()->f_d[i] != NULL)  //close all files before die
    {
      file_close(thread_current()->f_d[i]);  
    }
  }
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit(); 
}