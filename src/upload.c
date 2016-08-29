#ifndef lint
static const char rcsid[] = "$Id: echo.c,v 1.5 1999/07/28 00:29:37 roberts Exp $";
#endif /* not lint */

#include "fcgi_config.h"

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <process.h>
#else
extern char **environ;
#endif

#include "fcgi_stdio.h"
#include "make_log.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "redis_op.h"

#define MADULENAME "fastCGI"
#define PROCNAME   "upload"
#define CONFCLIENTPATH	"/etc/fdfs/client.conf"
#define SOURCES_FILE_PATH "/home/itcast/DistributedStorageDemo"
#define FILE_ID_LEN 1024


//#define REDIS_IP	"101.200.170.178"
#define REDIS_IP	"127.0.0.1"
#define REDIS_PORT	"6379"

#define FILE_INFO_TABLE_KEY "FILE_INFO_LIST"
#define ZSET_KEY 			"FILE_HOT_ZSET"


//信号回收
void do_sig_child(int signo)
{
    pid_t pid;
    while ((pid = waitpid(0, NULL, WNOHANG)) > 0) ;
}

//解析字符流 copy到buf中
int getbuf(char* buf,int len)
{
    int i ,ch;
    char* temp = NULL;

    temp = buf;
    for (i = 0; i < len; i++) {
        if ((ch = getchar()) < 0) {
            LOG(MADULENAME , PROCNAME,"getbuf:Not enough bytes received on standard"); 
            break;
        }

        *temp = ch;
        temp++;
    }

    return 0;
}

//解析buf
char* memstr(char* full_data, int full_data_len, char* substr) 
{ 
    if (full_data == NULL || full_data_len <= 0 || substr == NULL) { 
        return NULL; 
    } 

    if (*substr == '\0') { 
        return NULL; 
    } 

    int sublen = strlen(substr); 

    int i; 
    char* cur = full_data; 
    int last_possible = full_data_len - sublen + 1; 
    for (i = 0; i < last_possible; i++) { 
        if (*cur == *substr) { 
            //assert(full_data_len - i >= sublen);  
            if (memcmp(cur, substr, sublen) == 0) { 
                //found  
                return cur; 
            } 
        } 
        cur++; 
    } 

    return NULL; 
} 

//得到文件名
int  getfilename(char* buf, int len ,char* filename)
{
	char* p = buf;
	char* pStart = NULL;
	char* prefilename="filename=";
	int ret =0;
	
	if(len == 0 || buf ==NULL )
	{
		ret =-1;
		LOG(MADULENAME , PROCNAME,"getfilename:NULL %d\n",ret); 
		return ret;
	}
		
	
	p = memstr(buf, len , prefilename);
	pStart = p+ strlen(prefilename);
	pStart ++;
	p = pStart;
	
	p = memstr(p, len , "\r\n");
	p --;
	
	strncpy(filename,pStart,strlen(pStart)-strlen(p));
	//printf("filename = %s\n",filename);
	//printf("p = %s\n",p);
	//printf("pStart = %s\n",pStart);

	return 0;	
}

//存储到本地文件
int storagefile(char* buf,char*filename , int len)
{
	if( len<=0 )
		return -1;
		
	char file_path[FILE_ID_LEN]={0};
	int wfd	= 0;
	ssize_t wlen=0;
	char * ptmp=NULL;	
	char flag[50] = {0}; //结束的标识符
	
	sprintf(file_path,"%s/source/%s",SOURCES_FILE_PATH,filename);
			
	ptmp = memstr(buf,len,"------");
	memcpy(flag,ptmp,memstr(ptmp,len,"\r\n")-ptmp);		 
	
	
	ptmp = memstr(ptmp,len,"\r\n\r\n");
	
	ptmp = ptmp + strlen("\r\n\r\n");//内容首地址
	
	char *pEnd = memstr(ptmp,len,flag) - strlen("\r\n");
				

	wfd = open(file_path ,O_RDWR | O_CREAT |O_TRUNC,0666);
	if(wfd == -1)
		LOG(MADULENAME , PROCNAME,"storagefile:open:error \n"); 
	wlen = write(wfd ,ptmp ,pEnd-ptmp );
	if(wlen !=(pEnd-ptmp))
	{
		LOG(MADULENAME , PROCNAME,"storagefile:write:error %d\n",-3);
		close(wfd) ;
	}
	
	close(wfd);

	return 0;	
}

//得到文件信息
int getfile_info(char* file_id,char* file_info)
{
	int ret =0;
	pid_t pid;
	int pfd_info[2];
	
	
	//pipe
	if( (ret = pipe(pfd_info))<0)
	{
		LOG(MADULENAME , PROCNAME,"getfile_info:pipe:error %d\n",ret ); 	
		exit(2);	
	}
	
	//启动子进程执行fdfs_file_info
	pid = fork();
	if(pid == -1)
	{
		LOG(MADULENAME , PROCNAME,"getfile_info:fork:error %d\n",-1);
		exit(5);
	}
	
	if( pid == 0 )
	{
		close(pfd_info[0]);
		if (dup2(pfd_info[1],STDERR_FILENO) <0) {
			LOG(MADULENAME , PROCNAME,"dup2 error"); 	
			
		}
		if (dup2(pfd_info[1],STDOUT_FILENO) <0) {
			LOG(MADULENAME , PROCNAME,"dup2 error"); 	
			
		}
		close(pfd_info[1]);
		
		file_id[strlen(file_id)-1]='\0';	
		execlp("fdfs_file_info","fdfs_file_info", CONFCLIENTPATH,file_id, NULL);
		
		
		close(pfd_info[1]);
		LOG(MADULENAME , PROCNAME,"getfile:execlp error"); 	
		exit(4);
		
	}
	else if( pid > 0)
	{
		sleep(2);
		close(pfd_info[1]);	
		int n = read(pfd_info[0],file_info,FILE_ID_LEN);
		
		if (n < 0 ) 
			LOG(MADULENAME,PROCNAME, "read error %s", strerror(errno));
		else if( n == 0)
			LOG(MADULENAME,PROCNAME, "已读完");
	}
		
	return 0;	
}

//得到上传文件的http
int  getHttp(char*file_info , char* file_name,char* fileHttp)
{
	//printf("file_info=%s\n",file_info);
	char* p =NULL;
	char* pStart=NULL;
	char* ipPre=NULL;
	char ipBuf[20]={0};
	char httpBuf[1024]={0};
	
	if(file_info == NULL || file_name==NULL )
	{
		LOG(MADULENAME,PROCNAME,"getHttp:error:file_info == NULL | file_name==NULL  error");
		exit(1);
	}	
		
	ipPre = "source ip address: ";
	
	p = strstr(file_info,"source ip address: ");
	p = p + strlen(ipPre);
	pStart = p;
	p = strstr(p,"\n"); 
	
	strncpy(ipBuf,pStart,strlen(pStart)- strlen(p));

	sprintf(httpBuf,"http://%s/%s",ipBuf,file_name);
	strcpy(fileHttp,httpBuf);
	fileHttp[strlen(fileHttp)-1]='\0';
	
	return 0;
}

//得到上传文件的时间
int  get_file_create_time(char*file_info , char* file_create_time)
{
	char* p =NULL;
	char* pStart=NULL;
	char* timePre=NULL;
	
	if(file_info == NULL || file_create_time==NULL )
	{
		LOG(MADULENAME,PROCNAME,"getHttp:error:file_info == NULL | file_name==NULL  error");
		exit(1);
	}	
		
	timePre = "file create timestamp: ";
	
	p = strstr(file_info,timePre);
	p = p + strlen(timePre);
	pStart = p;
	p = strstr(p,"\n"); 
	
	strncpy(file_create_time,pStart,strlen(pStart)- strlen(p));
	
	return 0;
}

//上传到分布式系统中
int storage_fdfs_file(char* file_info,char* filename,char*file_id,char* fileHttp)
{
	int ret =0;
	pid_t pid;
	int pfd[2];
	char file_path[FILE_ID_LEN]={0};
	struct sigaction newact,oldact;
	
	//注册信号处理函数
	newact.sa_handler = do_sig_child;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;	
	sigaction(SIGCHLD, &newact, &oldact);

	//pipe
	if( (ret = pipe(pfd))<0)
	{
		LOG(MADULENAME , PROCNAME,"getfile:storage_fdfs_file %d\n",ret ); 	
		exit(2);	
	}
	
	//启动子进程执行fdfs_upload
	pid = fork();
	if(pid == -1)
	{
		LOG(MADULENAME , PROCNAME,"getfile:getfilecontext %d\n",pid); 	
		exit(3);
	}
	
	if( pid == 0 )
	{	
		close(pfd[0]);
		if (dup2(pfd[1],STDERR_FILENO) <0) {
			LOG(MADULENAME , PROCNAME,"dup2 error"); 
			return 4;	
			
		}
		if (dup2(pfd[1],STDOUT_FILENO) <0) {
			LOG(MADULENAME , PROCNAME,"dup2 error"); 	
			return 5;
		}
		close(pfd[1]);
		
		sprintf(file_path,"%s/source/%s",SOURCES_FILE_PATH,filename);
		execlp("fdfs_upload_file","fdfs_upload_file", CONFCLIENTPATH,file_path, NULL);
		
		close(pfd[1]);
		LOG(MADULENAME , PROCNAME,"getfile:execlp error"); 	
		
		return 6;	
	}

	//父进程
	sleep(1);
	close(pfd[1]);	
	int n = read(pfd[0],file_id,FILE_ID_LEN);
	if (n < 0 ) 
		LOG(MADULENAME,PROCNAME, "read error %s", strerror(errno));
	else if( n == 0)
		LOG(MADULENAME,PROCNAME, "已读完");
	else if( n>0 )
	{
		close(pfd[0]);	
		
		ret = getfile_info(file_id,file_info);
		if(ret != 0)
		{
			LOG(MADULENAME,PROCNAME, "getfile_info:error:%d",ret);
			return 7;
		}
		
		ret = getHttp(file_info,file_id,fileHttp);
		if(ret !=0)
		{
			LOG(MADULENAME,PROCNAME,"getfile:getHttp:error:%d error",ret);	
			return 8;
		}
	}
	
	return 0;	
}

//将数据 存入redis中
int storageRedis(char* file_info ,char * file_id ,char* fileHttp, char* filename)
{
	int ret =0;
    redisContext* conn =NULL;
    char value[FILE_ID_LEN] ={0};
    char file_create_time[FILE_ID_LEN] ={0};
    char file_path[FILE_ID_LEN]={0};
    
    conn = rop_connectdb_nopwd(REDIS_IP, REDIS_PORT);
    if(conn == NULL)
    {
       LOG(MADULENAME,PROCNAME,"storageRedis:rop_connectdb_nopwd:error");
       
       	//删除文件
		sprintf(file_path,"%s/source/%s",SOURCES_FILE_PATH,filename);
		unlink(file_path);   
       return 2;
    }
    LOG(MADULENAME,PROCNAME,"storageRedis:rop_connectdb_nopwd:sucess");
	
	
	ret = get_file_create_time(file_info ,file_create_time);
	if(ret != 0)
    {
       LOG(MADULENAME,PROCNAME,"storageRedis:get_file_create_time:error:%d",ret);
       return 3;
    }

	file_id[strlen(file_id)-1]='\0';
	sprintf(value,"%s||%s||%s||%s",file_id,fileHttp,filename,file_create_time);
	
	ret = rop_list_push(conn, FILE_INFO_TABLE_KEY, value);
	if(ret != 0)
    {
       LOG(MADULENAME,PROCNAME,"storageRedis:rop_list_push:error:%d",ret);
       return 4;
    }
	
	LOG(MADULENAME,PROCNAME,"storageRedis:rop_list_push:success");
	
	ret  = rop_zset_increment(conn, ZSET_KEY, file_id);
	if(ret !=0)
	{
       LOG(MADULENAME,PROCNAME,"storageRedis:rop_zset_increment:error:%d",ret);
       return 5;
    }
	
	return 0;
}

//上传文件业务
int uploadfile(int len,char* fileHttp)
{
    int ret =0;
    char* buf =NULL;
    char filename[FILE_ID_LEN] = {0};
    char file_info[FILE_ID_LEN]={0};
    char file_id[FILE_ID_LEN]={0};
    char file_path[FILE_ID_LEN]={0};
  
    
    buf = malloc(len);
    if(buf == NULL)
    {
        LOG(MADULENAME , PROCNAME,"getfile:malloc %d\n",-3); 
        return 1;
	}   
    memset(buf,0,len);
      
     ret =  getbuf(buf,len);
     if(ret !=0)
    {
        LOG(MADULENAME , PROCNAME,"getfile:getbuf %d\n",ret); 
        return 3;
    }
      
	 ret = getfilename(buf,len , filename);
	 if(ret != 0 )
	 {
	 	LOG(MADULENAME , PROCNAME,"getfile:getfilename %d\n",ret); 
	 	return 4;
	}
	
	 ret = storagefile(buf,filename,len);
	 if(ret != 0 )
	 {
	 	LOG(MADULENAME , PROCNAME,"getfile:getfilecontext %d\n",ret); 
	 	return 6;
	}
	 		  
	 ret = storage_fdfs_file(file_info,filename,file_id,fileHttp);
	 if(ret != 0 )
	 {
	 	LOG(MADULENAME , PROCNAME,"getfile:getfilecontext %d\n",ret); 
	 	return 7;
	}
	    
	 ret = storageRedis(file_info,file_id,fileHttp , filename  );
	 if(ret != 0 )
	 {
	 	LOG(MADULENAME , PROCNAME,"getfile:file_name %d\n",ret); 
	 	return 7;
	}
	
	//删除文件
	sprintf(file_path,"%s/source/%s",SOURCES_FILE_PATH,filename);
	unlink(file_path);
	     
    printf("\n</pre><p>\n");
    return 0;
}


int main ()
{
    char fileHttp[FILE_ID_LEN]={0};

    while (FCGI_Accept() >= 0) {
        char *contentLength = getenv("CONTENT_LENGTH");
        int len;

        printf("Content-type: text/html\r\n"
                "\r\n");

        if (contentLength != NULL) {
            len = strtol(contentLength, NULL, 10);
        }
        else {
            len = 0;
        }

        if (len <= 0) {
            printf("No data from standard input.<p>\n");
        }
        else {
            int ret =0;
          
            ret = uploadfile(len,fileHttp);
            if(ret != 0)
                LOG(MADULENAME , PROCNAME,"getfile:error%d\n",ret); 


			printf("%s<br>\n<pre>\n",fileHttp);

            printf("\n</pre><p>\n");
        }
        
    } /* while */

    return 0;
}
