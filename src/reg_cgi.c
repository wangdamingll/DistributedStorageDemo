#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "fcgi_stdio.h"
#include "fcgi_config.h"
#include "cJSON.h"
#include "dao_mysql.h"
#include "util_cgi.h"
#include "make_log.h"

#define MADULENAME "fastCGI"
#define PROCNAME   "reg"

#define MYSQL_HOST_IP "101.200.170.178"
#define MYSQL_PASSWD "wangdaming"
#define MYSQL_DBNAME "itcast"

extern char g_host_name[HOST_NAME_LEN];

void deal_reg_query(void)
{
    char *query_string = getenv("QUERY_STRING");
    char user[128];
    char flower_name[128];
    char pwd[128];
    char tel[128];
    char email[128];
    char *out;
    char buffer[SQL_MAX_LEN] = {0};


    //当前时间戳
    struct timeval tv;
    struct tm* ptm;
    char time_str[128];

    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", ptm);

    query_parse_key_value(query_string, "user", user, NULL);
    query_parse_key_value(query_string, "flower_name", flower_name, NULL);
    query_parse_key_value(query_string, "pwd", pwd, NULL);
    query_parse_key_value(query_string, "tel", tel, NULL);
    query_parse_key_value(query_string, "email", email, NULL);

    //入库 
    MYSQL *conn = msql_conn("root", MYSQL_HOST_IP,MYSQL_PASSWD, MYSQL_DBNAME);
    if (conn == NULL) {
    	LOG(MADULENAME , PROCNAME,"deal_reg_query:msql_conn:error"); 
        return;
    }

    sprintf(buffer, "insert into %s (u_name, nicheng, password, phone, createtime, email) values ('%s', '%s', '%s', '%s', '%s', '%s')",
            "user", user, flower_name, pwd, tel, time_str ,email);

    if (mysql_query(conn, buffer))  {
    	LOG(MADULENAME , PROCNAME,"deal_reg_query:mysql_query:error"); 
    	 return;
    }


    cJSON *root = cJSON_CreateObject(); 
    cJSON_AddStringToObject(root, "code", "000");
    out = cJSON_Print(root);

    printf(out);
    free(out);
}

int main (void)
{

    while (FCGI_Accept() >= 0) {

        cgi_init();


        printf("Content-type: text/html\r\n");
        printf("\r\n");
        deal_reg_query();

    }
    return 0;
}
