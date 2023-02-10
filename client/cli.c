#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<sys/types.h>
#include<fcntl.h>
#include <signal.h>

int connect_ser()//连接
{
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd==-1)
    {
        printf("socket err\n");
        return -1;
    }
    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family=AF_INET;
    saddr.sin_port=htons(6600);
    saddr.sin_addr.s_addr=inet_addr("127.0.0.1");//回环地址
	//向服务器发起连接请求。
    
    int res=connect(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
    if(res==-1)
    {
        printf("connect ser failed\n");
        return -1;
    }

    return sockfd;
}
char *get_cmd(char buff[],char *myargv[])//解析
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
    return myargv[0];
}
void CliExeCommand(int c,char *cmd_buff)
{
           send(c,cmd_buff,strlen(cmd_buff),0);//让服务器端recv  打印recv buff=

            //接受结果
            char recv_buff[1024]={0};
            if(recv(c,recv_buff,1023,0)<=0)
            {
                printf("ser close\n");
                return ;
            }
            if(strncmp(recv_buff,"ok#",3)==0){
                printf("%s\n",recv_buff+3);
             }
             else{
                printf("命令执行错误\n");
             }
}
void recv_file(int c,char*filename,char* cmd_str)//接收下载的文件
{
    if(filename==NULL)
    {
        send(c,"err #no name",12,0);
        return ;
    }
    send(c,cmd_str,strlen(cmd_str),0);//发get a.c..
    //先尝试打开这个文件,客户端能打开说明本地有
    //本地有，判断和服务器的文件大小是不是一样
    //一样的说明本地之前下载完了
    //不一样 就开始断点续传,找到本地文件大小末尾，从这开始下载
    int local_fd=open(filename,O_WRONLY);
    if(local_fd!=-1)//本地有这个文件
    {
        char ff_buff[128]={0};
        if(recv(c,ff_buff,127,0)<=0)//接收服务器端文件大小
        {
            return;
        }  
        if(strncmp(ff_buff,"ok#",3)!=0)
        {
            printf("ser find file err!\n");
            return ;
        }
        int ff_size=atoi(ff_buff+3);//拿到服务器要发的文件大小
        if(ff_size==0)
        {
            send(c,"file size err",13,0);
            return ;
        }
        int local_size=lseek(local_fd,0,SEEK_END);//拿到客户端本地文件大小
        if(local_size==ff_size)
        {
            printf("本地存在该文件!\n");
            close(local_fd);
            send(c,"not need download",17,0);
            return;
        }
        if(local_size!=ff_size)//大小不相等了，开始断点续传
        {
            printf("local file size=%d\n",local_size);
            char local_buff[128]={0};
            sprintf(local_buff,"ok#size#%d",local_size);//给服务器端发本地文件的信息ok#size#...
            send(c,local_buff,strlen(local_buff),0);//给服务器发过去
            //开始接收剩下没下载完的
            char local_recv_buff[512]={0};
            int local_num=0;
            int local_curr_size=local_size;//从原来接收的末尾去从服务端接收剩下的
            while((local_num=recv(c,local_recv_buff,512,0))>0)
            {
                write(local_fd,local_recv_buff,local_num);//开始接收文件
                local_curr_size+=local_num;//更新末尾位置
                float local_f=local_curr_size*100.0/ff_size;
                printf("continue download:%.2f%%\r",local_f);
                fflush(stdout);
                if(local_curr_size>=ff_size)
                {
                    break;
                }
            }
            printf("断点续传over!\n");
        }
        close(local_fd);
        return ;
    }
    //本地没有了，正常下载
    char buff[128]={0};
    int num=recv(c,buff,127,0);//接收一下服务器找到文件了的信息
    if(num<=0)
    {
        printf("ser close\n");
        return ;
    }
    if(strncmp(buff,"ok#",3)!=0)
    {
        printf("find file err!\n");
        return;
    }
    //拿到文件大小,忽略粘包
    int filesizes=atoi(buff+3);
    printf("download filesizes=%d\n",filesizes);
    if(filesizes==0)//如果大小是0了就退出，前面本地已经创建了一个空文件
    {
        printf("file size is 0 create over!\n");
        send(c,"err",3,0);
        return ;
    }
    //下载
    int fd=open(filename,O_CREAT|O_WRONLY,0600);
    if(fd==-1)
    {
        send(c,"err",3,0);
        return ;
    }
    //如果不是空文件就开始下载
    send(c,"ok",2,0);//给服务器回复ok说我能下载了，服务器在那边接收比对一下，正确了就开始给客户端发文件
    //下面客户端开始接收文件
    char data[512]={0};
    int curr_size=0;//收到当前文件大小
    int nums=0;//一次多少
    while((nums=recv(c,data,512,0))>0)
    {
        write(fd,data,nums);//接收多少写多少
        send(c,"recv",4,0);//给服务器那边接收一下回复一下

        curr_size+=nums;

        float f=curr_size*100.0/filesizes;//百分比
        printf("download:%.2f%%\r",f);//从行头每次重新打印
        fflush(stdout);//刷屏
        if(curr_size>=filesizes)
        {
            break;
        }
    }
    printf("\ndownload over!\n");
    close(fd);
    return ;
}
void upload_file(int c,char *filename,char *cmd_str)//上传文件
{
    if(filename==NULL)
    {
        send(c,"err #no name",12,0);
        return ;
    }
    send(c,cmd_str,strlen(cmd_str),0);//把up ..发给服务器

    //先把文件的md5发给服务器
    char m_buff[128]={0};
    sprintf(m_buff,"./my.sh %s",filename);
    system(m_buff);//计算文件的md5写入md5.c文件里
    //
    int md5_fd=open("md5.txt",O_RDONLY|O_CREAT);
    if(md5_fd==-1)
    {
        printf("md5_fd err!\n");
        return;
    }
    char md5_buff[128]={0};
    read(md5_fd,md5_buff,128);//读取md5文件的内容
    char *s=strtok(md5_buff,"\n");//分割得到文件md5的值
  
    char buff[128]={0};
    close(md5_fd);
    system("rm md5.txt");//得到文件的md5之后就把md5的文件删除
   
    sprintf(buff,"ok#%s",s);
    send(c,buff,strlen(buff),0);//把ok#md5的值发给服务器
    //来接收一下服务器回复的信息:samefile
    char p[128]={0};
    if(recv(c,p,127,0)<=0)
    {
        return ;
    }
    if(strncmp(p,"#samefile#",10)==0)//如果服务器回的是samefile，说明他有这个文件
    {
        printf("文件已经存在!\n");
        printf("秒传成功!\n");
        return;
    }
    //不存在的话就正常上传
    int fd=open(filename,O_RDONLY);//当前路径下的文件
    if(fd==-1)
    {
        printf("open file err!\n");//文件打开失败或者不存在
        send(c,"cli open file err!",18,0);//给服务器那发个我这边打开失败了
        return ;
    }
     //开始获取文件大小
    int file_size=lseek(fd,0,SEEK_END);
    lseek(fd,0,SEEK_SET);
  
    char buff_status[128]={0};
    sprintf(buff_status,"ok#%d",file_size);//ok#文件大小
    printf("upload to server file size=%d\n",file_size);
    printf("respond to server:%s\n",buff_status);
    send(c,buff_status,strlen(buff_status),0);//告诉一下服务器ok#文件大小

    if(file_size==0)//如果是空文件，直接返回
    {
        printf("local filesize is 0 upload over!\n");
        return ;
    }

    //接收一下服务器回复的ok
    char recv_buff[128]={0};
    int res=recv(c,recv_buff,127,0);
    if(res<=0)
    {
        close(fd);
        return ;
    }
    if(strncmp(recv_buff,"ok",2)!=0)
    {
        close(fd);
        return ;
    }
    //如果接收到了服务器发的ok
    //比对成功了开始上传文件
    int num=0;
    int curr_size=0;
    char data[512]={0};
    while((num=read(fd,data,512))>0)
    {
        send(c,data,num,0);//上传
        curr_size+=num;
        float f=curr_size*100.0/file_size;//百分比
        printf("*send*:%.2f%%\r",f);//从行头每次重新打印
        fflush(stdout);//刷屏
    }
    printf("\nupfile over!\n");
    close(fd);
    return ;
}


int main()
{
   
    int c=connect_ser();
    if(c==-1)
    {
        printf("connect ser err!\n");
        exit(0);
    }
    while(1)
    {
        printf("connect~ >> ");
        fflush(stdout);
        char buff[128]={0};

        fgets(buff,128,stdin);//"get a.c"
        buff[strlen(buff)-1]=0;//ls  rm ..

        char cmd_buff[128]={0};//备份一份，后面解析之后就不用拼接了
        strcpy(cmd_buff,buff);

        char* myargv[10]={0};
        char * cmd=get_cmd(buff,myargv);//要发给服务器的字符串
        if(cmd==NULL)
        {
            continue;
        }
        if(strcmp(cmd,"exit")==0)
        {
            break;
        }
        else if(strcmp(cmd,"get")==0)
        {
            //下载
            recv_file(c,myargv[1],cmd_buff);
        }
        else if(strcmp(cmd,"up")==0)
        {
            //上传
            upload_file(c,myargv[1],cmd_buff);
        }
        else
        {
            //执行命令
            //发命令
            CliExeCommand( c,cmd_buff);
        } 
    }
    close(c);
}

