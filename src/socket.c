#include "socket.h"

struct sockaddr_in addr_init(){
    struct sockaddr_in addr= {0};
    addr.sin_addr.s_addr=INADDR_ANY; //init local address (0.0.0.0)
    addr.sin_family=AF_INET;
    addr.sin_port=htons(8080);
    return addr;
}

bool network_init(int *fd,struct sockaddr_in *addr){
    int ret;
    *fd=socket(AF_INET,SOCK_STREAM,0); //init IPV4 and TCP connection
    if(*fd==-1){
        perror("socket initialize error");
        return 0; 
    }
    int opt=1;
    setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(bind(*fd,(struct sockaddr *)addr,sizeof(*addr))==-1){ //binding with localport
        perror("binding error");
        return 0;
    }
    if(listen(*fd,4096)==-1){
        perror("listen fail");
        return 0;
    }

    return 1;
}

