#define _GNU_SOURCE
#include "parser.h"
#define respond_template \
"HTTP/1.0 200 OK\r\n"\
"Content-Type: %s\r\n"\
"Content-Length: %ld\r\n"\
"Connection: close\r\n\r\n"

static const char *file_format[]={
    ".html","text/html",
    ".js","application/javascript",
    ".json","application/json",
    ".css","text/css",
    ".xml","application/xml",
    ".png","image/png",
    ".ico","image/x-icon",
    ".svg","image/svg+xml",
    ".ts","video/mp2t",
    ".woff", "font/woff",
    ".woff2", "font/woff2"
};
static const char error_meg[]="HTTP/1.0 404 Not Found\r\n\r\n";
static const size_t file_format_size=sizeof(file_format)/sizeof(file_format[0]);
typedef enum{
    GET,
    UNKNOWN,
    POST,
}method;
typedef enum{
    JS,
    HTML,
    CSS,
}path_t;
typedef struct {
    char method_str[20];
    char path_str[256];
    char version_str[20];
    method s_m;
    path_t s_p;
}request_line_t;

typedef struct{
    char req_str[4096];
    request_line_t *req;
}HTTP_t;
void method_state_init(HTTP_t *msg){
    if(!strcmp(msg->req->method_str,"GET")){
        msg->req->s_m=GET;
    }
    else{
        msg->req->s_m=UNKNOWN;
    }
}
static int parse(int new_socket,HTTP_t *msg){
    size_t byte_size=read(new_socket,msg->req_str,sizeof(msg->req_str));
    msg->req_str[byte_size]='\0';
    if(byte_size>0){
        int ret=sscanf(msg->req_str,"%19s %255s %19s",msg->req->method_str,msg->req->path_str,msg->req->version_str);
        //printf("%s\n",msg->req->path_str);              //temporarily disabled
        //printf("%s\n",msg->req_str);                  //temporarily disabled
        if(ret==3){
            method_state_init(msg);
        }
        else{
            perror("string split error");
            return -1;
        }
        if(msg->req->s_m==UNKNOWN){
            return 0; //not get response 404
        }
        else{
            return 1; // respond 200
        }
    }
    else{
        return -1; // read error response 404
    }
}
static int path_distinguish(HTTP_t *msg){
    char *MIME_TYPE=strrchr(msg->req->path_str,'.');
    if(MIME_TYPE==NULL){
        return 0;
    }
    for(int i=0;i<file_format_size;i+=2){
        if(!strcmp(MIME_TYPE,file_format[i])){
            return i;
        }
    }
    return -1; // fail
}
static int Whitelist_check(HTTP_t *msg){
    char list_path[]="./www/list.txt";
    struct stat f_st;
    FILE *Flist=fopen(list_path,"r");
    stat(list_path, &f_st);
    char list_content[f_st.st_size+1];
    memset(list_content,0,f_st.st_size+1);
    fread(list_content,sizeof(char),f_st.st_size,Flist);
    char *result=strstr(list_content,msg->req->path_str);
    int return_index;
    if(result==NULL){
        return_index=-1;
    }
    else{
        return_index=1;
    }
    fclose(Flist);
    return return_index;
}
static void error_respond(resp_use_t *res,HTTP_t *msg){
    send(res->new_socket,error_meg,strlen(error_meg),MSG_NOSIGNAL);
    if(msg){
        free(msg->req);
        free(msg);
        
    }
    shutdown(res->new_socket, SHUT_RDWR);
    close(res->new_socket);
    free(res);
    return;
}
static void success_respond(resp_use_t *res,char *content,char *res_header,HTTP_t *msg,FILE *res_file){
    size_t n;
    send(res->new_socket,res_header,strlen(res_header),MSG_NOSIGNAL);
    //printf("%s\n",res_header);                            //temporarily disabled
    while((n=fread(content,sizeof(char),sizeof(content),res_file))>0){
        send(res->new_socket,content,n,MSG_NOSIGNAL);
    }
    fclose(res_file);
    free(msg->req);
    free(msg);
    shutdown(res->new_socket, SHUT_RDWR);
    close(res->new_socket);
    free(res);
    return;
}
void respond_to_client(void *arg){
    resp_use_t *res=(resp_use_t*)arg;
    HTTP_t *msg;
    msg=(HTTP_t*)malloc(sizeof(HTTP_t));
    msg->req=(request_line_t*)malloc(sizeof(request_line_t));
    char general_dir[]="./www";
    char content[4096];
    char res_header[4096];
    memset(content,0,4096);
    memset(res_header,0,4096);
    struct stat st;
    memset(msg->req_str,0,sizeof(msg->req_str));
    if(parse(res->new_socket,msg)<1){
        error_respond(res,msg);
        return;
    }
    else{
        int file_format_index=path_distinguish(msg);
        //int whitelist_index=Whitelist_check(msg);       
        //printf("%d\n",file_format_index);                //temporarily disabled
        if(file_format_index<0 /*|| whitelist_index<1*/){
            error_respond(res,msg);
            return;
        }
        if(file_format_index==0){                  // if client msg is "GET/ HTTP1.0 " resend html file as default
            char html_dir[]="./www/index.html";
            FILE *res_file=fopen(html_dir,"r");
            stat(html_dir, &st);
            snprintf(res_header,sizeof(res_header)
            ,respond_template
            ,file_format[file_format_index+1]
            ,st.st_size);
            success_respond(res,content,res_header,msg,res_file);
            return;
        }
        else{
            char specific_path[512];
            memset(specific_path,0,256);
            snprintf(specific_path,sizeof(specific_path),"%s%s",general_dir,msg->req->path_str);
            FILE *res_file=fopen(specific_path,"r");
            if(res_file==NULL){    // when request file is not in /www folder, response error 404
                error_respond(res,msg);
                return;
            }
            stat(specific_path, &st);
            snprintf(res_header,sizeof(res_header)
            ,respond_template
            ,file_format[file_format_index+1]
            ,st.st_size);
            success_respond(res,content,res_header,msg,res_file);
            return;
        }
    }

}
