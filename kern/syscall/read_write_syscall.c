#include "read_write_syscalls.h"

struct openfile{
  struct vnode* v;
  int ref_cnt;
  int offset;
  int mode;
};
struct spinlock open_file_lock = SPINLOCK_INITIALIZER;

struct openfile sys_open_files[SYS_OPEN_FILE_MAX];

int sys_open(char* filename, int flags){
  struct vnode* v;
  int i, sfd=-1, fd=-1;
  
  if(vfs_open(filename, flags, 0777, &v)){
    return -ENOENT;
  }
  // insert into sys_open_file table
  spinlock_acquire(&open_file_lock);
  for(i=0; i < SYS_OPEN_FILE_MAX; i++){
    if(sys_open_files[i].v == v && sys_open_files[i].mode == flags){
      sfd = i;
      break;
    }
  }
  for(i=0; i < SYS_OPEN_FILE_MAX && sfd < 0; i++){
    if(sys_open_files[i].v == NULL){
      sfd = i;
      sys_open_files[i].v = v;
      sys_open_files[i].ref_cnt = 1;
      sys_open_files[i].offset = 0;
      sys_open_files[i].mode = flags;
      break;
    }
  }
  spinlock_release(&open_file_lock);
  // check if limit reached
  if(i >= SYS_OPEN_FILE_MAX)
    return -ENFILE;
  
  // insert into per process file table
  fd = proc_add_file(&sys_open_files[sfd]);
  if(fd < 0){
    // errors so remove from sys table
    spinlock_acquire(&open_file_lock);
    sys_open_files[i].v = NULL;
    sys_open_files[i].ref_cnt = 0;
    spinlock_release(&open_file_lock);
  }
  return fd;
}

int sys_close(int fd){
  struct openfile* proc_file;
  int sfd=-1;
  if(fd < 0 || fd >= OPEN_MAX)
    return -ENOENT;
  proc_file = proc_rem_file(fd);
  if(proc_file == NULL)
    return -ENOENT;

  sfd = (int) (proc_file - sys_open_files);
  if(sfd < 0 || sfd >= SYS_OPEN_FILE_MAX)
    return -ENOENT;

  spinlock_acquire(&open_file_lock);
  sys_open_files[sfd].ref_cnt--;
  if(sys_open_files[sfd].ref_cnt <= 0)
    sys_open_files[sfd].v = NULL;
  spinlock_release(&open_file_lock);

  return 0;
}

int sys_read(int file, void* buffer, int size){
  char* read_buffer = (char*)buffer;
  int i = file, result;
  char c;

  if(buffer == NULL)
    return 0;
  if(file < 0 || file >= OPEN_MAX)
    return -EBADF;
  
  if(file == STDIN_FILENO){
    for(i = 0; i < size; i++){
      c = getch();
      if(c == 3){
	read_buffer[i] = '\0';
	break;
      }
      read_buffer[i] = c;
    }
  }else{
    // get openfile struct
    struct openfile* open_file;
    char *buff;
    struct iovec iov;
    struct uio ku;
    int transferred = 0;
    
    open_file = curproc->open_files[file];
    // check if file is open for reading
    if(open_file == NULL || open_file->mode & 1) 
      return -EBADF;
    // alloc kernel buffer
    buff = kmalloc(size);
    if(buff == NULL)
      return -ENOMEM;
    // read into kernel buffer
    uio_kinit(&iov, &ku, buff, size, open_file->offset, UIO_READ);
    result = VOP_READ(open_file->v, &ku);
    if(result){
      kfree(buff);
      return -EIO;
    }
    transferred = size - ku.uio_resid;
    // move into user space
    if(copyout(buff, buffer, transferred)){
      return -EIO;
    }
    i = transferred;
    kfree(buff);
  }
  return i;
}

int sys_write(int file, void* buffer, int size){
  char* write_buffer = (char*) buffer;
  int nw = file;

   if(buffer == NULL)
    return 0;
  if(file < 0 || file >= OPEN_MAX)
    return -EBADF;

  if(file == STDOUT_FILENO || file == STDERR_FILENO){
     for(nw = 0; nw < size; nw++){
       putch(write_buffer[nw]); 
     }
  }else{
    struct openfile *open_file;
    struct uio u;
    struct iovec iov;
    int result;

    // get openfile
    open_file = curproc->open_files[file];
    // check file exists and opened for writing
    if(open_file == NULL || (open_file->mode & 3) == 0)
      return -EBADF;
    
    iov.iov_ubase = (userptr_t)buffer;
    iov.iov_len = size;       // length of the memory space
    u.uio_iov = &iov;
    u.uio_iovcnt = 1;
    u.uio_resid = size;          // amount to read from the file
    u.uio_offset = open_file->offset;
    u.uio_segflg = UIO_USERSPACE;
    u.uio_rw = UIO_WRITE;
    u.uio_space = proc_getas();

    result = VOP_WRITE(open_file->v, &u);
    if(result)
      return -EIO;
    nw = size - u.uio_resid;
  }
  return nw;
}
