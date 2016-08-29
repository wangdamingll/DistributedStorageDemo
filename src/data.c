#ifndef lint
static const char rcsid[] = "$Id: echo.c,v 1.5 1999/07/28 00:29:37 roberts Exp $";
#endif /* not lint */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <process.h>
#else
extern char **environ;
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "redis_op.h"
#include "fcgi_stdio.h"
#include "make_log.h"
#include "fcgi_config.h"
#include "cJSON.h"
#include "redis_op.h"

#define MADULENAME "fastCGI"
#define PROCNAME   "data"
#define FILE_ID_LEN 1024

#define QUERY_STRING_LEN 100
#define FILE_HOT    3

//#define REDIS_IP	"101.200.170.178"
#define REDIS_IP	"127.0.0.1"
#define REDIS_PORT	"6379"

#define FILE_INFO_TABLE_KEY "FILE_INFO_LIST"
#define ZSET_KEY 			"FILE_HOT_ZSET"

typedef  struct  _FILE_INFO_ {
	
	char file_id[FILE_ID_LEN] ;
	char file_url[FILE_ID_LEN] ;
	char file_name[FILE_ID_LEN] ;
	char file_create_time[FILE_ID_LEN];
	char file_picurl_m[FILE_ID_LEN];
	int pv;
	int hot;
	
}File_Info;

typedef struct _Query_String_Para
{
	char cmd[QUERY_STRING_LEN];
	char file_id[QUERY_STRING_LEN];
	int fromId;
	int count;
	
}QueryStringPara;



//解析前端的url请求参数cmd
int analysis_request_argument_cmd(QueryStringPara* qStrPara)
{
	int ret  =0;
	char *buf=NULL;
	char  * pStart =NULL;
	char * p =NULL;
	if(qStrPara == NULL ){
		ret =1;
		LOG(MADULENAME , PROCNAME,"dealData:analysis_request_argument:error:%d",ret); 
		return ret ;
	}
	
	memset(qStrPara,0,sizeof(QueryStringPara));
	
	p = buf = getenv("QUERY_STRING");
	//LOG(MADULENAME , PROCNAME,"dealData:getenv[%s]",buf); 
	p = strstr(p,"=");	
	pStart =  p+1 ;
	p = strstr(pStart,"&");
	
	strncpy(qStrPara->cmd,pStart,strlen(pStart)-strlen(p));
	
	//LOG(MADULENAME , PROCNAME,"dealData:getenv:[%s]",buf); 
	//LOG(MADULENAME , PROCNAME,"analysis_request_argument_cmd:qStrPara->cmd:[%s]",qStrPara->cmd); 
	
	return 0;
}


//解析前端的url除了cmd之外的其他参数
int analysis_request_argument(QueryStringPara* qStrPara)
{
	int ret  =0;
	char  * pStart =NULL;
	char * p =NULL;
	char intbuf[QUERY_STRING_LEN]={0};
	if(qStrPara == NULL ){
		ret =1;
		LOG(MADULENAME , PROCNAME,"dealData:analysis_request_argument:error:%d",ret); 
		return ret ;
	}
	//LOG(MADULENAME , PROCNAME,"dealData:qStrPara->cmd1:[%s]",qStrPara->cmd); 
	p = getenv("QUERY_STRING");
	
	if(strcmp(qStrPara->cmd,"newFile") == 0)  	//翻页请求
	{
		p = strstr(p,"&");	
		p = strstr(p,"=");
		p++;
		pStart = p;
		p = strstr(p,"&");
		
		strncpy(intbuf,pStart,strlen(pStart)-strlen(p));
		qStrPara->fromId = atoi(intbuf);
		memset(intbuf,0,QUERY_STRING_LEN);
		//LOG(MADULENAME , PROCNAME,"dealData:qStrPara->fromId:[%d]",qStrPara->fromId); 
		
		p = strstr(p,"=");
		p++;
		strncpy(intbuf,p,strlen(p));
		qStrPara->count = atoi(intbuf);
		//memset(intbuf,0,QUERY_STRING_LEN);
		//LOG(MADULENAME , PROCNAME,"dealData:qStrPara->count:[%d]",qStrPara->count); 
	}	
	else if(strcmp(qStrPara->cmd,"increase") == 0){   //下载请求
		char* temp = "fileId=";
		p = strstr(p,temp);	
		p = p +strlen(temp);
		strncpy(qStrPara->file_id,p,strlen(p));
		LOG(MADULENAME , PROCNAME,"dealData:qStrPara->file_id:[%s]",qStrPara->file_id); 
	}
	
	return 0;
}

//解析FILE_INFO_LIST的value信息
int get_File_info_from_value(File_Info* file_info,char* values)
{
	
	char* p =NULL;
	char* pStart =NULL;
	
	
	if(file_info == NULL|| values ==NULL )
	{
		LOG(MADULENAME,PROCNAME,"get_File_idandurlandname:file_info == NULL|| values ==NULL:error");
		return 1;
	}
	
	p = pStart= values;
	
	p = strstr(pStart,"||");
	memcpy(file_info->file_id,pStart,strlen(pStart)-strlen(p));
	
	pStart = p+2;
	p = strstr(pStart,"||");
	memcpy(file_info->file_url,pStart,strlen(pStart)-strlen(p));
	
	pStart = p+2;
	p = strstr(pStart,"||");
	memcpy(file_info->file_name,pStart,strlen(pStart)-strlen(p));
	
	//最后一个
	p = p+2;
	memcpy(file_info->file_create_time,p,strlen(p));
	
	return 0;
}

//得到文件信息
int get_File_Info(File_Info* file_info,int* file_num,QueryStringPara* qStrPara)
{
	int i,ret =0;
	int list_count=0;
	int get_num = 0;
	redisContext* conn =NULL;

    char values[FILE_ID_LEN][FILE_ID_LEN];
	
	if(file_info == NULL || file_num <0)
	{
		LOG(MADULENAME,PROCNAME,"get_File_Info:file_info == NULL || file_num <0:error");
		return 1;
	}
	
	
	
    conn = rop_connectdb_nopwd(REDIS_IP, REDIS_PORT);
    if(conn == NULL)
    {
       LOG(MADULENAME,PROCNAME,"get_File_Info:rop_connectdb_nopwd:error");
       return 2;
    }
   
	
	list_count = rop_get_list_cnt(conn, FILE_INFO_TABLE_KEY);
	if(list_count < 0)
    {
       LOG(MADULENAME,PROCNAME,"get_File_Info:rop_get_list_cnt:error:%d",list_count);
       return 3;
    }
    
	ret = rop_range_list(conn, FILE_INFO_TABLE_KEY, qStrPara->fromId, qStrPara->count, values, &get_num);
	if(ret != 0)
    {
       LOG(MADULENAME,PROCNAME,"get_File_Info:rop_range_list:error:%d",ret);
       return 4;
    }

	for (i = 0; i < get_num; ++i) {
         get_File_info_from_value(&file_info[i],values[i]);  
    }
    
    *file_num = get_num;
    
	return 0;
}

//字符串替换
int StrReplace(char strRes[],char from[], char to[]) 
{
    int i,flag = 0;
    char *p,*q,*ts;
    for(i = 0; strRes[i]; ++i) {
        if(strRes[i] == from[0]) {
            p = strRes + i;
            q = from;
            while(*q && (*p++ == *q++));
            if(*q == '\0') {
                ts = (char *)malloc(strlen(strRes) + 1);
                strcpy(ts,p);
                strRes[i] = '\0';
                strcat(strRes,to);
                strcat(strRes,ts);
                free(ts);
                flag = 1;
            }
        }
    }
    return flag;
}

//下载时，下载量+1
int redis_click_addone(File_Info* file_info,QueryStringPara* qStrPara,int file_num)
{
	int ret =0;

    redisContext* conn =NULL;conn = rop_connectdb_nopwd(REDIS_IP, REDIS_PORT);
    if(conn == NULL)
    {
       LOG(MADULENAME,PROCNAME,"redis_click_addone:rop_connectdb_nopwd:error");
       return 1;
    }
    LOG(MADULENAME,PROCNAME,"redis_click_addone:rop_connectdb_nopwd:sucess");
	
	ret = StrReplace(qStrPara->file_id,"%2F","/");
	if(ret <=0)
	{
       LOG(MADULENAME,PROCNAME,"redis_click_addone:StrReplace:error:%d",ret);
       return 2;
    }
	
	ret  = rop_zset_increment(conn, ZSET_KEY, qStrPara->file_id);
	if(ret !=0)
	{
       LOG(MADULENAME,PROCNAME,"storageRedis:rop_zset_increment:error:%d",ret);
       return 4;
    }
   
	return 0;	
}

//得到文件下载量
int get_file_pv(File_Info* file_info,int file_num)
{
	int i=0,ret =0;
	redisContext* conn =NULL;
	
	if(file_info ==NULL || file_num < 0  )
	{
		ret =1;
	  	LOG(MADULENAME,PROCNAME,"get_file_pv:file_info ==NULL || file_num ==NULL:error:%d",ret);
	  	return 1;
	}
	
	conn = rop_connectdb_nopwd(REDIS_IP, REDIS_PORT);
    if(conn == NULL)
    {
       LOG(MADULENAME,PROCNAME,"redis_click_addone:rop_connectdb_nopwd:error");
       return 2;
    }
    LOG(MADULENAME,PROCNAME,"redis_click_addone:rop_connectdb_nopwd:sucess");
	
	
	for(i=0;i<file_num;i++){
		
		file_info[i].pv = rop_zset_get_score(conn, ZSET_KEY, file_info[i].file_id);
		if(file_info[i].pv <=0)
		{
	       LOG(MADULENAME,PROCNAME,"redis_click_addone:get_file_pv:error:%d",ret);
	       return 2;
	    }
	   file_info[i].pv--;
	   
	   if(file_info[i].pv>=FILE_HOT)
	   		file_info[i].hot=1;
	   		
	}
	
	return 0;	
}

//得到文件类型
int get_file_picurl_m(File_Info* file_info,int file_num)
{
	int ret =0,i=0;
	char* p =NULL;
	char temp[10]={0};
	if(file_info ==NULL )
	{
		ret =1;
	  	LOG(MADULENAME,PROCNAME,"get_file_pv:file_info ==NULL :error:%d",ret);
	  	return 1;
	}	
	
	for(i=0;i<file_num;i++){
		
		p= file_info[i].file_name;
		p= p +  strlen(file_info[i].file_name);
		
		while(*p-- != '.');
		p = p+2;
		
		strncpy(temp,p,strlen(p));
		
		sprintf(file_info[i].file_picurl_m,"static/file_png/%s.png",temp);	
	}
	
	return 0;	
}

//创建json对象
int  create_Json_objects(File_Info* file_info,int file_num)
{
	cJSON *root,*thm,*fld;
	char *out;
	int i ;

	root=cJSON_CreateObject();	
	cJSON_AddItemToObject(root, "games", thm=cJSON_CreateArray());
	
	for (i=0;i<file_num;i++)
	{
		
		cJSON_AddItemToArray(thm,fld=cJSON_CreateObject());	
		cJSON_AddStringToObject(fld,"id",(const char*)file_info[i].file_id);
		cJSON_AddNumberToObject(fld,"kind",2);
		
		cJSON_AddStringToObject(fld,"title_m",(const char*)file_info[i].file_name);
		cJSON_AddStringToObject(fld,"title_s","rect");
		cJSON_AddStringToObject(fld,"descrip",(const char*)file_info[i].file_create_time);
		cJSON_AddStringToObject(fld,"picurl_m",file_info[i].file_picurl_m);
		cJSON_AddStringToObject(fld,"url",(const char*)file_info[i].file_url);
		cJSON_AddNumberToObject(fld,"pv",file_info[i].pv);
		cJSON_AddNumberToObject(fld,"hot",file_info[i].hot);
	}
	
	out=cJSON_Print(root);	
	cJSON_Delete(root);	
	printf("%s\n",out);	
	free(out);
	
	return 0;
}

int dealData(File_Info* file_info,int * file_num)
{
	int ret =0;
	//int file_num_temp=0;
	QueryStringPara qStrPara;
	memset(file_info,0,sizeof(File_Info)*FILE_ID_LEN);
	
	//先解析cmd
	ret = analysis_request_argument_cmd(&qStrPara);	
	if(ret !=0 )
	{
		LOG(MADULENAME , PROCNAME,"dealData:analysis_request_argument:error:%d",ret); 
		return 1;	
	}	
	
	if(strcmp(qStrPara.cmd,"newFile") == 0){  //翻页请求
		
		//解析其他参数
		ret = analysis_request_argument(&qStrPara);
		if(ret !=0 )
		{
			LOG(MADULENAME , PROCNAME,"dealData:analysis_request_argument:error:%d",ret); 
			return 3;	
		}	
		
		ret = get_File_Info(file_info,file_num,&qStrPara);
		if(ret !=0 )
		{
			LOG(MADULENAME , PROCNAME,"dealData:get_File_Info:error:%d",ret); 
			return 4;	
		}	
		
		ret = get_file_picurl_m(file_info,*file_num);
		if(ret !=0 )
		{
			LOG(MADULENAME , PROCNAME,"dealData:get_file_picurl_m:error:%d",ret); 
			return 5;	
		}	
		
		ret = get_file_pv(file_info,*file_num);
		if(ret !=0 )
		{
			LOG(MADULENAME , PROCNAME,"dealData:get_File_Info:error:%d",ret); 
			return 6;	
		}	
		
		ret = create_Json_objects(file_info,*file_num);
		if(ret !=0 )
		{
			LOG(MADULENAME , PROCNAME,"dealData:create_Json_objects:error:%d",ret); 
			return 7;	
		}	
		
	}
	else if(strcmp(qStrPara.cmd,"increase") == 0) {   //下载请求
		
		//解析其他参数
		ret = analysis_request_argument(&qStrPara);
		if(ret !=0 )
		{
			LOG(MADULENAME , PROCNAME,"dealData:analysis_request_argument:error:%d",ret); 
			return 8;	
		}	
		
		ret = redis_click_addone(file_info,&qStrPara,*file_num);
		if(ret !=0 )
		{
			LOG(MADULENAME , PROCNAME,"dealData:redis_click_addone:error:%d",ret); 
			return 9;	
		}	
		
	}
	
	return 0;
}


int main ()
{
	int ret =0;
	int file_num=0;
	File_Info file_info[FILE_ID_LEN];

    while (FCGI_Accept() >= 0) {
    	
    	printf("Content-type: text/javascript\r\n"
                "\r\n");
    	
		ret = dealData(file_info,&file_num) ;
		if(ret !=0)
		{
			LOG(MADULENAME , PROCNAME,"main:dealData:error:%d",ret); 
			return -1;
		} 
    	
       
       
    } /* while */

    return 0;
}
