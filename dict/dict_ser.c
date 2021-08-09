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
#include<time.h>

#define ERR_LOG(errmsg) do{\
	perror(errmsg);\
	fprintf(stderr,"%s %s %d\n",__FILE__,__func__,__LINE__);\
}while(0)

#define N 128
#define IP "0.0.0.0"
#define PORT 8888


#define REG 'R'  //注册指令
#define LOGIN 'L' //登录指令
#define QUERY 'Q' //查询单词
#define HISTORY 'H' //查询历史
#define EXIT 'E' //退出

typedef void (*sighandler_t)(int);

typedef struct{
	char username[10];//保存用户名
	char password[N];//保存密码
	int state; //用户状态 0:未登录 1:已登录
}MSG;

//线程需要用到
typedef struct{         
	int newfd;   
	struct sockaddr_in cin;
	sqlite3 *dp;

}__msgInfo;


int up_sqlite3(sqlite3 *);
int up_user_sqlite3(sqlite3 *);
int ser(int sfd);
void *callBackHandler(void *arg);
int do_REG(int newfd,sqlite3 *);
int do_LOGIN(int newfd,sqlite3 *,char *name,int *st);
int do_up_login(int newfd,sqlite3 *,sqlite3 *,char *name,int *st,char *his);
int do_up_history(int newfd,sqlite3 *dp,sqlite3 *db,char *name,char *his);


void handler(int sig)//回收子进程资源`
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}
//---------------------------------------------主函数
int main(int argc, const char *argv[])
{

	char *errmsg=NULL;

	//注册信号处理函数
	sighandler_t s= signal(SIGCHLD, handler);
	if(s == SIG_ERR)
	{
		ERR_LOG("signal");
		return -1;
	}


	//初始化数据库
	sqlite3 *db=NULL;
	up_sqlite3(db);


	//创建用户库
	sqlite3 *dp=NULL;
	up_user_sqlite3(dp);



	//创建服务器
	int sfd=ser(sfd);

	//获取新的文件描述符
	struct sockaddr_in cin;
	socklen_t addrlen = sizeof(cin);

	while(1)
	{
		int newfd = accept(sfd, (struct sockaddr*)&cin, &addrlen);

		if(newfd < 0)
		{
			ERR_LOG("accept");
			return -1;
		}
		printf("[%s:%d]链接成功\n", inet_ntoa(cin.sin_addr), ntohs(cin.sin_port));


		//开线程
		__msgInfo mmm;
		mmm.newfd=newfd;

		pthread_t tid;
		if(pthread_create(&tid,NULL,callBackHandler,(void*)&mmm)!=0)
		{
			ERR_LOG("pthread_create");
			return -1;
		}



	}


	return 0;
}
//------------------------------------------------------------主函数结尾

//导入数据库
int up_sqlite3(sqlite3 *db)
{   
	//{{{
	//创建并打开数据库
	if(sqlite3_open("./dict.db",&db)!=0)
	{
		printf("sqlite3_open 失败\n");
		printf("sqlite3_open:%s\n",sqlite3_errmsg(db));
		return -1;
	}

	//创建表格
	char sql[128]="";
	char *errmsg;
	sprintf(sql,"create table if not exists dict(english char,chinese char)");
	if(sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=0)
	{
		printf("sqlite3_exec:__%d__ %s\n",__LINE__,errmsg);
		return -1;
	}
	//-----------------------------------
	printf("数据库开始导入......please wait...\n");

	//填充数据信息
	FILE *fd=fopen("./dict.txt","r");
	if(fd==NULL)
	{
		perror("fopen");
		return -1;
	}

	char buf[128]="";
	char *a;
	char english[128]="";
	char chinese[128]="";
	char *sqll;
	int i=1;
	while(1)
	{
		bzero(buf,sizeof(buf));
		bzero(english,sizeof(english));
		bzero(chinese,sizeof(chinese));

		if(NULL==fgets(buf,sizeof(buf),fd))
		{
			break;
		}

		a=strtok(buf," ");//拆分英文
		strcat(english,a);

		while(a!=NULL)
		{
			a=strtok(NULL," ");//拆分中文
			if(a==NULL)
			{
				break;
			}
			strcat(chinese,a);
		}

		//插入数据
		sqll=sqlite3_mprintf("insert into dict(english,chinese) values('%q','%q')",english,chinese);
		if(sqlite3_exec(db,sqll,NULL,NULL,&errmsg)!=0)
		{
			printf("sqlite3_exec:__%d__ %s\n",__LINE__,errmsg);
			return -1;
		}
		i++;
	}

	printf("数据库导入成功\n");
	fclose(fd);



	return 0;
	//}}}
}

//创建用户库
int up_user_sqlite3(sqlite3 *dp)
{
	//{{{
	if(sqlite3_open("./user.db",&dp)!=0)
	{
		printf("sqlite3_open 创建失败\n");
		printf("sqlite3_open:%s\n",sqlite3_errmsg(dp));
		return -1;
	}

	//创建用户名密码表
	char u[128]="";
	char *errmsg;
	sprintf(u,"create table if not exists user(name char primary key,password char,state int)");
	if(sqlite3_exec(dp,u,NULL,NULL,&errmsg)!=0)
	{
		printf("sqlite3_exec:__%d__ %s\n",__LINE__,errmsg);
		return -1;
	}
	printf("用户名密码表创建成功\n");

	//创建历史查询表
	char h[128]="";
	sprintf(h,"create table if not exists history(name char ,history char)");
	if(sqlite3_exec(dp,h,NULL,NULL,&errmsg)!=0)
	{
		printf("sqlite3_exec:__%d__ %s\n",__LINE__,errmsg);
		return -1;
	}
	printf("用户历史记录表创建成功\n");


	return 0;

	//}}}
}
//服务器ip和端口创建
int ser(int sfd)
{
	//{{{
	//socket
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

	//bind
	//填充信息
	struct sockaddr_in sin;

	sin.sin_family=AF_INET;
	sin.sin_port=htons(PORT);
	sin.sin_addr.s_addr=inet_addr(IP);

	if(bind(sfd,(void*)&sin,sizeof(sin))<0)
	{
		ERR_LOG("bind");
		return -1;
	}

	//监听
	if(listen(sfd,5)<0)
	{
		ERR_LOG("listen");
		return -1;
	}

	return sfd;
	//}}}
}

//线程
void *callBackHandler(void *arg)
{
	sqlite3 *dp=NULL;
	sqlite3_open("./user.db",&dp);
	sqlite3 *db=NULL;
	sqlite3_open("./dict.db",&db);
	__msgInfo mmm=(*(__msgInfo *)arg);
	int newfd=mmm.newfd;
	char buf[N]="";
	char name[N]="";
	char his[N]="";
	int st=0;
	while(1)
	{
		bzero(buf,sizeof(buf));
		int recv_len=recv(newfd,buf,N,0);
		if(recv_len<0)
		{
			ERR_LOG("recv");
			break;
		}
		else if(0==recv_len)
		{
			printf("对方关闭\n");
			break;
		}

		switch(buf[0])
		{
		case 'R':
			do_REG(newfd,dp);//注册
			break;
		case 'L':
			if(5==do_LOGIN(newfd,dp,name,&st))//登录
			{
				do_up_login(newfd,dp,db,name,&st,his);//登陆后的功能
			}
			break;
		case 'E':     
			goto END; //退出

			break;
		default:
			printf("输入错误\n");

		}

	}
END:
	close(newfd);
	pthread_exit(NULL);//退出
	return 0;

}

//注册
int do_REG(int newfd,sqlite3 *dp)
{  
	//{{{
	char sql[128]="";
	MSG msg;
	int exe;
	char *errmsg;
	memset(&msg,0,sizeof(msg));
	int recv_reg=recv(newfd,&msg,sizeof(msg),0);  //读取客户端的用户信息
	sprintf(sql,"insert into user values('%s','%s',%d)",msg.username,msg.password,msg.state);
	exe=sqlite3_exec(dp,sql,NULL,NULL,&errmsg);
	if(exe!=0)
	{
		printf("用户 %s 注册失败 (原因已反馈)\n",msg.username);
		char buf2[N]="注册无效(重复注册)";
		send(newfd,buf2,sizeof(buf2),0);
		return -1;
	}
	else
	{
		//反馈客户端
		char buf[N]="注册成功";
		send(newfd,buf,sizeof(buf),0);

		printf("用户 %s 的信息已插入信息表\n",msg.username);
	}


	return 0;
	//}}}
}


//登录
int do_LOGIN(int newfd,sqlite3 *dp,char *name,int *st)
{
	//{{{
	char *errmsg=NULL;
	char buf[N]="";
	int i,j;
	char sql[N]="";
	char sqll[N]="";
	char ssq[N]="";
	char buff[N]="该用户未注册，请注册后再来尝试登录";
	char aaa[N]="密码不正确";
	char **dpresult;
	int row,column;

	MSG msg;

	int recv_reg=recv(newfd,&msg,sizeof(msg),0);  //读取客户端的用户信息
	strcpy(name,msg.username);
	memcpy(&st,&(msg.state),4);
	sprintf(sql,"select *from user where name='%s'",msg.username);
	sqlite3_get_table(dp,sql,&dpresult,&row,&column,&errmsg);

	if(row==0)
	{
		printf("用户 %s 未注册(错误已反馈)\n",msg.username);
		send(newfd,buff,sizeof(buff),0);
	}
	else
	{
		//跟密码做比较
		int str;
		str=strcmp(msg.password,dpresult[4]);
		if(str!=0)
		{
			//密码错误
			printf("用户 %s 的密码错误(错误已反馈)\n",msg.username);
			send(newfd,aaa,sizeof(aaa),0);
		}
		else
		{	
			int ato;
			ato=atoi(dpresult[5]);
			//比较状态位
			if(ato==1)
			{   
				//重复登录
				printf("用户 %s 重复登录(错误已反馈)\n",msg.username);
				char q[N]="重复登录，请重新登录";
				send(newfd,q,sizeof(q),0);
			}

			else
			{
				//登录成功 
				msg.state=1;
				sprintf(ssq,"update user set state=%d where name='%s'",msg.state,msg.username);
				if(sqlite3_exec(dp,ssq,NULL,NULL,&errmsg)==0);
				{
					printf("登录状态修改成功\n");
				}
				char qq[N]="登录成功";
				send(newfd,qq,sizeof(qq),0);
				sqlite3_free_table(dpresult);//释放
				return 5;
			}

		}
	}
	return -1;
	//}}}
}


//登陆后的功能
int do_up_login(int newfd,sqlite3 *dp,sqlite3 *db,char *name,int *st,char *his)
{
	char buf[N]="";
	char ssq[N]="";
	char *errmsg=NULL;

	while(1)
	{
		bzero(buf,sizeof(buf));
		int recv_len=recv(newfd,buf,N,0);//读取客户端指令
		if(recv_len<0)
		{
			ERR_LOG("recv");
			break;
		}
		else if(0==recv_len)
		{
			printf("退出登录\n");
			break;
		}
		//------------------------------
		switch(buf[0])
		{
		case 'Q':
			//查单词
			do_query(newfd,dp,db,name);
			break;
		case 'H':
			//查记录
			do_up_history(newfd,dp,db,name,his);
			break;
		case 'E':
			//退出
			*st=0;
			sprintf(ssq,"update user set state=%d where name='%s'",*st,name);
			if(sqlite3_exec(dp,ssq,NULL,NULL,&errmsg)==0);
			{
				printf("退出状态修改成功\n");
			}
			return -1;
		}
	}
	return -1;
}

//查单词
int do_query(int newfd,sqlite3 *dp,sqlite3 *db,char *name)
{
	//{{{
	char english[N]="";
	bzero(english,sizeof(english));
	int recv_len=recv(newfd,english,N,0);//读取客户端英文单词

	char *errmsg=NULL;
	char sql[N]="";
	char buff[N]="抱歉，该单词未收录";
	char **dpresult;
	int row,column;
	sprintf(sql,"select *from dict where english='%s'",english);
	if(sqlite3_get_table(db,sql,&dpresult,&row,&column,&errmsg))//查找单词
	{
		printf("查询报错\n");
	}
	if(row==0)
	{
		printf("单词 %s 未找到(错误已反馈)\n",english);
		send(newfd,buff,sizeof(buff),0);
	}
	else
	{
		char buf[N]="";
		char sqll[N]="";
		time_t curtime;
		char sqq[N]="查询完毕";
		int i=1,j=0;

		for(i;i<row+1;i++)
		{
			for(j;j<column;j++)
			{
				strcat(buf,dpresult[i*column+j]);
				strcat(buf,"  ");
			}

			send(newfd,buf,sizeof(buf),0);
			strcat(buf,"  ");
			time(&curtime);
			strcat(buf,ctime(&curtime));
			printf("记录: 用户(%s)>>%s\n",name,buf);
			sprintf(sqll,"insert into history values('%s','%s')",name,buf);//将记录插入表
			if(sqlite3_exec(dp,sqll,NULL,NULL,&errmsg))
			{
				printf("插入出错\n");
			}

		}
		//查询完毕
		send(newfd,sqq,sizeof(sqq),0);

	}


	return -1;
	//}}}
}


//查记录
int do_up_history(int newfd,sqlite3 *dp,sqlite3 *db,char *name,char *his)
{
	char sql[N]="";
	char buf[N]="";
	char *errmsg=NULL;
	char buff[N]="抱歉，您还没有记录";
	char sqq[N]="查询完毕";
	char **dpresult;
	int row,column;
	int i=1,j=0;

	sprintf(sql,"select *from history where name='%s'",name);
	if(sqlite3_get_table(dp,sql,&dpresult,&row,&column,&errmsg))//查找记录
	{
		printf("查询报错\n");
	}
	if(row==0)
	{
		printf("未找到记录(错误已反馈)\n");
		send(newfd,buff,sizeof(buff),0);
	}
	for(i;i<row+1;i++)
	{
		for(j;j<column;j++)
		{
			strcat(buf,dpresult[i*column+j]);
			strcat(buf,"  ");
		}

		send(newfd,buf,sizeof(buf),0);
	}
	//记录查找完毕
	send(newfd,sqq,sizeof(sqq),0);



	return 0;
}
