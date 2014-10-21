#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "devices/shutdown.h"
#include "filesys/filesys.h"

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

static void
syscall_handler (struct intr_frame *f UNUSED)
{
    //printf ("system call!\n");
    uint32_t *esp = f->esp;
    uint32_t sysnum    = *esp++;

    switch(sysnum) {
        /*Project2*/
        case SYS_HALT           : __sys_halt(); break;
        case SYS_EXIT           : __sys_exit(esp); break;
        case SYS_EXEC           : __sys_exec(esp); break;
        case SYS_WAIT           : __sys_wait(esp); break;
        case SYS_CREATE         : __sys_create(esp); break;
        case SYS_REMOVE         : __sys_remove(esp); break;
        case SYS_OPEN           : __sys_open(esp); break;
        case SYS_FILESIZE       : __sys_filesize(esp); break;
        case SYS_READ           : __sys_read(esp); break;
        case SYS_WRITE          : __sys_write(esp); break;
        case SYS_SEEK           : __sys_seek(esp); break;
        case SYS_TELL           : __sys_tell(esp); break;
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
    struct thread *curr = thread_current();
    printf("%s: exit(%d)\n", curr->name, *status);
    /*TODO: Wake up parents waiting for this child*/
    lock_acquire(&curr->parent_thread->waitLock);
    curr->isFinished = true;
    curr->exit_status = status;   /** Process exit normally */
    cond_signal(&curr->parent_thread->waitCV, &curr->parent_thread->waitLock); /** signal and release the parent */
    lock_release(&curr->parent_thread->waitLock);
    thread_exit();
}

pid_t   __sys_exec(uint32_t *esp){
    char *file = (char *)(*esp)++;
    tid_t tid = process_execute(file);
    return tid;
}

/**
    Waits for child process' pid and returns child's exit status
*/
int __sys_wait(uint32_t *esp){
    pid_t *child_pid = *(esp)++;
    int child_exit_status = process_wait(*child_pid);
    return child_exit_status;
}
bool    __sys_create(uint32_t *esp)
{return false;}
bool    __sys_remove(uint32_t *esp){
    char *file = (char *)(*esp)++;
    return filesys_remove(file);
}
int     __sys_open(uint32_t *esp)
{return 0;}
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
        return 0;
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
