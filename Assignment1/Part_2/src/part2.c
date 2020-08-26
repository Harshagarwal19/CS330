#include<stdio.h>
#include <unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<string.h>
int main(int argc, char *argv[])
{
    if(argc<4)
    {
        printf("Input fromat is incorrect. Please check\n");
    }
    else
    {
        if(argv[1][0]=='@')                       //case of @
        {
            char* key = argv[2];                 //read key and path
            char* path = argv[3];
            
            int fd[2];
            if(pipe(fd) < 0){
                perror("pipe");
                exit(-1);
            }
            int pid = fork();                 
            if(pid==0)         //child
            {
                dup2(fd[1],1);                                      //dup with 1
                execlp("grep","grep","-rF",key, path, NULL);        //call for grep
                exit(0);
            }
            else              //parent
            {
                
                // wait(NULL);
                close(fd[1]);
                dup2(fd[0],0);                                      //dup with 0
                execlp("wc","wc","-l",NULL);                        //call for wc
            }
        }
        else if(argv[1][0]=='$')
        {
            char* key = argv[2];
            char* path = argv[3];
            char* new_file = argv[4];
            char* new_argv[argc-4];                             //new array for last command which can be of any length
            for(int i=5;i<argc;i++)
                new_argv[i-5] = argv[i];
            new_argv[argc-5] = NULL;
            int fd1[2];
            int fd2[2];
            if(pipe(fd1)<0 )
            {
                perror("pipe");
                exit(-1);
            }

            int pid = fork();
            if(pid==0)       //child
            {
                close(fd1[0]);
                dup2(fd1[1],1);                           //dup with 1
                
                execlp("grep","grep","-rF",key, path, NULL);
                exit(0);
            }
            else            //parent
            {
                // wait(NULL);
                close(fd1[1]);
                dup2(fd1[0],0);                           //dup with 0
                int r; 
                int curr=100;
                int prev = 0;
                int total_size=0;
                char* buff = malloc(sizeof(char)*100);         
                while(r = read(0, buff+total_size, 1 )){                   //reading file into a char array
                    total_size +=r;
                    if(total_size==curr-1)
                    {
                        buff = realloc(buff, sizeof(char)*2*curr);          //dynamically increasing size
                        // prev = curr;
                        curr = curr*2;
                    }
                    
                }
                buff[total_size] = '\0';
                int fd = open(new_file, O_WRONLY | O_CREAT| O_TRUNC, 00777);          //create a new file with given name. If already present erases its contents.
                write(fd, buff, total_size);                                          //writes in the file
                close(fd);

                if(pipe(fd2)<0 )                            //pipe2
                {
                    perror("pipe");
                    exit(-1);
                }
                int pid2 = fork();
                if(pid2==0){

                    dup2(fd2[1],1);                                         //dup with 1
                    close(fd2[0]);
                    write(1, buff, strlen(buff));                           //into the pipe
                    exit(0);
                }
                else{
                    // wait(NULL);
                    close(fd2[1]);
                    dup2(fd2[0],0);                                        //dup with 0
                    execvp(new_argv[0],new_argv);                           //executes the last command
                }
            }
        }
    }
    return 0;
}
