#include<stdio.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<fcntl.h>
#include<sys/types.h>
#include<string.h>
#include<errno.h>
#include<dirent.h>
#include<stdlib.h>
#include<sqlite3.h>
#include<arpa/inet.h>
#include<signal.h>
#include<pthread.h>

#define ERR_LOG(errmsg) do{\
	perror(errmsg);\
	fprintf(stderr,"%s %s %d\n",__FILE__,__func__,__LINE__);\
}while(0)

#define N 128
#define IP "0.0.0.0"
#define PORT 8888

#define REG 'R'//注册指令
#define LOGIN 'L' //登录指令
#define QUERY 'Q' //查询单词
#define HISTORY 'H' //查询历史
#define EXIT 'E' //退出

typedef struct{
	char username[10];//保存用户名
	char password[N];//保存密码
	int state; //用户状态，0:未登录 1:已登录
}MSG;


//线程用到的
typedef struct{
	int newfd;
	struct sockaddr_in cin;
	sqlite3 *dp;
}__msgInfo;



int up_ser(int sfd);
int do_REG(int sfd);
int do_LOGIN(int sfd);
int do_up_login(int sfd);
int do_up_history(int sfd);
//-----------------------------------------------主函数
int main(int argc, const char *argv[])
{
	char *errmsg=NULL;

	//连接服务器
	int sfd=up_ser(sfd);

	int choose=0;
	while(1)
	{
		system("clear");
		choose = 0;
		printf("-----------------\n");
		printf("---- 1.注册 -----\n");
		printf("---- 2.登录 -----\n");
		printf("---- 3.关闭 -----\n");
		printf("-----------------\n");

		printf("请输入>>>");
		scanf("%d",&choose);
		while(getchar()!=10);
		switch(choose)
		{  
		case 1:
			do_REG(sfd);//注册
			break;
		case 2:
			if(5==do_LOGIN(sfd))//登录
			{
				do_up_login(sfd);//登录后的功能
			}

			break;
		case 3:
			goto END;//退出
			break;

		default:
			printf("输入错误\n");
		}


		printf("按任意键清屏>>>");
		while(getchar()!=10);

	}
END:
	close(sfd);
	return 0;
}
//--------------------------------------------主函数结尾

//连接服务器
int up_ser(int sfd)
{
	//{{{
	//SOCKET
	sfd=socket(AF_INET,SOCK_STREAM,0);
	if(sfd<0)
	{
		ERR_LOG("socket");
		return -1;
	}

	//端口快速重用
	int reuse=1;
	if(setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(int))<0)
	{
		ERR_LOG("setsockopt");
		return -1;
	}

	//填充服务器
	struct sockaddr_in sin;
	sin.sin_family=AF_INET;
	sin.sin_port=htons(PORT);
	sin.sin_addr.s_addr=inet_addr(IP);

	if(connect(sfd,(struct sockaddr *)&sin,sizeof(sin))<0)
	{
		ERR_LOG("connect");
		return -1;
	}

	printf("连接服务器成功\n");




	return sfd;
	//}}}
}

//注册
int do_REG(int sfd)
{	
	//{{{
	char buf[N]="";
	int res;
	MSG msg;
	char buff[N]="";

	bzero(buf,sizeof(buf));
	buf[0]=REG;
	if(send(sfd,buf,sizeof(buf),0)<0) //发送协议
	{
		ERR_LOG("send");
		return -1;
	}

	printf("输入用户名>>");
	scanf("%s",msg.username);
	while(getchar()!=10);


	printf("输入用户名密码>>");
	scanf("%s",msg.password);
	while(getchar()!=10);

	msg.state=0;
	res=send(sfd,&msg,sizeof(msg),0);
	if(res<0)
	{
		ERR_LOG("recv");
		return -1;
	}

	//读取服务器的反馈
	recv(sfd,buff,sizeof(buff),0);
	printf("%s\n",buff);

	return 0;
	//}}}
}


//登录
int do_LOGIN(int sfd)
{
	//{{{
	char buf[N]="";
	bzero(buf,sizeof(buf));
	buf[0]=LOGIN;      
	if(send(sfd,buf,sizeof(buf),0)<0)   //发送协议
	{
		ERR_LOG("send");
		return -1;
	}

	//------------------------------------------
	MSG msg;

	printf("输入用户名>>");
	scanf("%s",msg.username);
	while(getchar()!=10);


	printf("输入用户名密码>>");
	scanf("%s",msg.password);
	while(getchar()!=10);

	int res;
	res=send(sfd,&msg,sizeof(msg),0);//发送信息给服务器
	if(res<0)
	{
		ERR_LOG("recv");
		return -1;
	}

	char buff[N]="";
	recv(sfd,buff,sizeof(buff),0);//读取服务器反馈
	printf("%s\n",buff);
	if(strcmp(buff,"登录成功")==0)
	{
		return 5;
	}



	return 0;
	//}}}
}

//登陆后的功能
int do_up_login(int sfd)
{
	int choose=0;
	char buf[N]="";
	while(1)
	{
		system("clear");
		choose=0;

		printf("---------------------\n");
		printf("---- 1.查询单词 -----\n");
		printf("---- 2.查询历史记录 -\n");
		printf("---- 3.退出登录 -----\n");
		printf("---------------------\n");

		printf("请输入>>>");
		scanf("%d",&choose);
		while(getchar()!=10);


		switch(choose)
		{
		case 1:
			//查单词
			do_query(sfd);
			break;
		case 2:
			//查历史记录
			do_up_history(sfd);

			break;
		case 3:
			//退出

			bzero(buf,sizeof(buf));
			buf[0]=EXIT;      
			if(send(sfd,buf,sizeof(buf),0)<0)   //发送协议
			{
				ERR_LOG("send");
				return -1;
			}
			printf("退出登录成功\n");
			return -1;
		default :
			printf("输入错误\n");


		}

		printf("按任意键清屏>>>");
		while(getchar()!=10);

	}

	return -1;
}

//查单词
int do_query(int sfd)
{
	//{{{
	char buf[N]="";
	bzero(buf,sizeof(buf));
	buf[0]=QUERY;      
	if(send(sfd,buf,sizeof(buf),0)<0)   //发送协议
	{
		ERR_LOG("send");
		return -1;
	}
	//-----------------------------------------------

	//输入要查询的单词
	printf("输入要查询的单词>>");
	char english[N]="";
	bzero(english,sizeof(english));
	fgets(english,N,stdin);
	english[strlen(english)-1]=0;

	if(send(sfd,english,sizeof(english),0)<0)
	{
		ERR_LOG("send");
		return -1;
	}


	char chinese[N]="";
	int i;
	int j;
	while(1)
	{
		bzero(chinese,sizeof(chinese));
		recv(sfd,chinese,N,0); //接受反馈
		i=strcmp(chinese,"查询完毕");
		if(i==0)
		{
			printf("查询完毕\n");
			break;
		}
		else 
		{
			j=strcmp(chinese,"抱歉，该单词未收录");
			if(j==0)
			{
				printf("抱歉，该单词未收录\n");
				break;
			}
			else
				printf("%s\n",chinese);

		}
	}
	return -1;
	//}}}
}

//查记录
int do_up_history(int sfd)
{

	char buf[N]="";
	bzero(buf,sizeof(buf));
	buf[0]=HISTORY;      
	if(send(sfd,buf,sizeof(buf),0)<0)   //发送协议
	{
		ERR_LOG("send");
		return -1;
	}
	//-----------------------------------------------
	char his[N]="";
	int i;
	int j;
	while(1)
	{
		bzero(his,sizeof(his));
		recv(sfd,his,N,0); //接受反馈
		i=strcmp(his,"查询完毕");
		if(i==0)
		{
			printf("查询完毕\n");
			break;
		}
		else 
		{
			j=strcmp(his,"抱歉，您还没有记录");
			if(j==0)
			{
				printf("抱歉，没有记录\n");
				break;
			}
			else
				printf("%s\n",his);
		}
	}
	return 0;
}
