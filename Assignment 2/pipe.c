#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
/***********************************************************************
 * Use this function to allocate pipe info && Don't Modify below function
 ***********************************************************************/
struct pipe_info* alloc_pipe_info()
{
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);
    pipe ->pipe_buff = buffer;
    return pipe;
}


void free_pipe_info(struct pipe_info *p_info)
{
    if(p_info)
    {
        os_page_free(OS_DS_REG ,p_info->pipe_buff);
        os_page_free(OS_DS_REG ,p_info);
    }
}
/*************************************************************************/
/*************************************************************************/



int pipe_read(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Read the contect from buff (pipe_info -> pipe_buff) and write to the buff(argument 2);
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL;
    if((filep==NULL) || (filep->mode != O_READ))
        return -EINVAL;
    if(filep->pipe->is_ropen==1){
        int read_posi = filep->pipe->read_pos;
        int write_posi = filep->pipe->write_pos; 
        int max_read;
        if(write_posi - read_posi == PIPE_MAX_SIZE)
            max_read = PIPE_MAX_SIZE;
        else if(write_posi == read_posi)
            max_read=0;
        else if(write_posi-read_posi < PIPE_MAX_SIZE)
        {
            max_read = write_posi - read_posi;
        }
        if(max_read<count)
            return -EINVAL;
        
        int actual_count=0;
        for(int i=0;i<count;i++)
        {
            buff[i] = filep->pipe->pipe_buff[read_posi%PIPE_MAX_SIZE];

            read_posi +=1;
        }
        filep->pipe->read_pos = read_posi;
        // buff[count]='\0';
        ret_fd = count;
    }
    return ret_fd;
}


int pipe_write(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the contect from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL; 
    if((filep==NULL) || (filep->mode != O_WRITE))
        return -EINVAL;
    if(filep->pipe->is_wopen==1){
        int write_posi = filep->pipe->write_pos;
        int read_posi = filep->pipe->read_pos;
        int max_write;
        if(write_posi - read_posi == PIPE_MAX_SIZE)
            max_write=0;
        else if(read_posi == write_posi)
            max_write = PIPE_MAX_SIZE;
        else if(write_posi-read_posi < PIPE_MAX_SIZE)
            max_write = PIPE_MAX_SIZE - (write_posi-read_posi);
        
        if(max_write < count)
            return -EINVAL;
        for(int i=0;i<count;i++)
        {
            filep->pipe->pipe_buff[write_posi%PIPE_MAX_SIZE] = buff[i];
            write_posi = write_posi+1;
        }
        filep->pipe->write_pos = write_posi;
        ret_fd = count;
    }
    return ret_fd;

}

int create_pipe(struct exec_context *current, int *fd)
{
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL; 
    struct file *read_end_pipe = alloc_file();
    if(read_end_pipe==NULL)
        return -ENOMEM;
    read_end_pipe->type = PIPE;
    read_end_pipe->fops->read = pipe_read;
    read_end_pipe->fops->write = pipe_write;
    read_end_pipe->fops->close = generic_close;
    read_end_pipe->inode = NULL;
    read_end_pipe->offp=0;
    read_end_pipe->mode = O_READ;
    read_end_pipe->ref_count = 1;
    struct file *write_end_pipe = alloc_file();
    if(write_end_pipe==NULL)
    {
        generic_close(read_end_pipe);
        return -ENOMEM;
    }
    write_end_pipe->type = PIPE;
    write_end_pipe->fops->read = pipe_read;
    write_end_pipe->fops->write = pipe_write;
    write_end_pipe->fops->close = generic_close;
    write_end_pipe->inode = NULL;
    write_end_pipe->offp=0;
    write_end_pipe->mode = O_WRITE;
    write_end_pipe->ref_count = 1;

    struct pipe_info *pipe_info = alloc_pipe_info(); 
    if(pipe_info==NULL)
    {
        generic_close(read_end_pipe);
        generic_close(write_end_pipe);
        return -ENOMEM;
    }
    pipe_info->read_pos=0;
    pipe_info->write_pos=0;
    pipe_info->buffer_offset=0;
    pipe_info->is_ropen=1;
    pipe_info->is_wopen=1;
    read_end_pipe->pipe = pipe_info;
    write_end_pipe->pipe = pipe_info;

    int flag=0;
    for(int i=3;i<32;i++)
    {
        if(current->files[i]==NULL)
        {
            if(flag==0){
                fd[0]=i;
                current->files[i] = read_end_pipe;
                flag=1; 
            }
            else if(flag==1){
                fd[1] = i;
                current->files[i] = write_end_pipe;
                flag=2;
                break;
            }
        }
    }
    if(flag==2)
        ret_fd = fd[0];
    else if(flag==1)
    {
        current->files[fd[0]]=NULL;
        generic_close(read_end_pipe);
        generic_close(write_end_pipe);
        free_pipe_info(pipe_info);
        return -ENOMEM;
    }
    else{
        generic_close(read_end_pipe);
        generic_close(write_end_pipe);
        free_pipe_info(pipe_info);
        return -ENOMEM;
    }
    return ret_fd;
}

