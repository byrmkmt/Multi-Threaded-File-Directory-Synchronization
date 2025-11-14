#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

void formatChecking(int * ipaddress,int* portNumber,char* ip,char* port);
void clientOperations(char* directory,char* ip,char* port);
int isDirectory(char *path);
void clientPath(int cfd,char* path);
void pathCheck(char* path);
int findSlash(const char* path);

typedef struct{	
	char name[256];
	mode_t mode;
	int isDir;
	unsigned long int size;
}PType;

PType item;

int 
main(int argc,char* argv[]){
	if(argc!=4){
		printf("Argument count should be four.\n");
		exit(1);
	}
	int ipaddress;
	int portNumber;
	formatChecking(&ipaddress,&portNumber,argv[2],argv[3]);
	clientOperations(argv[1],argv[2],argv[3]);
	return 0;
}

void 
clientOperations(char* directory,char* ip,char* port){	
	ssize_t numRead;	
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int cfd;
	int i,j;

	/*	Socket kurulumu.	*/
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;

	if (getaddrinfo(ip,port,&hints,&result)!=0){
		perror("");
		exit(1);
	}

	for(rp=result;rp!=NULL;rp=rp->ai_next){
		cfd=socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
		if(cfd==-1)
			continue;
		if (connect(cfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;
		close(cfd);
	}

	if (rp==NULL)
		exit(1);
	/*	Dinamik olarak ayrılmış result degiskenine ait resourcelar serbest bırakılır.	*/
	freeaddrinfo(result);

	if(findSlash(directory)==1)
		pathCheck(directory);
	clientPath(cfd,directory);

	exit(0);
}

int findSlash(const char* path){
	int i;
	int rv=0;
	for(i=0;path[i]!='\0';++i){
		if(path[i]=='/'){
			rv=1;
			break;
		}
	}
	return rv;
}

void 
pathCheck(char* path){
	char fname[255];
	char temp[255];
	char fn[255];
	char* log,* first;
	strcpy(fname,path);

	log=strtok(fname,"/");
	while(log!=NULL){
		first=log;
		log=strtok(NULL,"/");
		if(log!=NULL){
			strcat(fn,"/");
			strcat(fn,first);
			strcpy(temp,log);
		}
	}
	if(strlen(fn)!=0)
		chdir(&fn[1]);
	if(strlen(temp)!=0)
		strcpy(path,temp);
}

void 
clientPath(int cfd,char* path){
	DIR* direct;
	struct dirent* dir;
	struct stat statbuf;
	PType instance;
	char buff[512];	
	int i;
	int size;
	if(lstat(path,&statbuf) == -1){
		perror("System path error:");
		exit(1);
	}
	/* Kendi directory variableımızı create ediyor ve onu hemen socket'e yazıyor	*/
	strcpy(instance.name,path);
	instance.mode=statbuf.st_mode;
	instance.isDir=1;
	write(cfd,&instance,sizeof(PType));
	/*	Sonra kendi directoryimiz içindeki file'ların bilgilerini yazıyor.	*/
	if((direct=opendir(path))==NULL){
		perror("Directory opening error = ");
		exit(1);
	}
	while((dir=readdir(direct))!=NULL){
		if(strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0){
			strcat(instance.name,"/");
			strcat(instance.name,dir->d_name);
			if(isDirectory(instance.name)!=0){
				if (lstat(instance.name,&statbuf) == -1){
					perror("System path error:");
					exit(1);
				}
				instance.mode=statbuf.st_mode;
				instance.size=statbuf.st_size;
				instance.isDir=0;
				i=open(instance.name,O_RDONLY,instance.mode);
				write(cfd,&instance,sizeof(PType));
				while((size=read(i,buff,sizeof(char)*512))>0)
					write(cfd,&buff,sizeof(char)*size);
				close(i);
			}
			strcpy(instance.name,path);
		}
	}
	closedir(direct);
	/*	Sonra subdirectorylere geçiyoruz.	*/
	if((direct=opendir(path))==NULL){
		perror("Directory opening error = ");
		exit(1);
	}
	while((dir=readdir(direct))!=NULL){
		if(strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0){
			strcat(instance.name,"/");
			strcat(instance.name,dir->d_name);
			if(isDirectory(instance.name)==0)
				clientPath(cfd,instance.name);
			strcpy(instance.name,path);
		}
	}
	closedir(direct);
}

void 
formatChecking(int* ipaddress,int* portNumber,char* ip,char* port){
	struct in_addr info;
	int rv=inet_pton(AF_INET,ip,&info);
	if(rv==-1){
		printf("IPv4 checking error.\n");
		exit(1);
	}
	else if(rv==0){
		if(strcmp(ip,"localhost")!=0){
			printf("IPv4 format is invalid.\n");
			exit(1);
		}
	}
	*ipaddress=info.s_addr;
	if(sscanf(port,"%d",portNumber)!=1){
		printf("Port Number should be a integer number.\n");
		exit(1);
	}
	if(*portNumber<0){
		printf("Port Number can't be negative number.\n");
		exit(1);
	}
}

int 
isDirectory(char *path){
	struct stat statbuf;
	if (lstat(path, &statbuf) == -1){
		perror("System path error:");
		exit(1);
	}
	if(S_ISDIR(statbuf.st_mode)){
		return 0;
	}
	else
		return 1;
}