#include<stdio.h>
#include <unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<dirent.h> 
#include<sys/stat.h>
#include<string.h>
#include <linux/limits.h>
long int find_size(char* path)                //recursive function to calculte the size of directory
{
    DIR *dir = opendir(path);
    struct dirent * de;
    if(dir == NULL)
    {
        //if the directory does not exist. Might never happen actually. For safety
        return 0;
    }
    else
    {
        long int size=0;                   //Total size
        while ((de = readdir(dir)) != NULL) 
        {
            if(de->d_type == DT_DIR && strcmp(de->d_name,".")!=0 && strcmp(de->d_name,"..")!=0)
            {

                int len  =strlen(path);
                char path1[PATH_MAX];                     //new variable for storing new path
                strcpy(path1,path);
                if(path1[len-1]!='/'){                 //edge case
                    strcat(strcat(path1,"/") ,de->d_name);
                    size += find_size(path1);             //cal to find size
                }
                else
                {
                    strcat(path1, de->d_name);
                    size += find_size(path1);
                }
            }
            else if(de->d_type != DT_DIR && strcmp(de->d_name,".")!=0 && strcmp(de->d_name,"..")!=0)
            {
                int len = strlen(path);
                char file_name[PATH_MAX];
                strcpy(file_name, path);
                if(file_name[len-1]!='/')
                    strcat(strcat(file_name,"/") ,de->d_name);
                else
                    strcat(file_name, de->d_name);
                struct stat st;
                if(stat(file_name, &st) != 0) {
                    return 0;
                }
                size += st.st_size;   
            }
        }
        return size;                  //Total size
    }
    
}



int main(int argc, char *argv[])
{
    char* path = argv[1];
    DIR *dir = opendir(path);
    struct dirent *de;
    if(dir == NULL){
        printf("Could not open directory or it does not exist %s\n", path);
        exit(0);
    }
    else
    {
        long int total_size=0;
        while ((de = readdir(dir)) != NULL) {
            if(de->d_type == DT_DIR && strcmp(de->d_name,".")!=0 && strcmp(de->d_name,"..")!=0)                 //if directory
            {
                int len  =strlen(path);
                char path1[PATH_MAX];
                strcpy(path1,path);
                if(path1[len-1]!='/')
                    strcat(strcat(path1,"/") ,de->d_name);
                else
                    strcat(path1, de->d_name);
                int fd[2];
                pipe(fd);
                int pid = fork();
                if(pid==0)
                {
                    dup2(fd[1],1);
                    printf("%ld\n",find_size(path1));                //push the calculated size to pipe
                    exit(0);
                }
                else
                {
                    wait(NULL);
                    close(fd[1]);
                    dup2(fd[0],0);
                    long int size=0;
                    scanf("%ld",&size);                            //get the calculated size from pipe read end
                    total_size+=size;
                    close(fd[0]);
                    printf("%s %ld\n", de->d_name, size);
                }
            }
            else if(de->d_type != DT_DIR && strcmp(de->d_name,".")!=0 && strcmp(de->d_name,".."))                     //if not directory
            {
                int len = strlen(path);
                char file_name[PATH_MAX];
                strcpy(file_name, path);
                if(file_name[len-1]!='/')                              //handling edge case
                    strcat(strcat(file_name,"/") ,de->d_name);
                else
                    strcat(file_name, de->d_name);
                struct stat st;
                if(stat(file_name, &st) != 0) {
                    return 0;
                }
                total_size += st.st_size;
            }
        }
        int len = strlen(path);
        int k=0;
        //for just printing the directory name
        for(int i=len-1;i>=0;i--)
        {
            if(path[i]=='/')
                path[i]='\0';
            else
            {
                break;
            }
        }
        len = strlen(path);
        for(int i=0;i<len;i++){
            if(path[i]=='/')
                k=0;
            else
            {
                path[k]=path[i];
                k++;
            }
        }
        path[k]='\0';
        printf("%s %ld\n", path,total_size);
    }
}