#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
void open_wrapper(struct intr_frame *f,void* esp);
int open (const char *file);
void create_wrapper(struct intr_frame *f,void* esp);
bool create (const char *file, unsigned initial_size);
void remove_wrapper(struct intr_frame *f,void* esp);
bool remove (const char *file);
void read_wrapper (struct intr_frame *f,void* esp);
int read (int fd, void *buffer, unsigned size);
struct fd_element* get_fd(int fd);
static void validate_void_ptr(const void* pt);
int write (int fd, const void *buffer, unsigned size);
//static struct lock lockf;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&files_sync_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  //modified
  validate_void_ptr((const void*)f->esp);
  int sys_code = *(int*)f->esp;
  void *args = f -> esp;
  switch(sys_code){
       case SYS_HALT:
       halt_wrapper();
            break;
       case SYS_EXIT:
           exit_wrapper(f->esp);
           break;
       case SYS_EXEC:
           exec_wrapper(f);
           break;
       case SYS_WAIT:
          wait_wrapper(f);
           break;
       case SYS_CREATE:
            create_wrapper(f,f -> esp);
           break;
       case SYS_REMOVE:
            remove_wrapper(f,f -> esp);
           break;
       case SYS_OPEN:
           open_wrapper(f,f->esp);
           break;
       case SYS_FILESIZE:
       
           break;
       case SYS_READ:
           read_wrapper(f,f -> esp);
           break;
       case SYS_WRITE:
           write_wrapper(f,f -> esp);
           break;
       case SYS_SEEK:

           break;
       case SYS_TELL:

           break;
       case SYS_CLOSE:

           break;
    default:
        exit(-1);
        break;
}
}
void halt_wrapper(){
  halt();
}
void halt(){
  shutdown_power_off();
}


void create_wrapper(struct intr_frame *f,void* esp){
    void* file_name = (void*)(*((int*)esp+1));
    validate_void_ptr(file_name);
    f -> eax = create((const char*)(*((int*)esp+1)),(unsigned )(*((int*)esp+2)));
}
bool create (const char *file, unsigned initial_size){
    lock_acquire(&files_sync_lock);
    bool created = filesys_create(file, initial_size);
    lock_release(&files_sync_lock);
    return created;
}

void remove_wrapper(struct intr_frame *f,void* esp){
    void* file_name = (void*)(*((int*)esp+1));
    validate_void_ptr(file_name);
    f -> eax = remove((const char*)(*((int*)esp+1)));
}
bool remove (const char *file){
    lock_acquire(&files_sync_lock);
    bool removed = filesys_remove(file);
    lock_release(&files_sync_lock);
    return removed;
}

void open_wrapper(struct intr_frame *f,void* esp){
    void* valid = (void*)(*((int*)esp+1));
    validate_void_ptr(valid);
    f -> eax = open((const char *)(*((int*)esp+1)));
}
int open (const char *file){
    int fd = -1;    //returned if the file could not be opened.
    lock_acquire(&files_sync_lock);
    struct thread *current = thread_current ();
    struct file * opened_file = filesys_open(file);
    lock_release(&files_sync_lock);
    if(opened_file != NULL)
    {
        current->fd_size = current->fd_size + 1;
        fd = current->fd_size;
        struct fd_element *file = (struct fd_element*) malloc(sizeof(struct fd_element));
        file->fd = fd;
        file->file = opened_file;
        // add the fd_element to the thread fd_list
        list_push_back(&current->fd_list, &file->element);
    }
    return fd;
}

void read_wrapper (struct intr_frame *f,void* esp){
    int fd = *((int*)esp+1);
    void* buffer = (void*)(*((int*)esp+2));
    unsigned size = * ((unsigned*)esp+3);
    validate_void_ptr(buffer);
    validate_void_ptr(buffer+size);
    f->eax = read(fd,buffer,size);
}
int read(int fd, void *buffer, unsigned size){

    int bytes_read = -1;
    if(fd == 0)     //keyboard read
    {
        bytes_read = input_getc();
    }
    else if(fd > 0)     //file read
    {
        struct fd_element *fd_elem = get_fd(fd);
        if(fd_elem == NULL || buffer == NULL)
        {
            return -1;
        }
        //get the file
        struct file *file = fd_elem->file;
        lock_acquire(&files_sync_lock);
        bytes_read = file_read(file, buffer, size);
        lock_release(&files_sync_lock);
        if(bytes_read < (int)size && bytes_read != 0)
        {
            //some error happened
            bytes_read = -1;
        }
    }
    return bytes_read;
}

void write_wrapper(struct intr_frame *f,void* esp){
  
    int fd = *((int*)esp+1);
    void* buffer = (void*)(*((int*)esp+2));
    unsigned size = * ((unsigned*)esp+3);
  
    validate_void_ptr(buffer);
    validate_void_ptr(buffer+size);
     f->eax= write(fd,buffer,size);

}
int write(int fd, const void *buffer, unsigned size){
    int written=-1;  
    struct file* f;
    lock_acquire(&files_sync_lock);
    if(fd==1){
        putbuf(buffer,(size_t) size);
    }
    else if(fd==0){
        lock_release(&files_sync_lock);
        return written;
    }
    /*else if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer+size)){
      lock_release (&files_sync_lock);
      exit (-1);
    }*/
    else{
        struct fd_element *search;
        struct list_elem *elem;
        search=get_fd(fd);
        if(!search){
           lock_release(&files_sync_lock);
            return written;
        }
        f=search->file;
        written=file_write(f,buffer,size);
    }
    lock_release(&files_sync_lock);
    return written;
}



void exit_wrapper(void *esp){
    int* temp = (int*) esp+1;
    //int status = get_int((int**)&temp);
    int status = *(temp);
    validate_void_ptr((const void*)temp);
    exit(status);
}
void exit(int status){
   thread_current()->exit_status = status;
   thread_exit();
}

void exec_wrapper(struct intr_frame *f){
    int* temp = (int*)f->esp+1;
    validate_void_ptr((const void*)temp);
    validate_void_ptr((const void*)*temp);
    char *cmd = (void*)(*(temp));
    f->eax = exec(cmd);
}

tid_t exec(const char *cmd_line){
     tid_t tid = process_execute(cmd_line);
     if(thread_current()->child_creation_sucess == false){
     return -1;
     }
     return tid;
}

void wait_wrapper(struct intr_frame *f){
    int* temp = (int*)f->esp +1;
    //int pid = get_int((int**)&temp);
    validate_void_ptr((const void*)temp);
    int pid = *(temp);
    f->eax = wait(pid);
}

int wait(tid_t pid){
    int status = process_wait(pid);
    return status;
}


static void validate_void_ptr(const void* pt){
    if(!((pt!=NULL)&&(is_user_vaddr(pt))&&(pagedir_get_page (thread_current()->pagedir, pt)!=NULL))){
         exit(-1);
     }
} 
struct fd_element* get_fd(int fd)
{
    struct list_elem *e;
    for (e = list_begin (&thread_current()->fd_list); e != list_end (&thread_current()->fd_list);
         e = list_next (e))
    {
        struct fd_element *fd_elem = list_entry (e, struct fd_element, element);
        if(fd_elem->fd == fd)
        {
            return fd_elem;
        }
    }
    return NULL;
}