#include "work_thread.h"
#include <sys/wait.h>
#define ARGC_MAX 10

#define ERR "err"
#define ERR_PIPE "pipe err"
#define ERR_FORK "fork err"

struct ArgNode//线程里转换返回的套接子类型用，免得强转报警告
{
    int c;
};
//封装了几个回复错误信息的函数
void send_err(int c)
{
    send(c,ERR,strlen(ERR),0);
}
void send_errpipe(int c)
{
    send(c,ERR_PIPE,strlen(ERR_PIPE),0);
}
void  send_errfork(int c)
{
    send(c,ERR_FORK,strlen(ERR_FORK),0);
}

void Send_file(int c,char *filename)//给客户端发送
{
    if(filename==NULL)
    {
        send(c,"err#no name",11,0);
        return ;
    }
    //客户端建一个文件夹,把每次下的文件拼接进去，一般写到配置文件里
    //这里能下载的文件就在服务端当前目录下，也可以改路径
    //先尝试一下能不能打开需要给客户端传的文件
    int fd=open(filename,O_RDONLY);
    if(fd==-1)
    {
        printf("do not has this file!\n");
        send_err(c);
        return ;
    }

    //获取文件大小
    int file_size=lseek(fd,0,SEEK_END);//距离末尾0个字节
    lseek(fd,0,SEEK_SET);//回到文件头
    printf("send to cli file_size= %d\n",file_size);

    //有文件存在,回复成功报文ok#..size
    char buff_status[128]={0};
    sprintf(buff_status,"ok#%d",file_size);//拼字符串
    
    printf("respond to cli:%s\n",buff_status);//打印一下回复的报文信息
    printf("*************\n");
    //发送文件大小给客户端去比较
    send(c,buff_status,strlen(buff_status),0);//ok#size

    //接收一下客户端回复文件大小，他能开始接收了他就回复ok
    char cli_status[128] = {0};
    int res=recv(c,cli_status,127,0);
    if(res<=0)
    {
        close(fd);
        return;
    }
    //比对一下状态
    if(strncmp(cli_status,"ok",2)!=0)
    {
        close(fd);
        return ;
    }
    //判断要不要断点续传
    if(strncmp(cli_status+3,"size#",4)==0)//是断点续传
    {
        int d_size=atoi(cli_status+8);//从ok#size#后面开始读客户端文件大小
        if(d_size<0)
        {
            send(c,"cli file size err!",18,0);
            return;
        }
        lseek(fd,d_size,SEEK_SET); //将文件光标移到客户端文件大小的地方,进行传输
        int d_num=0;
        int d_curr_size=0;
        char d_sendbuff[512]={0};
        while((d_num=read(fd,d_sendbuff,512))>0)
        {
            send(c,d_sendbuff,d_num,0);//从客户端文件大小的地方发文件
            d_curr_size+=d_num;
            float d_f=d_curr_size*100.0/(file_size-d_size);
            printf("断点send:%.2f%%\r",d_f);
            fflush(stdout);
        }
        printf("\n断点续传over!\n");
        printf("-------------\n");
        close(fd);
        return ;
    }
    //比对成功开始发文件
    //发文件的数据部分
    int num=0;//表示每次读了多少数据
    int curr_size=0;//当前一共发的，也写成百分比形式和客户端保持一致 看起来
    char data[512]={0};
    while((num=read(fd,data,512))>0)
    {
        send(c,data,num,0);//读多少，发多少
        char r_buf[128]={0};//发一次，客户端收一次，给我服务器回复一次,服务器接收一次,不过这样效率会差，但保证了客户端终止接收，服务器还不知道继续发错误信息的问题
        if(recv(c,r_buf,127,0)<=0)
        {
            close(fd);
            return ;
        }
        curr_size+=num;
        float f=curr_size*100.0/file_size;//百分比
        printf("send:%.2f%%\r",f);//从行头每次重新打印
        fflush(stdout);//刷屏
        
    }
    close(fd);
    printf("\nsend over!\n");
    printf("------------\n");
    return;
}
void Recv_file(int c,char *filename)//服务端接收文件
{
    //进到这函数之前上面就已经成功recv了客户端消息
    if(filename==NULL)
    {
        return ;
    }
    char recv_buff[128]={0};//收客户端发来的ok#md5
    if(recv(c,recv_buff,127,0)<=0)
    {
        printf("recv err!\n");
        return ;
    }
    if(strncmp(recv_buff,"ok#",3)!=0)
    {
        return ;
    }
    char *s=recv_buff;
    s+=3;//从客户端的md5的值开始
    int md5_fd=open("md5.txt",O_RDONLY,0600);
    char md5_buff[128]={0};
    read(md5_fd,md5_buff,128);
    char *ss=strtok(md5_buff,"\n");//得到服务器md5的值
    while(ss!=NULL)
    {
        if(strcmp(s,ss)==0)//遍历文件的md5.c文件查找
        {
            send(c,"#samefile#",10,0);//告知客户端是相同的文件
            printf("秒传成功了!\n");
            return ;
        }
        ss=strtok(NULL,"\n");
    }
    //不然的话告诉客户端可以上传了
    send(c,"#client can send#",17,0);
    //接收一下客户端回复的ok#文件大小
     char buff[128] = {0};
    if (recv(c,buff,127,0) <= 0 )
    {
        return;
    }

    if ( strncmp(buff,"ok#",3) != 0 )//ok#文件size
    {
        printf("Err!\n");
        return;
    }
    int file_size=atoi(buff+3);//输出一下文件大小
    printf("recv filesizes=%d\n",file_size);
    
    //下载到本地，如果是空文件直接就本地创建
    int fd=open(filename,O_WRONLY|O_CREAT,0600);
    if(fd==-1)
    {
        send(c,"open err!",9,0);
        return ;
    }
    if(file_size==0){
        printf("\nrecv filesize==0 from cli over!\n");
        printf("------------\n");
        close(fd);
        char md5[128]={0};//文件上传完毕之后把文件的md5值存进md5.c文件中
        sprintf(md5,"./my.sh %s",filename);
        system(md5);
        return;
    }

    //如果size不是0，开始接收
    send(c,"ok",2,0);//给客户端回ok表示自己可以下载了
    char data[512]={0};
    int nums=0;//每次接收的大小
    int curr_size=0;//当前接收的文件大小
    while((nums=recv(c,data,512,0))>0)
    {
        write(fd,data,nums);
        curr_size+=nums;
        float f=curr_size*100.0/file_size;
        printf("recv from cli:%.2f%%\r",f);
        fflush(stdout);
        if(curr_size>=file_size)
        {
            break;
        }
    }
    printf("\nrecv from cli over!\n");
    printf("------------\n");
    close(fd);
    char md5[128]={0};//文件上传完毕之后把文件的md5值存进md5.c文件中
    sprintf(md5,"./my.sh %s",filename);
    system(md5);
    return;
}

void ExeCommd(int c,char * cmd,char * myargv[])//命令对文件操作
{                                             //调系统命令
     int pipefd[2];
     if(pipe(pipefd)==-1)
     {
        //服务端自己的错误
         send_errpipe(c);
         return ;
     }
     pid_t pid=fork();
     if(pid==-1)
     {
        send_errfork(c);
        return ; 
      }
     if(pid==0)
     {
        close(pipefd[0]);//子进程关读端
        //用管道写端覆盖标准输出和标准错误输出
        dup2(pipefd[1],1);//标准输出就是往屏幕上打，这下就不向屏幕打，而是给管道打
        dup2(pipefd[1],2);
        execvp(cmd,myargv);
        perror("exec err");
        exit(0);
     }
     close(pipefd[1]);//父进程关写端
     wait(NULL);
     char send_buff[1024]={"ok#"};
     read(pipefd[0],send_buff+3,1020);//读出管道中数据
     send(c,send_buff,strlen(send_buff),0);//把管道中数据发给客户端
     
}
char * get_cmd(char buff[],char *myargv[])//解析命令
{
    if(buff==NULL||myargv==NULL){
        return NULL;
    }
    //分割
    char *ptr=NULL;
    char *s=strtok_r(buff," ",&ptr);//线程安全的版本strtok_r
    int i=0;
    while(s!=NULL)
    {
        myargv[i++]=s;
        s=strtok_r(NULL," ",&ptr);//沿着原来位置继续分割
    }
    return myargv[0];//比如 get a.c  返回的是get 
}
void *work_fun(void *arg)//工作线程
{
    struct ArgNode *p=(struct ArgNode*)arg;//最上面结构体用上了,免得(int)arg警告
    if(p==NULL)
    {
        return NULL;
    }
    int c=p->c;
    free(p);

    while(1)
    {
        char buff[128]={0};
        int num=recv(c,buff,127,0);//收到命令数据 "ls""get.." "up .." "ls -l" "rm a.c "...
        printf("recv buff=%s\n",buff);
        if(num<=0)
        {
            printf("one client over\n");
            break;
        }
        //解析命令
        char *myargv[ARGC_MAX]={0};
        //把buff里面的命令去解析，把他分成各个字段放进myargv
        char * cmd=get_cmd(buff,myargv);
        if(cmd==NULL)
        {
            send_err(c);
            continue;
        }
        else if(strcmp(cmd,"get")==0) //发送客户端要下载的文件
        {
            Send_file( c,myargv[1]);
        }
        else if(strcmp(cmd,"up")==0)//接收上传的文件
        {
            Recv_file(c,myargv[1]);//此时已经都收到客户端发来的命令了
        }
        else   //执行命令 ls   rm  ps-f ..
        {
            //fork + exec
            ExeCommd(c,cmd,myargv);
        }
    }
    //free(p);//如果里面还有其他成员 就要在这释放，因为上面可能还需要用这个结构体
    printf("close pthread\n");
   // close(c);
}
int start_thread(int c)//启动线程
{
  
   pthread_t id;
   struct ArgNode *p=(struct ArgNode*)malloc(sizeof(struct ArgNode));//去堆上找
   if(p==NULL)
   {
       return -1;
   }
   p->c=c;

   if( pthread_create(&id,NULL,work_fun,(void*)p)!=0)
   {
       close(c);
       printf("启动线程失败\n");
       return -1 ;
   }
   return 0;
}
