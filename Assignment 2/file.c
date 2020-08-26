#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>
#include<pipe.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/
void free_file_object(struct file *filep)
{
    if(filep)
    {
       os_page_free(OS_DS_REG ,filep);
       stats->file_objects--;
    }
}

struct file *alloc_file()
{
  
  struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
  file->fops = (struct fileops *) (file + sizeof(struct file)); 
  bzero((char *)file->fops, sizeof(struct fileops));
  stats->file_objects++;
  return file; 
}

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
  kbd_read(buff);
  return 1;
}

static int do_write_console(struct file* filep, char * buff, u32 count)
{
  struct exec_context *current = get_current_ctx();
  return do_write(current, (u64)buff, (u64)count);
}

struct file *create_standard_IO(int type)
{
  struct file *filep = alloc_file();
  filep->type = type;
  filep->ref_count=1;
  if(type == STDIN)
     filep->mode = O_READ;
  else
      filep->mode = O_WRITE;
  if(type == STDIN){
        filep->fops->read = do_read_kbd;
  }else{
        filep->fops->write = do_write_console;
  }
  filep->fops->close = generic_close;
  return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
   int fd = type;
   struct file *filep = ctx->files[type];
   if(!filep){
        filep = create_standard_IO(type);
   }else{
         filep->ref_count++;
         fd = 3;
         while(ctx->files[fd])
             fd++; 
   }
   ctx->files[fd] = filep;
   return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/



void do_file_fork(struct exec_context *child)
{
   /*TODO the child fds are a copy of the parent. Adjust the refcount*/
  if(child==NULL)
    return;
  for(int i=0;i<MAX_OPEN_FILES;i++)
  {
    if(child->files[i]!=NULL){
      child->files[i]->ref_count+=1;
    }
  }
  
  return;
}

void do_file_exit(struct exec_context *ctx)
{
   /*TODO the process is exiting. Adjust the ref_count
     of files*/
  if(ctx==NULL)
    return;
  for(int i=0;i<MAX_OPEN_FILES;i++)
  {
    if(ctx->files[i]!=NULL)
    {
      int ret_val = generic_close(ctx->files[i]);
      ctx->files[i]=NULL; //to free the file descriptor //when the closing does not happen beacuse of close
    }
  }
  return ;
}

long generic_close(struct file *filep)
{
  /** TODO Implementation of close (pipe, file) based on the type 
   * Adjust the ref_count, free file object
   * Incase of Error return valid Error code 
   */
    int ret_fd = -EINVAL; 
    if(filep == NULL)
      return -EINVAL;
    if(filep->type== REGULAR )      //REGULAR FILE
    {
      if(filep->ref_count!=1)
      {
        filep->ref_count = filep->ref_count-1;
      }
      else
      {
        filep->ref_count = 0;
        free_file_object(filep);
      }
      ret_fd=1;
    }
    else if(filep->type == PIPE)
    {
      if(filep->ref_count!=1){
        filep->ref_count--;
      }
      else
      {
        filep->ref_count=0;
        if(filep->mode==O_READ){
          filep->pipe->is_ropen=0;
        }
        if(filep->mode==O_WRITE){
          filep->pipe->is_wopen=0;
        }
        if(filep->pipe->is_wopen==0 && filep->pipe->is_ropen==0)
          free_pipe_info(filep->pipe);
        free_file_object(filep);
      }
      ret_fd=1;
    }
    else                  //STDIN, STDOUT,STDERR cases
    {
      if(filep->ref_count!=1)
      {
        filep->ref_count = filep->ref_count-1;
      }
      else
      {
        filep->ref_count = 0;
        free_file_object(filep);
      }
      ret_fd=1;
    }
    return ret_fd;
}

static int do_read_regular(struct file *filep, char * buff, u32 count)
{
   /** TODO Implementation of File Read, 
    *  You should be reading the content from File using file system read function call and fill the buf
    *  Validate the permission, file existence, Max length etc
    *  Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 
    if(filep==NULL|| buff == NULL || count<0)
      return -EINVAL;
    // struct inode *existing_inode = lookup_inode(filename);
    if(filep->mode & O_READ){
      u64 i, size = 0, s_pos;
    	
      long int remain_len;
      remain_len = (long int)filep->inode->file_size - filep->offp;
      if( remain_len <= 0  || remain_len <count)
        return -EINVAL;
      // size = ( count > remain_len ? remain_len : count );
      s_pos = filep->inode->s_pos;
      for( i=0 ; i<count ; i++)
      {
        buff[i] = *(char *)(s_pos+ filep->offp + i);
      }
      filep->offp = filep->offp + count;
      ret_fd = count;
    }
    else{
      ret_fd = -EACCES;
    }
    return ret_fd;
}


static int do_write_regular(struct file *filep, char * buff, u32 count)
{
    /** TODO Implementation of File write, 
    *   You should be writing the content from buff to File by using File system write function
    *   Validate the permission, file existence, Max length etc
    *   Incase of Error return valid Error code 
    * */
    if(filep==NULL|| buff == NULL || count<0)
        return -EINVAL;
    int ret_fd = -EINVAL;
    if(filep->mode & O_WRITE){ 

      u64 i, size, s_pos,max_pos;

      long int max_len;

      
      max_len = (long int)filep->inode->e_pos - filep->offp - (long int)filep->inode->s_pos;

      if( count > max_len )
      {
        return -EINVAL; // file_size exceeded
      }
      s_pos = filep->inode->s_pos;
      if(filep->inode->s_pos + filep->offp > filep->inode->max_pos){
        int t = (filep->inode->s_pos + filep->offp - filep->inode->max_pos);
        max_pos = filep->inode->max_pos;
        for(int i=0;i<t;i++)
          *(char *)(max_pos + (u64)t) = '\0';
      }

      for( i=0 ; i<count ; i++)
      {
        *(char *)(s_pos +filep->offp + i) = buff[i];
      }

      if( filep->inode->s_pos + filep->offp + count > filep->inode->max_pos )
        filep->inode->max_pos = filep->inode->s_pos + filep->offp + count;
      filep->inode->file_size = filep->inode->max_pos - filep->inode->s_pos;
      filep->offp = filep->offp + count;
      ret_fd = count;
    }else{
      ret_fd=-EACCES;
    }
    return ret_fd;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
    /** TODO Implementation of lseek 
    *   Set, Adjust the ofset based on the whence
    *   Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 
    if(filep==NULL)
      return -EINVAL;

    if(whence==SEEK_SET){
      if((offset>FILE_SIZE) || (offset<0))
        return -EINVAL;
      filep->offp = offset;
    }
    else if(whence==SEEK_CUR)
    {
      if(((filep->offp + offset) >FILE_SIZE) || ((filep->offp+offset)<0))
        return -EINVAL;
      filep->offp = filep->offp + offset;
    }
    else if(whence==SEEK_END)
    {
      if(((filep->inode->file_size+offset)>FILE_SIZE) || ((filep->inode->file_size+offset)<0))
        return -EINVAL;
      filep->offp = filep->inode->file_size + offset;
    }
    else   //invalid arrgument
    {
      return -EINVAL;
    }
    ret_fd = filep->offp;
    return ret_fd;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{ 
  /**  TODO Implementation of file open, 
    *  You should be creating file(use the alloc_file function to creat file), 
    *  To create or Get inode use File system function calls, 
    *  Handle mode and flags 
    *  Validate file existence, Max File count is 32, Max Size is 4KB, etc
    *  Incase of Error return valid Error code 
    * */
   //mode refers to read, write and execute permissions.
    int ret_fd = -EINVAL;
    if(ctx==NULL || filename==NULL)
      return -EINVAL;

    struct inode *existing_inode = lookup_inode(filename);
    if(existing_inode == NULL){
      // return flags & O_CREAT;
      if(!(flags & O_CREAT)){          //O_CREAT flag not passed
        return -EINVAL;
      }
      struct inode *new_inode = create_inode(filename, mode);
      if(new_inode == NULL)
        return -ENOMEM;
      //if permission in flags are not subset of permission of inode

      struct file* file_des = alloc_file();
      if(file_des==NULL)
        return -ENOMEM;
      file_des->inode = new_inode;
      file_des->type = REGULAR;
      file_des->mode = flags;
      file_des->offp=0;
      
      file_des->fops->read = do_read_regular;
      file_des->fops->write = do_write_regular;
      file_des->fops->close = generic_close;
      file_des->fops->lseek = do_lseek_regular;
      file_des->ref_count = 1;

      ret_fd = -ENOMEM;            //error handling when all 32 fd are occupied
      int flag=0;
      for(int i=3;i<MAX_OPEN_FILES;i++)
      {
        if(ctx->files[i]==NULL)
        {
          
          ctx->files[i]=file_des;
          ret_fd=i;
          flag=1;
          break;
        }
      }
      if(flag==0)
      {
        generic_close(file_des);
        return -ENOMEM;
      }
    } 
    else             //file with same name already exist, here the mode arrgument is ignored.
    {

      //if inode(or file) exist but nothing other than O_CREATE in flags
      if(!(flags&(O_READ)|| (flags&O_WRITE)|| (flags&O_EXEC)))
        return -EINVAL;
      mode = existing_inode->mode;
      //if inode(or file) exist and permission in flag are not subset of inode 
        if((flags&O_READ)>(mode&O_READ) || (flags&O_WRITE)>(mode&O_WRITE) || (flags&O_EXEC)>(mode&O_EXEC))
          return -EACCES;
        struct file* file_des = alloc_file();
        if(file_des==NULL)
          return -ENOMEM;
        file_des->inode = existing_inode;
        file_des->type = REGULAR;
        file_des->mode = flags;              //Since new file is not made mode passed in arrgument is of no use
        file_des->offp=0;

        file_des->fops->read = do_read_regular;
        file_des->fops->write = do_write_regular;
        file_des->fops->close = generic_close;
        file_des->fops->lseek = do_lseek_regular;
        file_des->ref_count=1;

        ret_fd = -ENOMEM;    //error handling when all 32 fd are occupied
        int flag=0;
        for(int i=3;i<MAX_OPEN_FILES;i++)
        {
          if(ctx->files[i]==NULL)
          {

            ctx->files[i]=file_des;
            ret_fd=i;
            flag=1;
            break;
          }
        }
        if(flag==0){
          generic_close(file_des);
          return -ENOMEM;
        }
      
    }
    
    return ret_fd;
}

int fd_dup(struct exec_context *current, int oldfd)
{
     /** TODO Implementation of dup 
      *  Read the man page of dup and implement accordingly 
      *  return the file descriptor,
      *  Incase of Error return valid Error code 
      * */
    int ret_fd = -EINVAL; 
    //if oldfd is invalid
    if(current==NULL )
      return -EINVAL;
    if((oldfd<0)|| (oldfd>=MAX_OPEN_FILES) || current->files[oldfd]==NULL)
      return -EINVAL;
    ret_fd = -ENOMEM; //to handle when no descriptor is empty
    for(int i=0;i<MAX_OPEN_FILES;i++)
    {
      if(current->files[i]==NULL)
      {
        current->files[oldfd]->ref_count= current->files[oldfd]->ref_count + 1;
        current->files[i] = current->files[oldfd];
        ret_fd = i;
        break;
      }
    }
    return ret_fd;
}


int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
  /** TODO Implementation of the dup2 
    *  Read the man page of dup2 and implement accordingly 
    *  return the file descriptor,
    *  Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL;
    if(current==NULL)
      return -EINVAL;
    if((oldfd<0)|| (oldfd>=MAX_OPEN_FILES) ||(newfd<0) ||(newfd>=MAX_OPEN_FILES)||(newfd==oldfd)|| current->files[oldfd]==NULL)
      return -EINVAL;

    if(current->files[newfd]==NULL)
    {
      current->files[oldfd]->ref_count= current->files[oldfd]->ref_count + 1;
      current->files[newfd]=current->files[oldfd];
    }
    else
    {
      generic_close(current->files[newfd]);
      current->files[oldfd]->ref_count= current->files[oldfd]->ref_count + 1;
      current->files[newfd]=current->files[oldfd];
    }
    ret_fd = newfd;
    return ret_fd;
}
