#include "socket.h"
#include "work_thread.h"
#include <signal.h>
void handle_signal(int sig)
{//捕捉一下信号
    ;
} 

int main()
{
    //用来处理SIGPIPE信号，下载下一部分，客户端中断连接
    
   // signal(SIGPIPE,SIG_IGN);//忽略他
    signal(SIGPIPE,handle_signal);//或者捕捉他
    int sockfd=socket_init();
    if(sockfd==-1)
    {
        exit(0);
    }
    //接收客户端链接
    struct sockaddr_in caddr;
    int len=sizeof(caddr);
    while(1)
    {
        int c=accept(sockfd,(struct sockaddr*)&caddr,&len);
        if(c<0)
        {
            continue;
        }
         printf("accept c=%d\n客户端ip:（%s）已连接;客户端端口port:(%d)已连接\n",c,inet_ntoa(caddr.sin_addr),ntohs(caddr.sin_port)); 
        
        int res=start_thread(c);//启动线程
        if(res==-1)
        {
            close(c);
        }
    }
}
