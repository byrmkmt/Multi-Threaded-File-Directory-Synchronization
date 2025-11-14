#define _BSD_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <time.h>
#define ADDRSTRLEN 100
#define DIRMODE S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH

typedef struct{	
	char name[256];
	mode_t mode;
	/*	Directory yada file 	*/
	int isDir;
	/*	File ise size bilgisi*/
	unsigned long int size;
}PType;

static pthread_mutex_t mtx=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond=PTHREAD_COND_INITIALIZER;

sem_t semp;
int cfd;
PType messageItem;
char currentDir[255];
char addrStr[ADDRSTRLEN];

void formatChecking(int* numOfThread,int* portNumber,char* thread,char* port);
void serverOperations(char* directory,int numOfThread,char* port);
void socketOperations(pthread_t* threads,char* directory,int numOfThread,char* port);
int isDirectory(char *path);
int hasDirectory(char *path);
void removeDirectory(char* path);

/*	Signal handler 	*/
static void 
sigHandler(int sig){
	if(sig==SIGINT){
		printf("\nServer exit from a signal.\n");
		exit(1);
	}
}

/* Thread function 	*/
static void* 
clientsOperations(void* arg){
	printf("Multi-threading.	\n");
	sem_wait(&semp);	
	printf("A thread is using for client %s.\n",addrStr);	
	char copy[255];
	int size,i,j;
	char* path=(char*) arg;	
	strcpy(copy,path);
	pthread_mutex_lock(&mtx);	
	// reading datas from client	
	while(read(cfd,&messageItem,sizeof(PType))>0){	
		strcat(copy,"/");
		strcat(copy,messageItem.name);
		// Data Tipi directory ise
		if(messageItem.isDir==1){
			int fd;
			char logf[255];			
			//	Client Directory serverde yok ise eklenir.
			if(hasDirectory(copy)==-1)
				mkdir(copy,messageItem.mode);
			//	Şuanda bu directorydeyiz.
			strcpy(logf,currentDir);
			strcat(logf,"/log.dat");
			strcpy(currentDir,copy);
			//	Her bir directory için log file oluşturulur.
			strcat(copy,"/log.dat");
			fd=open(copy,O_WRONLY|O_CREAT|O_APPEND,0666);
			close(fd);
		}
		// Data Tipi file ise
		else{
			int fd,logfd;
			char buff;
			char logID[255];
			char buf[255];
			int print=0;
			unsigned long int count=0;
			struct stat statbuf;
			strcpy(buf,messageItem.name);
			/*	log dosyası path 	*/
			strcpy(logID,currentDir);
			strcat(logID,"/log.dat");
			if(hasDirectory(copy)==-1){
				/*	Dosya yeni oluşturulacaktır. Çünkü dosya henüz yoktur.	*/
				strcpy(buf," dosyası oluşturuldu.\tTarih: ");
				print=1;
			}
			else{
				/*	Dosya zaten vardı ve güncel mi?	*/
				if (lstat(copy,&statbuf) == -1){
					perror("System path error:");
					exit(1);
				}
				if(messageItem.size!=(int)statbuf.st_size){
					print=1;
					strcpy(buf," dosyası güncellendi.\t Tarih: ");
				}
			}			
			fd=open(copy,O_WRONLY|O_CREAT|O_TRUNC,messageItem.mode);
			if (lstat(copy,&statbuf) == -1){
				perror("System path error:");
				exit(1);
			}
			if(print==1){			
				logfd=open(logID,O_WRONLY|O_APPEND);
				write(logfd,addrStr,sizeof(char)*strlen(addrStr));
				write(logfd,"\t",sizeof(char));
				write(logfd,messageItem.name,sizeof(char)*strlen(messageItem.name));	
				write(logfd,buf,sizeof(char)*strlen(buf));
				write(logfd,ctime(&statbuf.st_mtime),sizeof(char)*strlen(ctime(&statbuf.st_mtime)));
				write(logfd,"\n",sizeof(char));
				close(logfd);
			}			
			// Dosyamız yeni baştan güncellenir.
			// Dosya size'ına kadar okuma yapılır.
			while(count<messageItem.size){
				/*	Socketten veriler okunuyor.	*/
				read(cfd,&buff,sizeof(char));
				/*	Server file'ımıza veriler kopyalanıyor.	*/
				write(fd,&buff,sizeof(char));
				count++;
			}
			close(fd);
		}
		strcpy(copy,path);
	}

	printf("A thread is completed.\n");

	pthread_cond_signal(&cond);

	pthread_mutex_unlock(&mtx);	

//	pthread_exit(NULL);
}

int 
main(int argc,char* argv[]){
	if(argc!=4){
		printf("Argument count should be four.\n");
		exit(1);
	}
	int threadPoolSize;
	int portNumber;
	formatChecking(&threadPoolSize,&portNumber,argv[2],argv[3]);
	serverOperations(argv[1],threadPoolSize,argv[3]);
	return 0;
}

void 
serverOperations(char* directory,int numOfThread,char* port){
	pthread_t* threads;
	char log[255];
	int fd;
	if((threads=calloc(sizeof(pthread_t),numOfThread))==NULL){
		printf("Thread pool variables are created error.\n");
		exit(1);
	}
	// existing directory removing.
	if(isDirectory(directory)==0)
		removeDirectory(directory);
	mkdir(directory,DIRMODE);
	// Client-Server Model with Sockets
	socketOperations(threads,directory,numOfThread,port);
	free(threads);
}

void 
socketOperations(pthread_t* threads,char* directory,int numOfThread,char* port){
	struct addrinfo serverSide,*clientSide,*loop;
	struct sockaddr_storage claddr;
	int sfd;
	int i;
	int opt=1;
	struct sigaction sa;

	socklen_t addrlen;
	char host[NI_MAXHOST];
	char service[NI_MAXSERV];

	sigemptyset(&sa.sa_mask);
	sa.sa_flags=0;
	sa.sa_handler=sigHandler;

	memset(&serverSide,0,sizeof(struct addrinfo));
	serverSide.ai_canonname = NULL;
	serverSide.ai_addr = NULL;
	serverSide.ai_next = NULL;
	serverSide.ai_socktype = SOCK_STREAM;
	serverSide.ai_family = AF_INET;
	serverSide.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

	if(getaddrinfo(NULL,port,&serverSide,&clientSide)!=0){
		printf("Client's datas can't be taken.\n");
		exit(1);
	}

	for(loop=clientSide;loop!=NULL;loop->ai_next){
		sfd=socket(loop->ai_family,loop->ai_socktype,loop->ai_protocol);
		if(sfd==-1)
			continue;
		if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))==-1){
			printf("Socket option error!\n");
			exit(1);
		}
		if (bind(sfd,loop->ai_addr,loop->ai_addrlen)==0)
		break;
		close(sfd);		
	}

	if(listen(sfd,20)==-1){
		printf("Server listining(pasive mode) error.\n");
		exit(1);
	}
	// deallocating
	freeaddrinfo(clientSide);
	sem_init(&semp,0,0);

	for(i=0;i<numOfThread;++i)
		pthread_create(&threads[i],NULL,clientsOperations,directory);

	while(1){
		sigaction(SIGINT,&sa,NULL);	
		/*	Client geliyor ve bilgileri alınır.	*/
		addrlen=sizeof(struct sockaddr_storage);
		cfd=accept(sfd,(struct sockaddr *)&claddr,&addrlen);
		if(cfd==-1){
			printf("Accept error");
			continue;
		}
		if (getnameinfo((struct sockaddr *)&claddr,addrlen,host,NI_MAXHOST,service,NI_MAXSERV,0)==0)
			snprintf(addrStr, ADDRSTRLEN, "(%s, %s)", host, service);
		else
			snprintf(addrStr, ADDRSTRLEN, "(?UNKNOWN?)");
		/*	Bir thread kullanılır.	*/
		printf("%s\n",addrStr);
		sem_post(&semp);
		pthread_cond_wait(&cond,&mtx);
		/*	Diğer 9 thread beklemeli(bloke edilir.)	*/
		/*  Read ve write işlemleri için	*/				
	}
	for(i=0;i<numOfThread;++i)
		pthread_join(threads[i],NULL);
}

void 
formatChecking(int* numOfThread,int* portNumber,char* thread,char* port){
	if(sscanf(thread,"%d",numOfThread)!=1){
		printf("Number of thread should be a integer number.\n");
		exit(1);
	}
	if(*numOfThread<0){
		printf("Number of thread can't be negative number.\n");
		exit(1);
	}
	if(sscanf(port,"%d",portNumber)!=1){
		printf("Port Number should be a integer number.\n");
		exit(1);
	}
	if(*portNumber<0){
		printf("Port Number can't be negative number.\n");
		exit(1);
	}
}

void
removeDirectory(char* path){
	DIR* direct;
	struct dirent* dir;
	char str[255];

	strcpy(str,path);
	if((direct=opendir(path))==NULL){
		perror("Directory opening error = ");
		exit(1);
	}
	// İlk önce kendi directorydeki dosyaları siliyoruz.
	while((dir=readdir(direct))!=NULL){
		if(strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0){
			strcat(str,"/");
			strcat(str,dir->d_name);
			if(isDirectory(str)!=0)
				remove(str);
			strcpy(str,path);
		}
	}
	closedir(direct);
	if((direct=opendir(path))==NULL){
		perror("Directory opening error = ");
		exit(1);
	}
	// Sonra sub directorydeki dosyaları siliyoruz.	
	while((dir=readdir(direct))!=NULL){
		if(strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0){
			strcat(str,"/");
			strcat(str,dir->d_name);
			if(isDirectory(str)==0)
				removeDirectory(str);
			strcpy(str,path);
		}
	}
	closedir(direct);
	// En sonda kendi (boş) directoryimizi siliyoruz.
	rmdir(path);
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

int 
hasDirectory(char *path){
	struct stat statbuf;
	if (lstat(path, &statbuf) == -1)
		return -1;
	else{
		if(S_ISDIR(statbuf.st_mode))
			return 0;
		else
			return 1;
	}
}