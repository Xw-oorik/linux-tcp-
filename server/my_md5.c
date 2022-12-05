#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <openssl/md5.h>
#include <fcntl.h>

#define  MD5_LEN  16 
#define  BUFF_SIZE 1024*16

void print_md5(unsigned char md[])
{
    int i = 0;
    for( ; i < MD5_LEN; i++ )
    {
        printf("%02x",md[i]);
    }

    printf("\n");
}
void fun_md5(int fd)
{
    MD5_CTX ctx;
    unsigned char md[MD5_LEN] = {0};
    MD5_Init(&ctx);

    unsigned long len = 0;
    char buff[ BUFF_SIZE ];
    while( (len = read(fd,buff,BUFF_SIZE )) > 0 )
    {
        MD5_Update(&ctx,buff,len);
    }

    MD5_Final(md,&ctx);

    print_md5(md);
}
//编译程序需要指定库   -lcrypto
// gcc -o my_md5 my_md5.c -lcrypto
// 
// 提示头文件openssl/md5.h 头文件没有 就需要安装如下的包
//sudo apt install openssl
//sudo apt install libssl - dev
int main(int argc, char* argv[])
{
    if ( argc == 1 )
    {
        return 0;
    }

    int i = 1;
    for( ;i < argc;i++)
    {
        int fd = open(argv[i],O_RDONLY);
        if ( fd == -1 )
        {
            printf("File:$%s not open\n",argv[i]);
            continue;
        }

        fun_md5(fd);

        close(fd);
    }
}

