#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include "lib/kernel/list.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/inode.h"

struct file
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

void    __sys_halt();                   /*void 	    halt (void) NO_RETURN                               */
void    __sys_exit(uint32_t *esp);      /*void 	    exit (int status) NO_RETURN                         */
pid_t   __sys_exec(uint32_t *esp);      /*pid_t 	exec (const char *file)                             */
int     __sys_wait(uint32_t *esp);      /*int 	    wait (pid_t)                                        */
bool    __sys_create(uint32_t *esp);    /*bool 	    create (const char *file, unsigned initial_size)    */
bool    __sys_remove(uint32_t *esp);    /*bool 	    remove (const char *file)                           */
int     __sys_open(uint32_t *esp);      /*int 	    open (const char *file)                             */
int     __sys_filesize(uint32_t *esp);  /*int 	    filesize (int fd)                                   */
int     __sys_read(uint32_t *esp);      /*int 	    read (int fd, void *buffer, unsigned length)        */
int     __sys_write(uint32_t *esp);     /*int 	    write (int fd, const void *buffer, unsigned length) */
void    __sys_seek(uint32_t *esp);      /*void 	    seek (int fd, unsigned position)                    */
unsigned    __sys_tell(uint32_t *esp);  /*unsigned 	tell (int fd)                                       */
void    __sys_close(uint32_t *esp);     /*void 	    close (int fd)                                      */
/*
mapid_t 	mmap (int fd, void *addr)
void 	munmap (mapid_t)
bool 	chdir (const char *dir)
bool 	mkdir (const char *dir)
bool 	readdir (int fd, char name[READDIR_MAX_LEN+1])
bool 	isdir (int fd)
int 	inumber (int fd)*/

void sys_exit_internal(int status);     /* exit function to be used internally */
bool is_valid_user_addr(void *uaddr);   /* checks if the user address is valid */

bool is_valid_user_addr(void *uaddr) {
    if (!is_user_vaddr(uaddr) || uaddr == NULL)  /* prevent possible assertion failure */
        return false;
    uint32_t *pd = thread_current()->pagedir;
    return (pagedir_get_page(pd, uaddr) != NULL);

}


static void
syscall_handler (struct intr_frame *f UNUSED)
{
    uint32_t *esp       = f->esp;
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    uint32_t sysnum     = *esp++;

    switch(sysnum) {
        /*Project2*/
        case SYS_HALT           : __sys_halt(); break;
        case SYS_EXIT           : __sys_exit(esp); break;
        case SYS_EXEC           : f->eax = __sys_exec(esp); break;
        case SYS_WAIT           : f->eax = __sys_wait(esp); break;
        case SYS_CREATE         : f->eax = __sys_create(esp); break;
        case SYS_REMOVE         : f->eax = __sys_remove(esp); break;
        case SYS_OPEN           : f->eax = __sys_open(esp); break;
        case SYS_FILESIZE       : f->eax = __sys_filesize(esp); break;
        case SYS_READ           : f->eax = __sys_read(esp); break;
        case SYS_WRITE          : f->eax = __sys_write(esp); break;
        case SYS_SEEK           : __sys_seek(esp); break;
        case SYS_TELL           : f->eax = __sys_tell(esp); break;
        case SYS_CLOSE          : __sys_close(esp); break;
        /* Below, Project3 and later*/
        case SYS_MMAP           : break;
        case SYS_MUNMAP         : break;

        case SYS_CHDIR          : break;
        case SYS_MKDIR          : break;
        case SYS_READDIR        : break;
        case SYS_ISDIR          : break;
        case SYS_INUMBER        : break;
        default                 : break;
    }

  //thread_exit ();
}

void __sys_halt() {
    shutdown_power_off();
}

/**
    Terminates the current user program, returning status to the kernel. If the process's parent waits for it,
    this is the status that will be returned. Conventionally, a status of 0 indicates success and nonzero values indicate errors
*/
void __sys_exit(uint32_t *esp) {
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    int *status = *(esp++);
    sys_exit_internal(status);
}
void sys_exit_internal(int status) { //int pointer? or just int?
    char *save_ptr;
    char *name = strtok_r(thread_name(), " ", &save_ptr);
    printf("%s: exit(%d)\n", name, status);
    thread_current()->exit_status = status;
    /*Wake up parents waiting for this child*/
    thread_exit();  /* thread_exit calls process_exit() and process_exit() will wake up parents */
}

/**
    Runs the executable whose name is given in cmd_line, passing any given arguments, and returns the
    new process's program id (pid). Must return -1, which otherwise should not be valid pid, if the program
    cannot load or run for any reason. Thus, the parent process cannot return from the exec until process
    successfully loaded its executable. Use sync.
*/
pid_t   __sys_exec(uint32_t *esp){
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    char *cmd_line = (char *)(*esp++);
    if (!is_valid_user_addr(cmd_line))
        sys_exit_internal(-1);

    lock_acquire(&file_lock);

    tid_t tid = process_execute(cmd_line);

    lock_release(&file_lock);

    return tid;
}

/**
    Waits for child process' pid and returns child's exit status
*/
int __sys_wait(uint32_t *esp){
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    pid_t *child_pid = *esp++;
    int child_exit_status = process_wait(child_pid);
    return child_exit_status;
}

/**
   Creates new file called file initially initial_size bytes in size.
    returns true if successful, false otherwise.
    Createing a new file does not open it.
*/
bool    __sys_create(uint32_t *esp) {
    //hex_dump((unsigned int)esp, esp, 64, 1);
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    const char *file = (char *)(*esp++);
    //hex_dump((unsigned int)esp, esp, 64, 1);
    if (!is_valid_user_addr(esp) || !is_valid_user_addr(file))
        sys_exit_internal(-1);
    if (file == NULL)
        sys_exit_internal(-1);
    uint32_t initial_size = (*esp++);
    lock_acquire(&file_lock);
//    printf("\n\t%d: lock acquired in create\n", thread_tid());
    bool success = filesys_create(file, initial_size);
//    printf("\n\t%d: lock releasing in create\n", thread_tid());
    lock_release(&file_lock);
    return success;
}

/**
    Deletes the file called file. Returns true if successful, false otherwise.
    A file may be removed regardless of whether it is opened or closed, and removing an
    open file does not close it.
*/
bool    __sys_remove(uint32_t *esp){
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    char *file = (char *)(*esp++);
    /* TODO: Remove the lock for the file */
    lock_acquire(&file_lock);
    bool success = filesys_remove(file);
    lock_release(&file_lock);
    return success;
}

/**
    Opens the file called file. Returns a nonnegative integer handle called a "file descriptor" (fd),
    or -1 if the file could not be opened
*/
int     __sys_open(uint32_t *esp) {
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    char *file = (char *)(*esp++);
    if (file == NULL)
        return -1;
    if (!is_valid_user_addr(file))
        sys_exit_internal(-1);
    struct thread *t = thread_current();
    int fd;                         /* fd 0, 1 are reserved for stdin and stdout */
    for(fd = 2; fd < 128; fd++){    /* 128 is pintos-doc specified limit */
        if(t->fdtable[fd] == NULL){
            lock_acquire(&file_lock);

            //hex_dump((unsigned int)file, file, 128, 1);
            t->fdtable[fd] = filesys_open(file);

            lock_release(&file_lock);
            if(t->fdtable[fd] == NULL) {
                //printf("\n\tFAILFAILFAIL\n");
                return -1;
            }else {

                return fd;
            }
        }
    }
    return -1;
}

/**
    Returns the size, in bytes, of the ile open as fd
*/
int     __sys_filesize(uint32_t *esp){
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    int fd = (int)*(esp++);
    struct thread *t = thread_current();
    if (t->fdtable[fd] == NULL)
        return -1;
    lock_acquire(&file_lock);
    int size = (int)file_length(t->fdtable[fd]);
    lock_release(&file_lock);
    return size;
}

/**
    Reads size bytes from the file open as fd into buffer. Returns the number of bytes actually read
    (0 at end of file), or -1 if the file could not be read (due to a condition
    other than end of file). fd 0 reads from the keyboard using input_getc()
*/
int     __sys_read(uint32_t *esp){
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    int fd = (int)*(esp++);
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    char *buffer = (char *)(*esp++);
    if (!is_valid_user_addr(esp) || !is_valid_user_addr(buffer))
        sys_exit_internal(-1);
    unsigned size = (*esp++);

    if (fd < 0 || fd == 1 || fd >= 128)
        sys_exit_internal(-1);
    /* Console read */
    if( fd == 0 ){
        uint32_t i = 0;
        while (1) {
            uint8_t key = input_getc();
            if(key==13){                    /* when input == carrage return  */
                buffer[i] = '\0';           /* put delimiter */
                break;
            }
            else buffer[i++] = key;
            if (i >= size)
                break;
        }
        return size;
    }
    /*read from user process file*/
    else if( fd > 1 ){
        struct thread *t = thread_current();
        if(t->fdtable[fd] == NULL ){
            return -1;
        } else if(t->fdtable[fd] == t->executed_file){
            /* trying to modify the code file */
            return -1;
        }else{

            lock_acquire(&file_lock);
            //printf("\n\t%d: lock acquired in read\n", thread_tid());
            int bytes_read = file_read(t->fdtable[fd], buffer, (off_t)size);
            //printf("\n\t%d: lock released in read\n", thread_tid());
            lock_release(&file_lock);
            return bytes_read;
        }
    }
    /*should not get here*/
    return -1;
}

/**
    Writes 'size' bytes from 'buffer' to the open file 'fd' Then returns the number of bytes actually written.
*/
int __sys_write(uint32_t *esp) {
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    int fd  = (int)*(esp++);
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    const void *buffer = *(esp++);
    if (!is_valid_user_addr(esp) || !is_valid_user_addr(buffer))
        sys_exit_internal(-1);
    unsigned size = (unsigned)*(esp++);

    if (fd < 1 || fd >= 128)
        sys_exit_internal(-1);

    if (fd == 1) {              /// the console write
        putbuf(buffer, size);
        return size;
    }else if (fd > 1){          /// other than console
        struct thread *t = thread_current();
        if (t->fdtable[fd] == NULL )
            return -1;
        else {

            lock_acquire(&file_lock);
//            printf("\n\tcurrent: %x * %x \n\tparent: %x * %x \n\tfiletable: %x * %x",
//                   &t->executed_file, t->executed_file,
//                   &t->parent_thread->executed_file, t->parent_thread->executed_file,
//                   &t->fdtable[fd], t->fdtable[fd]);
            int bytes_written = file_write(t->fdtable[fd],buffer,size);
            //printf("\n\twritten: %d\n",bytes_written);
            lock_release(&file_lock);

            return bytes_written;
        }
    }else {                     /// should not get here
        return -1;
    }
}

/**
    Changes the next byte to be read or written in open file fd to position,
    expressed in btes from the beginning of the file. pos = 0 = file's start
*/
void    __sys_seek(uint32_t *esp) {
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    int fd = (int)*(esp++);
    struct thread *t = thread_current();
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    uint32_t pos = (uint32_t)*(esp++);
    file_seek(t->fdtable[fd], pos);
}

/**
    Returns the position of the next byte to be read or written in open file fd,
    expressed in bytes from the beginning of the file
*/
unsigned    __sys_tell(uint32_t *esp) {
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    int fd = (int)*(esp++);
    lock_acquire(&file_lock);
    unsigned pos = file_tell(thread_current()->fdtable[fd]);
    lock_release(&file_lock);
    return pos;
}

/**
    Closes file descriptor fd. Exiting or terminating a process implicitly closes all its
    open file descriptors, as if by calling this function for each one
*/
void    __sys_close(uint32_t *esp){
    if (!is_valid_user_addr(esp))
        sys_exit_internal(-1);
    int fd = (int)*(esp++);
    if (fd <= 1 || fd >= 128)
        sys_exit_internal(-1);
    struct thread *t = thread_current();

    lock_acquire(&file_lock);
    file_close(t->fdtable[fd]);
    lock_release(&file_lock);
    t->fdtable[fd] = NULL;
}
