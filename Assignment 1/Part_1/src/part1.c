#include<stdio.h>
#include<dirent.h> 
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>
#include <linux/limits.h>
void check(char* buff, char* key, char* file_name, int flag)
{
    
    int len = strlen(buff);
    char* display = malloc(sizeof(char)*(len+1));                //to read line from buffer(or file)
    int k=0;
    for(int i=0;i<len;i++)
    {
        if(buff[i]!='\n')
        {
            display[k]=buff[i];
            k++;
        }
        else
        {
            display[k]='\0';
            k=0;
            if(strstr(display, key)!=NULL)                    //check condition if line contain key
            {
                if(flag==1)                                    //when the file was given instead of directory
                    printf("%s\n", display);
                else
                    printf("%s:%s\n",file_name, display);
            }
        }
    }
    //handling edge case
    if(k!=0){
        if(strstr(display, key)!=NULL)
        {
            if(flag==1)
                printf("%s\n", display);
            else
                printf("%s:%s\n",file_name, display);
        }
        free(display);
    }
    return;
}

void rec(char *path,char* key)                       //recursive function
{
    DIR *dir = opendir(path);
    struct dirent *de;
    if(dir == NULL){
        printf("Could not open directory or it does not exist %s\n", path);
    }
    else
    {
        while ((de = readdir(dir)) != NULL) 
        {
            if(de->d_type == DT_DIR && strcmp(de->d_name,".")!=0 && strcmp(de->d_name,"..")!=0)           //handling case of . and ..
            {

                int len  =strlen(path);
                char path1[PATH_MAX];
                strcpy(path1,path);
                if(path1[len-1]!='/')                                  //modifying path format given
                    rec(strcat(strcat(path1,"/") ,de->d_name), key);   //call the recursive function
                else                                                 
                {
                    rec(strcat(path1, de->d_name),key);                //call the recursive function
                }
            }
            if(de->d_type == DT_REG)                                   //In case of regular file
            {
                int len = strlen(path);
                char file_name[PATH_MAX];
                strcpy(file_name, path);
                if(file_name[len-1]!='/')                              //modifying path format given
                    strcat(strcat(file_name,"/") ,de->d_name);
                else
                {
                    strcat(file_name, de->d_name);
                }
                int fd = open(file_name, O_RDONLY);                    
                if (fd < 0)  
                { 
                    printf("Error cannot read file %s \n", file_name );
                } 
                else
                {
                    int total_length=0;
                    char* file_data = malloc(sizeof(char)*100);             
                    int r;
                    int curr=100;
                    int prev=0;
                    while(r = read(fd, file_data+total_length, 1))        //read file into a char array
                    {
                        total_length++;
                        if(total_length==curr-1){
                            file_data = (char*)realloc(file_data,sizeof(char)*2*curr);         //dynamically increase the size of char array
                            // prev= curr;
                            curr = 2*curr;
                        }
                        // total_length += r;
                    }
                    file_data[total_length]='\0';
                    check(file_data, key, file_name,0);                 //check for key in file
                }
                close(fd);
            }
        }
    }
    closedir(dir); 
    return ;
}

int main(int argc, char *argv[]) {
    if( argc != 3 )                                           //Check for invalid input
        printf("Invalid command\n");
    else
    {
        char* key = argv[1];
        char path[PATH_MAX];                                //path cannot have size grater than PATH_MAX
        int path_len = strlen(argv[2]);
        for(int i=0;i<path_len;i++)
        {
            path[i] = argv[2][i];
        }
        path[path_len]='\0';
        struct stat path_stat;                             //create a stat
        stat(path, &path_stat);                    
        if(S_ISREG(path_stat.st_mode))                      //checked for regular file
        {
            int fd = open(path, O_RDONLY);                   //opened the file to check
            if (fd < 0)                                      //error handling
            { 
                printf("Error cannot read file %s \n", path );
            } 
            else
            {
                int total_length=0;
                char* file_data = malloc(sizeof(char)*100);
                int r;
                int curr = 100;
                while(r = read(fd, file_data+total_length, 1))                      //read the file into a char array
                {
                    total_length++;
                    if(total_length == curr-1)
                    {
                        file_data = (char*)realloc(file_data,sizeof(char)*2*curr);            //dynamically increase the size of char array
                        curr = 2*curr;
                    }
                    
                }
                file_data[total_length]='\0';           
                check(file_data, key, path,1);                      //check for key in the file
            }
            close(fd);
        }
        else                            //Case when given path is of directory
        {
            rec(path, key);             //calls the recursive function
        }
    }
}