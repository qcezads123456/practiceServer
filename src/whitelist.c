#include "whitelist.h"

void traversal_folder(char *dir,char *output_buf,int buf_size,size_t *used){
    struct dirent *entry;
    struct stat st;
    DIR *dp = opendir(dir); // Open current directory
    if (dp == NULL) {
        perror("opendir");
        return;
    }
    while ((entry = readdir(dp)) != NULL) {
        if(strcmp(entry->d_name,".")==0 ||strcmp(entry->d_name,"..")==0){ //ignore .. and .
            continue;
        }
        char target_path[512];
        snprintf(target_path,sizeof(target_path),"%s/%s",dir,entry->d_name);  //combine target path and file name
        stat(target_path,&st);
        if(S_ISDIR(st.st_mode)){
            traversal_folder(target_path,output_buf,buf_size,used);  // travel every folder recurssively 
        }
        else{
            int n=snprintf(output_buf+*used,buf_size-*used,"%s\n",target_path);  //writing target path in output buffer
            *used+=n;
        }
    }
    closedir(dp);
    return;
}

void whitelist_init(){
    char output_buf[1024*1024];
    char *output_dir="./whitelist/whitelist.txt";  //whitelist target path
    char *dir="./www";
    size_t used=0;
    FILE *fp=fopen(output_dir,"w");  //create a new whitelist.txt when whitelist folder dont have txt file, otherwise, replace it with a new blank txt file
    traversal_folder(dir,output_buf,1024*1024,&used);  //call traversal and write all path in output_buf
    fwrite(output_buf,used,sizeof(char),fp);  
    fclose(fp);
    return;
}