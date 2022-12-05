#include "socket.h"

#define IPS "ipstr"
#define PORT "port"
#define LISMAX "lismax"


struct sock_info//定义一个结构体存放一下配置文件相关信息
{
    char ips[32];//存放ip地址
    short port;
    short lismax;
};

int read_conf(struct sock_info *p)//从配置文件中读取到存放在结构体中
{
    if(p==NULL)
    {
        return -1;
    }
    FILE* fp=fopen("my.conf","r");//打开配置文件
    if(fp==NULL)
    {
        printf("open myconf failed\n");
        return -1;
    }
    char buff[128]={0};
    int index=0;//打印告诉第几行出错
    while(fgets(buff,128,fp)!=NULL) //读取配置文件
    {//粗暴的去解析了一下
        ++index;
        if(strncmp(buff,"#",1)==0||strcmp(buff,"\n")==0)
        {
            continue;
        }
        buff[strlen(buff)-1]=0;
        if(strncmp(buff,IPS,strlen(IPS))==0)
        {
            strcpy(p->ips,buff+strlen(IPS)+1);//把等号后面的ip拷进去
        }   
        else if(strncmp(buff,PORT,strlen(PORT))==0)
        {
            p->port=atoi(buff+strlen(PORT)+1);
        }   
        else if(strncmp(buff,LISMAX,strlen(LISMAX))==0)
        {
            p->lismax=atoi(buff+strlen(LISMAX)+1);
        } 
        else
        {
            printf("无法解析第%d行的内容:%s",index,buff);
        }
    }
}

int socket_init()
{
   struct sock_info sk;//配置的信息
   if( read_conf(&sk)==-1)
   {
       return -1;//读取失败，返回-1
   }

   printf("ip:%s\n",sk.ips);
   printf("port:%d\n",sk.port);
   printf("lismax:%d\n",sk.lismax);
    
   //创建套接字，开始链接
   int sockfd=socket(AF_INET,SOCK_STREAM,0);
   if(sockfd==-1)
   {
       return -1;
   }
   struct sockaddr_in saddr;
   memset(&saddr,0,sizeof(saddr));
   saddr.sin_family=AF_INET;
   saddr.sin_port=htons(sk.port);
   saddr.sin_addr.s_addr=inet_addr(sk.ips);
    
   int res=bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
   if(res==-1){
       printf("bind err\n");
       return -1;
   }
   if(listen(sockfd,sk.lismax)==-1)
   {
       printf("listen err\n");
       return -1;
   }
   return sockfd;

}
