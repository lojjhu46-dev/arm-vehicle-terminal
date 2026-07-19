#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/input.h>
#include <linux/videodev2.h>

#define TOUCH_PATH "/dev/input/event0"
#define LCD_PATH "/dev/fb0"
int main()
{
    int pipe_fd[2];
    pipe(pipe_fd);
    pid_t pid = fork();
    if(pid!=0)
    {
        system("./showlogo");
        char buff[256];
        unsigned int x,y;
        while (1)
        {
            memset(buff,0,sizeof(buff));
            read(pipe_fd[0],buff,sizeof(buff));
            char *p=strtok(buff,",");
            x=atoi(p);
            char *p1=strtok(NULL,",");
            y=atoi(p1);
            printf("x=%d,y=%d\n",x,y);
            fflush(stdout);
            if (x>450&&y>340)
            {
                printf("touch\r\n");
            }
        }
        
    }
    else if (pid==0)
    {
        int touch_fd=open(TOUCH_PATH,O_RDWR);
            if(touch_fd==-1) 
            {
                perror("open touch_fd error");
                return -1;
            }
        
        unsigned int touch_x=0;
        unsigned int touch_y=0;
        while (1)
        {
            struct input_event touchsrceen_event;
            char writebuff[256];


            memset(&touchsrceen_event,0,sizeof(struct input_event));
            {
                read(touch_fd,&touchsrceen_event,sizeof(struct input_event));
                if (touchsrceen_event.type==EV_ABS)
                {
                    if (touchsrceen_event.code==ABS_X)
                    {
                        touch_x=touchsrceen_event.value*800/1024;
                    }
                    else if (touchsrceen_event.code==ABS_Y)
                    {
                        touch_y=touchsrceen_event.value*480/600;
                    }
                }
                memset(writebuff,0,sizeof(writebuff));
                sprintf(writebuff,"%d,%d",touch_x,touch_y);
                write(pipe_fd[1],writebuff,sizeof(writebuff));
            }
        }
        exit(1);
    }
    else
    fprintf(stderr,"fork error:%s",strerror(errno));
    exit(1);
}