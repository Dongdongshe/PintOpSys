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

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
    if (!is_user_vaddr(uaddr))  /* prevent possible assertion failure */
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
    int *status = *(esp)++;
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

pid_t   __sys_exec(uint32_t *esp){

    char *cmd_line = (char *)(*esp)++;
    tid_t tid = process_execute(cmd_line);
    return tid;
}

/**
    Waits for child process' pid and returns child's exit status
*/
int __sys_wait(uint32_t *esp){
    pid_t *child_pid = *(esp)++;
    int child_exit_status = process_wait(child_pid);
    return child_exit_status;
}
/**
   Creates new file called file initially initial_size bytes in size.
    returns true if successful, false otherwise.
    Createing a new file does not open it.
*/
bool    __sys_create(uint32_t *esp) {
    const char *file = (char *)(*esp)++;
    unsigned initial_size = (unsigned)(*esp)++;
    return filesys_create(file, initial_size);
}

/**
    Deletes the file called file. Returns true if successful, false otherwise.
    A file may be removed regardless of whether it is opened or closed, and removing an
    open file does not close it.
*/
bool    __sys_remove(uint32_t *esp){
    char *file = (char *)(*esp)++;
    return filesys_remove(file);
}

/**
    Opens the file called file. Returns a nonnegative integer handle called a "file descriptor" (fd),
    or -1 if the file could not be opened
*/
int     __sys_open(uint32_t *esp) {
    char *file = (char *)(*esp)++;
    if (file == NULL)
        return -1;
    struct thread *t = thread_current();
    int fd;                         /* fd 0, 1 are reserved for stdin and stdout */
    for(fd = 2; fd < 128; fd++){    /* 128 is pintos-doc specified limit */
        if(t->fdtable[fd] == NULL){
            t->fdtable[fd] = filesys_open(file);
            if(t->fdtable[fd] == NULL)
                return -1;
            else
                return fd;
        }
    }
    return -1;
}
int     __sys_filesize(uint32_t *esp)
{return 0;}
int     __sys_read(uint32_t *esp)
{return 0;}

/**
    Writes 'size' bytes from 'buffer' to the open file 'fd' Then returns the number of bytes actually written.
*/
int __sys_write(uint32_t *esp) {
    int fd                  = *(esp)++;
    const void *buffer      = *(esp)++;
    unsigned size           = *(esp)++;

    /**TODO: validate the memory address*/
    if (fd == 1) {              /// the console write
        putbuf(buffer, size);
        return size;
    }else if (fd > 1){          /// other than console
        /**TODO: file write*/
        return -1;
    }else {                     /// should not get here
        return -1;
    }
}

void    __sys_seek(uint32_t *esp)
{}
unsigned    __sys_tell(uint32_t *esp)
{return 0;}
void    __sys_close(uint32_t *esp)
{}
