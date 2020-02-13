#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <termios.h>
#include <sys/stat.h>
#include <ctype.h>  
#include <sys/sendfile.h>
#include <fcntl.h>
#include <semaphore.h>

#define PORT 3535
#define BACKLOG 32
#define TABLE_SIZE 999983
#define MESSAGE_SIZE 200
#define MODE 3 //1 PARA SEMAFORO, 2 PARA MUTEX, 3 PARA TUBERIA
//global syncronization variables
//semaphores
sem_t dataDogs_sem;
sem_t log_sem;
sem_t hash_sem;
sem_t array_sem;
//mutex 
pthread_mutex_t dataDogs_mutex;
pthread_mutex_t log_mutex;
pthread_mutex_t hash_mutex;
pthread_mutex_t array_mutex;
//pipes
int dataDogs_pipe[2];
int log_pipe[2];
int hash_pipe[2];
int array_pipe[2];

//CAMBIO//
int arrMH[BACKLOG];
//CAMBIO//

//USED STRUCTURES
//Record's structure
struct dogType{
    int id;
    char nombre[32];
    char tipo[32];
    int edad;
    char raza[16];
    int estatura;
    float peso;
    char sexo;
    long next; //file address for the next dog with the same name
    long addr; //file address for this dog
};

//Argument for threads
struct Argument{
    struct sockaddr_in client;
    int clientfd;
    long *table;
    //CAMBIO//
    int idThread;
    //CAMBIO//
} ;

//Log's structure
struct log {
    char date[20];
    char clientIp[20];
    char operation[32];
    char searchedString [32];
};



//GENERAL FUNCTIONS
//Checks Errors
bool checkError(int number,int errorValue, char *message){
    if(number==errorValue){
        perror(message);
        exit(-1);
        return false;
    }
    return true;
}

//Gets the size of a file
int getFileSize(int fd){
    struct stat s;
    if (fstat(fd, &s) == -1) {
        int saveErrno = errno;
        fprintf(stderr, "fstat(%d) returned errno=%d.", fd, saveErrno);
        return(-1);
    }
    return(s.st_size);
}

//Returns a copy in lower case of a given string
char* toLowerCase(char *str,int size){
    char *lowStr;
    lowStr=malloc(size*sizeof(char));
    int j;
    for(int i = 0; str[i]; i++){
        lowStr[i] = tolower(str[i]);
        j=i;
    }
    lowStr[j+1]='\0';
    return lowStr;
}

//Turns into Lower Case
char *strlwr(char *str){
    unsigned char *p = (unsigned char *)str;
    while (*p) {
        *p = tolower((unsigned char)*p);
        p++;
    }
    return str;
}

//Fills the array with a given value
void fillArray(long *array,int size,int value){
    for(int i=0;i<size;i++){
        *(array+i)=value;
    }
}



//SYNC FUNCTIONS
//Initialices the sync method
int init_lock(sem_t *sem,pthread_mutex_t *mutex, int *pipes){
    int r;
    //1 PARA SEMAFORO, 2 PARA MUTEX, 3 PARA TUBERIA
    if(MODE==1){
        r=sem_init(sem, 0, 1);// 2nd arg is 0: lock shared between threads, 3rd arg is 1:puts 1 as value of lock
    }else if(MODE==2){
        r=pthread_mutex_init(mutex, NULL);
        if(r!=0)
            r=-1;
    }else if(MODE==3){
        char witness='T';
        pipe(pipes); 
        r=write(pipes[1], &witness, 1);//writes "witness"
    }else{
        r=-1;
    }
    return r;
}

//Enter the sync method
int enter_lock(sem_t *sem,pthread_mutex_t *mutex, int *pipe){
    int r;
    //1 PARA SEMAFORO, 2 PARA MUTEX, 3 PARA TUBERIA
    if(MODE==1){
        r=sem_wait(sem);
    }else if(MODE==2){
        r=pthread_mutex_lock(mutex);
        if(r!=0)
            r=-1;
    }else if(MODE==3){
        char witness; 
        r=read(pipe[0], &witness, 1);//tries to read witness
    }else{
        r=-1;
    }
    return r;
}

//Finish/Exit the sync method
int exit_lock(sem_t *sem,pthread_mutex_t *mutex, int *pipe){
    int r;
    //1 PARA SEMAFORO, 2 PARA MUTEX, 3 PARA TUBERIA
    if(MODE==1){
        r=sem_post(sem);
    }else if(MODE==2){
        r=pthread_mutex_unlock(mutex);
        if(r!=0)
            r=-1;
    }else if(MODE==3){
        char witness; 
        r=write(pipe[1], &witness, 1);//writes witness
    }else{
        r=-1;
    }
    return r;
}

//Kills the sync method
int destroy_lock(sem_t *sem,pthread_mutex_t *mutex, int *pipe){
    int r;
    //1 PARA SEMAFORO, 2 PARA MUTEX, 3 PARA TUBERIA
    if(MODE==1){
        r=sem_destroy(sem);
    }else if(MODE==2){
        r=pthread_mutex_destroy(mutex);
        if(r!=0)
            r=-1;
    }else if(MODE==3){
        r=close(pipe[0]);  
        int x=close(pipe[1]);
        if(x==-1)
            r=-1;
    }else{
        r=-1;
    }
    return r;
}



//SOCKECT FUNCTIONS
int total_recv(int sockfd, void *buff, int size) {
    int received = 0;
    while(received < size) {
        int r = recv(sockfd, buff + received, size - received, 0);
        checkError(r,-1,"error en recv");
        received += r;
    }
    return received;
}

int total_send(int sockfd, void *buff, int size){
    int sent = 0;
    while(sent < size){
        int r = send(sockfd, buff + sent, size - sent, 0);
        checkError(r,-1,"error en el sent");
        sent += r;
    }
    return sent;
}



//HASH TABLE FUNCTIONS

//Hash function
int hashFunction(unsigned char *str){
    unsigned long hash = 5381;
    int c;
    while (c = *str++){
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash%TABLE_SIZE;
}

//Reads the file "table" to get data, if they exist
void initTable(long *table){
    FILE *file;
    int ac=access("hashTable.dat", F_OK);
    if(ac==-1){
        //When File doesn't exist
        fillArray(table,TABLE_SIZE,-1);
        file=fopen("hashTable.dat","wb");
        if(file==NULL){
            perror("initTable: error in fopen when doesn't exist\n"); exit(-1);
        }
        int r=fwrite(table,sizeof(long),TABLE_SIZE,file);
        checkError(r,0,"initTable: error in fwrite\n");
        fclose(file);
    }
    file=fopen("hashTable.dat","rb");
    if(file==NULL){
        perror("initTable: error in fopen\n"); exit(-1);
    }
    int r=fread(table,sizeof(long),TABLE_SIZE,file);
    checkError(r,0,"initTable: error in fread\n");
    fclose(file);
}

//Updates the table in the file
void updateTable(long *table){
    FILE *file;
    file=fopen("hashTable.dat","wb");
    if(file==NULL){
        perror("updateTable: error in fopen\n");
        exit(-1);
    }
    int r=fwrite(table,sizeof(long),TABLE_SIZE,file);
    checkError(r,0,"updateTable: error in fwrite\n");
    fclose(file);
}



//FUNCTIONS OVER DOGS

//Calculates the number of dogs
long getNumDogs(){
    FILE *file;
    file = fopen("dataDogs.dat", "rb+");
    if (file==NULL){
        return 0;
    }
    fseek(file, 0L, SEEK_END);
    long sz = ftell(file);
    fclose(file);
    return sz/sizeof(struct dogType);
}

//Calculates the ID of the last dog, if the last dog was deleted its ID is reused
int getCurrId(){
    struct dogType *currentDog;
    currentDog=malloc(sizeof(struct dogType));
    if (currentDog==NULL){
        perror("getCurrId:error in malloc\n");
        exit(-1);
    }
    FILE *file;
    file = fopen("dataDogs.dat", "rb+");
    if (file==NULL){
        return 0;
    }else{
        int r=fseek(file,0L, SEEK_END);
        checkError(r,-1,"getCurrId: error in fseek 1\n");
        if(ftell(file)==0){
            return 0;
        }
        fseek(file, (long)-sizeof(struct dogType), SEEK_END);
        checkError(r,-1,"getCurrId: error in fseek 2\n");
        r=fread(currentDog,sizeof(struct dogType),1,file);
        checkError(r,0,"getCurrId:error in fread\n");
        fclose(file);
        int currentId=currentDog->id;
        free(currentDog);
        return currentId;
    }
}

//Recursive function, for simplicity call checkDog instead
int recursiveCheckDog(int id, int first, int last,struct dogType *dog,FILE *file){
    if (last >= first){
        int mid = first + (last-first)/2;
        int r=fseek(file, mid*sizeof(struct dogType),SEEK_SET);
        checkError(r,-1,"recursiveCheckDog: error in fseek\n");
        r=fread(dog, sizeof(struct dogType),1,file);
        checkError(r,-1,"recursiveCheckDog: error in fread\n");
        if (dog->id == id){
            return dog->addr;
        }
        if (dog->id > id){
            return recursiveCheckDog(id, first, mid-1,dog,file);
        }else{
            return recursiveCheckDog(id, mid+1, last,dog,file);
        }
    }
    return -1;
}

//Checks if a dog exists depending on its ID, if it exists returns its address in the file
int checkDog(int id){
    struct dogType *dog;
    dog = malloc(sizeof(struct dogType));
    if(dog==NULL){
        perror("checkDog:error in malloc\n");
    }
    FILE *file;
    file=fopen("dataDogs.dat","rb");
    if(file==NULL){
        return -1;
    }
    int last=getNumDogs()-1;
    int address=recursiveCheckDog(id,0,last,dog,file);
    fseek(file, sizeof(struct dogType),SEEK_SET);
    fclose(file);
    free(dog);
    return address;
}

//Returns the dog which was detected by its ID
struct dogType* getDogById(int id){
    struct dogType *dog;
    dog = malloc(sizeof(struct dogType));
    if(dog==NULL){
        perror("getDogById:error in malloc\n");
        exit(-1);
    }
    int pos = checkDog(id);
    if (pos==-1){
        dog->id=-1;
    }else{
        FILE *file;
        file=fopen("dataDogs.dat","rb");
        int r = fseek(file, pos, SEEK_SET);
        checkError(r,-1,"getDogById:error in fseek\n");
        r=fread(dog,sizeof(struct dogType),1,file);
        checkError(r,0,"getDogById:error in fread\n");
        fclose(file);
    }
    return dog;
}

//Takes an address where there is a dog and overrides it in file with the given dog
void overrideDog(FILE *file,long address, struct dogType *dog){
    if (file==NULL){
        perror("overrideDog: error in fopen\n");
        exit(-1);
    }
    int r=fseek(file, address,SEEK_SET);
    checkError(r,-1,"overrideDog: error in fseek 1\n");
    r=fwrite(dog,sizeof(struct dogType),1,file);
    checkError(r,0,"overrideDog: error in fwrite\n");
    r=fseek(file,0, SEEK_CUR);
    checkError(r,-1,"overrideDog: error in fseek 2\n");
}

//Updates every "next" value for all dogs that have the same name as newDog
void sameNameUpdate(long *table, int hashIndex, long newDogAddr){
    //hash index is the value returned by the hash function after applying it to newDog name lowercased
    FILE *file;
    file = fopen("dataDogs.dat", "rb+");
    long sameNameAddr=*(table+hashIndex);
    struct dogType *sameNameDog;
    sameNameDog=malloc(sizeof(struct dogType));
    while(true){
        int r=fseek(file, sameNameAddr,SEEK_SET);
        checkError(r,-1,"sameNameUpdate: error in fseek\n");
        r=fread(sameNameDog,sizeof(struct dogType),1,file);
        checkError(r,0,"sameNameUpdate: error in fread\n");
        if(sameNameDog->next==-1){
            sameNameDog->next=newDogAddr;
            overrideDog(file,sameNameAddr,sameNameDog);
            break;
        }else{
            sameNameAddr=sameNameDog->next;
        }
    }
    fclose(file);
    free(sameNameDog);
}

//Shows all the registered dogs with the name searched by the user
void getListOfDogs(long *table, int hashIndex,char *lowerName,int clientfd){
    //hash index is the value returned by the hash function after applying it to newDog name lowercased
    FILE *file;
    file = fopen("dataDogs.dat", "rb+");
    long sameNameAddr=*(table+hashIndex);
    struct dogType *sameNameDog;
    int r;
    sameNameDog=malloc(sizeof(struct dogType));
    while(true){
        int r=fseek(file, sameNameAddr,SEEK_SET);
        checkError(r,-1,"getListOfDogs: error in fseek\n");
        r=fread(sameNameDog,sizeof(struct dogType),1,file);
        checkError(r,0,"getListOfDogs: error in fread\n");
        char *nameTemp=malloc(32);
        strcpy(nameTemp,sameNameDog->nombre);
        if(strcmp(lowerName,strlwr(nameTemp))==0){
            //sends dogs one by one
            r=total_send(clientfd,&sameNameDog->id,sizeof(sameNameDog->id));
            checkError(r,-1,"error en send");    
            r=total_send(clientfd,&sameNameDog->nombre,sizeof(sameNameDog->nombre));
            checkError(r,-1,"error en send");    
        }
        free(nameTemp);
        if(sameNameDog->next==-1){
            break;
        }else{
            sameNameAddr=sameNameDog->next;
        }
    }
    int stop=-1;
    char fakename[32]="x";
    //sends fake dog to identify end of message
    r=send(clientfd,&stop,sizeof(stop),0);
    checkError(r,-1,"error en send");
    r=send(clientfd,fakename,sizeof(fakename),0);
    checkError(r,-1,"error en send");
    fclose(file);
    free(sameNameDog);
}



//CASES FUNCTIONS
//Case 2: Functions about medical records

//checks if a record exists
bool newRecord(int id){
    char *path;
    path=malloc(15*sizeof(char));
    sprintf(path,"%d.txt",id);
    int ac=access(path, F_OK);
    free(path);
    if (ac==-1){
        return true;
    }else{
        return false;
    }
}

//creates a record when it doesn't exist
void createRecord(int id,struct dogType *dog){
    FILE *file;
    char *path;
    path=malloc(15*sizeof(char));
    sprintf(path,"%d.txt",id);
    file=fopen(path, "ab+");
    fprintf(file,"MECICAL HISTORY\n");
    fprintf(file,"Name: %s\nType: %s\nAge: %d\nBreed: %s\nHeight: %d\nWeight: %f\nGender: %c\nNEXT: %ld\nADDRESS: %ld",dog->nombre, dog->tipo, dog->edad, dog->raza, dog->estatura, dog->peso, dog->sexo,dog->next,dog->addr);
    fclose(file);
    free(path);
}

//send a record to client
void sendRecord(int clientfd, int id){
    int r;
    char *path;
    path=malloc(15*sizeof(char));
    sprintf(path,"%d.txt",id);
    int fd=open(path,O_RDONLY);
    int file_size=getFileSize(fd);
    r=send(clientfd,&file_size,sizeof(file_size),0);
    checkError(r,-1,"error en send");
    r=sendfile(clientfd,fd,NULL,file_size);
    checkError(r,-1,"error en sendfile");
    free(path);
    close(fd);
}

int writeRecord(int fd, void *auxBuffer, int file_size){
    int r;
    int written = 0;
    while(written < file_size){
        //writes record from buffer,into new file 
        r=write(fd,auxBuffer + written,file_size - written);
        checkError(r,-1,"error en write");
        written+=r;
    }
    return written;
}



//CASE 3: Erase a register, returns 0 if succesfull, -1, -2, -3 otherwise
int deleteRegister(int id,long *table, int clientfd){
    FILE *file;
    FILE *file_temp;
    int r,returnValue;
    int found = 0;
    struct dogType *registro;
    char *message;
    registro=malloc(sizeof(struct dogType));
    file=fopen("dataDogs.dat", "rb");
    if (file==NULL){
        //Sends value to indicate error
        returnValue=-1;
        r=send(clientfd,&returnValue,sizeof(returnValue),0);
        checkError(r,-1,"error en send");
        return returnValue;
    }else{
        file_temp=fopen("tmp.dat", "wb");
        if (!file_temp){
            //Sends value to indicate error
            returnValue=-2;
            r=send(clientfd,&returnValue,sizeof(returnValue),0);
            checkError(r,-1,"error en send");
            return returnValue;
        }
        registro=getDogById(id);
        if(registro->id==-1){
            //Sends value to indicate error
            returnValue=-3;
            r=send(clientfd,&returnValue,sizeof(returnValue),0);
            checkError(r,-1,"error en send");
            return -returnValue;
        }
        //Sends value to indicate success
        returnValue=0;
        r=send(clientfd,&returnValue,sizeof(returnValue),0);
        checkError(r,-1,"error en send");
        r=send(clientfd,registro,sizeof(struct dogType),0);
        checkError(r,-1,"error en send");
        long deleteNext=registro->next;
        long deleteAddr=registro->addr;
        long currentNext;
        while(fread(registro, sizeof(struct dogType),1,file) != 0){
            char *lowerName=malloc(32*sizeof(char));
            strcpy(lowerName,registro->nombre);
            strlwr(lowerName);
            int hashIndex=hashFunction(lowerName);
            free(lowerName);
            currentNext=registro->next;
            if(currentNext!=-1 && currentNext>deleteAddr){
                registro->next=currentNext-sizeof(struct dogType);
            }else if(currentNext==deleteAddr){
                if(deleteNext==-1){
                    registro->next=-1;
                }else{
                    registro->next=deleteNext - sizeof(struct dogType);
                }
            }
            if (id==registro->id){
                //updating hash table
                if(deleteAddr==*(table+hashIndex)){   //if deleteAddr is the first ocurrence of the name
                    *(table+hashIndex)=deleteNext;
                }
                found=1;
                char *path;
                path=malloc(15*sizeof(char));
                sprintf(path,"%d.dat",id);
                remove(path);
                free(path);
            }else{
                if(found==1){
                    if(registro->addr==*(table+hashIndex)){
                        *(table+hashIndex)=*(table+hashIndex)-sizeof(struct dogType);
                    }
                    registro->addr=registro->addr-sizeof(struct dogType);
                }
                fwrite(registro, sizeof(struct dogType), 1, file_temp);
            }
        }
        fclose(file_temp);
        remove("dataDogs.dat");
        rename("tmp.dat","dataDogs.dat");
        free(registro);
        return 0;
    }
    fclose(file);
}



//LOG FUNCTIONS

//writes a log giving specific information about the option selected by client
void makeLog(char operation[32],struct in_addr ip,char searchedString[32]){
    //A log register is created
    struct log *myLog;
    myLog = (struct log*) malloc (sizeof(struct log));
    //this code is necessary to get the current time
    struct tm *tm;
    time_t t;
    t=time(NULL);
    tm=localtime(&t);
    char fechayhora[100];
    strftime(fechayhora,100, "%Y/%m/%d-%H:%M:%S\0 ", tm); //here the format is given
    char fecha [20];
    for (int i=0; i<sizeof(fecha); i++){
        fecha[i]=fechayhora[i];
    }
    strcpy(myLog->date,fecha);
    strcpy(myLog->operation,operation);
    strcpy(myLog->searchedString, searchedString);
    //The first log register is read and shown by the server->This code is not important
    FILE *file;
    file = fopen("serverDogs.log", "ab+");
    fprintf(file,"%s", myLog->date);
    fprintf(file," %s ", inet_ntoa(ip));
    fprintf(file,"%s", myLog->operation);
    fprintf(file,"%s\n", myLog->searchedString);
    fclose(file);
    free(myLog);
}

void readLog(){
    struct log *readLog;
    readLog = (struct log*) malloc (sizeof(struct log));
    FILE *file;
    file=fopen("serverDogs.dat", "rb");
    int r=fread(readLog,sizeof(struct log),1,file);
    checkError(r,0,"readLog:error in fread\n");
    fclose(file);
    printf("La fecha guardada es: %s\n", readLog->date);
    printf("%s\n", readLog->clientIp);
    printf("%s\n", readLog->operation);
    printf("%s\n", readLog->searchedString);
    free(readLog);
    fclose(file);
}



//THREAD FUNCTION
void *thread(void *ap){
    struct dogType *dog;
    char *lowerName;
    char name[32];
    long numberOfDogs,nextId;
    int option, quantity, reg, id,r,success, hashIndex;
    struct Argument *argument= (struct Argument*) ap;
    int clientfd=argument->clientfd;
    struct sockaddr_in client=argument->client;
    long *table=argument->table;
    //CAMBIO//
    int idThread=argument->idThread;
    //CAMBIO//
    checkError(r,-1,"error en el recv");
    do{
        //receive option
        r=recv(clientfd, &option, sizeof(option), 0);
        fflush(stdout);
        switch(option){

            //CASE 1: ENTER A REGISTER
            case 1:
                ;//semicolon avoids nasty bug
                FILE *file;
                dog=malloc(sizeof(struct dogType));
                //receives pet's information
                r=recv(clientfd, dog, sizeof(struct dogType), 0);
                checkError(r,-1,"Case 1: error in recv");

                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE1-START: escritura datadogs->ingresan datos\n");
                file = fopen("dataDogs.dat", "ab+");
                if (file==NULL){
                    perror("Case1: error in fopen\n");
                    exit(-1);
                }
                int r=fseek(file,0L,SEEK_END);
                checkError(r,-1,"Case 1: error in fseek\n");
                //getting the address of newDog in file
                long dogAddr=ftell(file);
                checkError(dogAddr,-1,"Case1: error in ftell\n");
                dog->addr=dogAddr;
                dog->id=getCurrId()+1;
                //saves information on "dataDogs.dat"
                r=fwrite (dog, sizeof(struct dogType), 1, file);
                checkError(r,0,"Case1: error in fwrite\n");
                fclose(file);
                exit_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE1-END: escritura datadogs->ingresan datos\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                //updates links of the pets with the same name
                lowerName=toLowerCase(dog->nombre,32);
                int hashIndex=hashFunction(lowerName);

                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&hash_sem, &hash_mutex, hash_pipe);
                printf("CASE1-START: lectura tabla hash\n");
                if(*(table+hashIndex)==-1){
                    //There's no collision
                    *(table+hashIndex)=dogAddr;
                }else{
                    
                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                    printf("CASE1-START: escritura datadogs->actualiza enlaces\n");
                    sameNameUpdate(table,hashIndex,dogAddr);
                    exit_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                    printf("CASE1-END: escritura datadogs->actualiza enlaces\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                }
                exit_lock(&hash_sem, &hash_mutex, hash_pipe);
                printf("CASE1-END: lectura tabla hash\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                free(lowerName);
                //sends "x" as a sucessful message
                r=send(clientfd,"x",1,0);
                checkError(r,-1,"error en el send");
                //makes a log register

                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&log_sem, &log_mutex, log_pipe);
                printf("CASE1-START: escritura log\n");
                makeLog("Insert ",client.sin_addr,dog->nombre);
                exit_lock(&log_sem, &log_mutex, log_pipe);
                printf("CASE1-END: escritura log\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                free(dog);
                printf("---------------------------------------------\n");
            break;

            //CASE 2: SEE A MEDICAL HISTORY
            case 2:
                ;
                //flag gives info about wanted medical record
                int flag;
                //size of record file
                int file_size;

                //CAMBIO//
                int flagMH;
                //CAMBIO
                
                //sends number of pets

                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE2-START: lectura datadogs->cantidad perros\n");
                numberOfDogs=getNumDogs();
                exit_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE2-END: lectura datadogs->cantidad perros\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                r=send(clientfd,&numberOfDogs,sizeof(numberOfDogs), 0);
                checkError(r,-1,"error en send");
                //receives id of pet
                r=recv(clientfd,&id,sizeof(id),0);
                checkError(r,-1,"error en recv");
                struct dogType *dog=malloc(sizeof(struct dogType));

                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE2-START: lectura datadogs->obtener perro\n");
                dog=getDogById(id);
                exit_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE2-END: lectura datadogs->obtener perro\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////


                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&array_sem, &array_mutex, array_pipe);
                printf("CASE2-START: lectura arreglo\n");
                for (int i=0;i<BACKLOG;i++){
                    if (id==arrMH[i]){
                        flagMH=1;
                        break;
                    }else{
                        flagMH=0;
                    }
                }

                exit_lock(&array_sem, &array_mutex, array_pipe);
                printf("CASE2-END: lectura arreglo\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                //CAMBIO//
                if(flagMH==1){
                    r=send(clientfd,&flagMH, sizeof(flagMH), 0);
                    checkError(r,-1,"error en send");

                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&array_sem, &array_mutex, array_pipe);
                    printf("CASE2-START: escritura arreglo->no disponible\n");
                    arrMH[idThread]=-1;
                    exit_lock(&array_sem, &array_mutex, array_pipe);
                    printf("CASE2-END: escritura arreglo->no disponible\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////
                }else{

                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&array_sem, &array_mutex, array_pipe);
                    printf("CASE2-START: escritura arreglo\n");
                    arrMH[idThread]=id;
                    exit_lock(&array_sem, &array_mutex, array_pipe);
                    printf("CASE2-END: escritura arreglo\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                    r=send(clientfd,&flagMH, sizeof(flagMH), 0);
                    checkError(r,-1,"error en send");

                    if(dog->id!=-1 && newRecord(id) == true){
                        //creates record and sends it to client
                        flag=1;
                        r=send(clientfd,&flag, sizeof(flag), 0);
                        checkError(r,-1,"error en send");
                        createRecord(id, dog);
                        sendRecord(clientfd,id);
                    }else if(dog->id!=-1){
                        //sends existing record to client
                        flag=2;
                        r=send(clientfd,&flag, sizeof(flag), 0);
                        checkError(r,-1,"error en send");
                        sendRecord(clientfd,id);
                    }else{
                        //there is no record
                        flag=-1;
                        r=send(clientfd,&flag, sizeof(flag), 0);
                        checkError(r,-1,"error en send");
                    }

                    //server recvs info about wether the client made changes to record
                    r=recv(clientfd,&flag, sizeof(flag), 0);
                    checkError(r,-1,"error en el rcv");

                    if(flag==1){
                        //changes were made
                        //gets size of record
                        r=recv(clientfd, &file_size, sizeof(file_size),0);
                        checkError(r,-1,"error en el rcv");
                        //array of strings, temporaly stores content of record
                        char auxBuffer[file_size + 1];
                        //opens record in client side
                        char *path;
                        path=malloc(15*sizeof(char));
                        sprintf(path,"%d.txt",id);
                        int fd=open(path,O_RDWR);
                        //stores record in auxiliary buffer
                        total_recv(clientfd,auxBuffer,file_size);
                        // puts '\0' to ensure end of record
                        auxBuffer[sizeof(auxBuffer)-1]='\0';
                        //writes record in file
                        writeRecord(fd, auxBuffer,file_size);
                        close(fd);
                        free(path);
                    }

                    
                    char searched[32];
                    sprintf(searched,"%d",id);

                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&array_sem, &array_mutex, array_pipe);
                    printf("CASE2-START: escritura arreglo->limpieza\n");
                    arrMH[idThread]=-1;
                    exit_lock(&array_sem, &array_mutex, array_pipe);
                    printf("CASE2-END: escritura arreglo->limpieza\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&log_sem, &log_mutex, log_pipe);
                    printf("CASE2-START: escritura log\n");
                    makeLog("Read ", client.sin_addr,searched);
                    exit_lock(&log_sem, &log_mutex, log_pipe);
                    printf("CASE2-END: escritura log\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////
                }

                //CAMBIO//
                free(dog);
                printf("---------------------------------------------\n");
            break;

            //CASE 3: DELETE A REGISTER
            case 3:
                ;
                char strid[10];

                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE3-START: lectura datadogs->cantidad perros\n");
                numberOfDogs=getNumDogs();
                exit_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE2-END: lectura datadogs->cantidad perros\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                //sends number of pets
                r=send(clientfd,&numberOfDogs,sizeof(numberOfDogs),0);
                checkError(r,-1,"error en el send");
                //receives id of pet
                r=recv(clientfd, &id, sizeof(id), 0);
                checkError(r,-1,"error en recv");
                //sends pet's information


                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&hash_sem, &hash_mutex, hash_pipe);
                printf("CASE3-START: lectura/escritura tabla hash\n");

                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE3-START: lectura/escritura datadogs\n");
                success=deleteRegister(id,table,clientfd);
                exit_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                printf("CASE3-END: lectura/escritura datadogs\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                exit_lock(&hash_sem, &hash_mutex, hash_pipe);
                printf("CASE3-END: lectura/escritura tabla hash\n");
                ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////
                
                
                if(success==0){
                    sprintf(strid,"%d",id);
                    fflush(stdout);

                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&log_sem, &log_mutex, log_pipe);
                    printf("CASE3-START: escritura log\n");
                    makeLog("Delete ",client.sin_addr,strid);
                    exit_lock(&log_sem, &log_mutex, log_pipe);
                    printf("CASE3-END: escritura log\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                }
                //sends a value that means success
                r=send(clientfd,&success,sizeof(success),0);
                checkError(r,-1,"error en el send");
                printf("---------------------------------------------\n");
            break;

            //CASE 4: LIST ALL DOGS WITH THE SEARCHED NAME
            case 4:
                ;//semicolon avoids nasty bug
                //receives name
                r=recv(clientfd,name,sizeof(name),0);
                checkError(r,-1,"error en el recv");
                lowerName=toLowerCase(name,32);
                hashIndex=hashFunction(lowerName);
                
                ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                enter_lock(&hash_sem, &hash_mutex, hash_pipe);
                printf("CASE4-START: lectura tabla hash\n");

                if(*(table+hashIndex)==-1){
                    exit_lock(&hash_sem, &hash_mutex, hash_pipe);
                    printf("CASE4-END: lectura tabla hash\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////
                    
                    //Sends flag to indicate dog wasn't found
                    success=-1;
                    r=send(clientfd,&success,sizeof(success),0);
                    checkError(r,-1,"error en el send");

                }else{
                    exit_lock(&hash_sem, &hash_mutex, hash_pipe);
                    printf("CASE4-END: lectura tabla hash\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////
                    
                    success=0;
                    //Sends flag to indicate dog was found
                    r=send(clientfd,&success,sizeof(success),0);
                    checkError(r,-1,"error en el send");
                    
                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                    printf("CASE4-START: lectura datadogs\n");
                    getListOfDogs(table,hashIndex,lowerName,clientfd);
                    exit_lock(&dataDogs_sem, &dataDogs_mutex, dataDogs_pipe);
                    printf("CASE4-END: lectura datadogs\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                    name[r+1]='\0';

                    ////////////////////////////////START OF CRITICAL SECTION////////////////////////////////
                    enter_lock(&log_sem, &log_mutex, log_pipe);
                    printf("CASE4-START: escritura log\n");
                    makeLog("Search ",client.sin_addr,name);
                    exit_lock(&log_sem, &log_mutex, log_pipe);
                    printf("CASE4-END: ESCRITURA LOG\n");
                    ////////////////////////////////END OF CRITICAL SECTION////////////////////////////////

                }
                
                free(lowerName);
                printf("---------------------------------------------\n");
            break;
        }
    } while (option!=5);
    updateTable(table);
}






//MAIN FUNCTION WHERE THE PROGRAM MAKES EVERYTHING
int main(){

    //CAMBIO//
    for (int i=0;i<BACKLOG;i++){
        arrMH[i]=-1;
    }
    //CAMBIO//
    
    //init locks
    int r;
    r=init_lock(&dataDogs_sem,&dataDogs_mutex,dataDogs_pipe);
    checkError(r,-1,"error init datadogs lock");
    r=init_lock(&hash_sem,&hash_mutex,hash_pipe);
    checkError(r,-1,"error init datadogs lock");
    r=init_lock(&log_sem,&log_mutex,log_pipe);
    checkError(r,-1,"error init log lock");
    r=init_lock(&array_sem,&array_mutex,array_pipe);
    checkError(r,-1,"error init array lock");
    
    //Initialize hash table
    long *table;
    table=malloc(TABLE_SIZE*sizeof(long));
    initTable(table);
    //variables for trheads
    pthread_t tfd[BACKLOG];
    struct Argument argument[32];
    fflush(stdout);
    //NECESSARY CODE TO MAKE CONEXION WITH A CLIENT
    //necessary variables to make the conexion
    int serverfd;
    int clientsfd[BACKLOG];
    struct sockaddr_in server;
    struct sockaddr_in clients[BACKLOG];
    char option[2];
    char searched[33];
    socklen_t len;
    //socket
    serverfd=socket(AF_INET, SOCK_STREAM, 0);
    checkError(serverfd,-1,"error en el socket");
    //server's structure arguments
    server.sin_family=AF_INET;
    server.sin_port = htons (PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero((server.sin_zero), 8);
    len = sizeof(struct sockaddr_in);
    //setsockopt:to reuse the socket
    int sock_option=1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &sock_option, sizeof(sock_option));
    r=bind(serverfd,(struct sockaddr*)&server, len);
    checkError(r,-1,"error en el bind");
    //the server "listens"
    r=listen(serverfd,BACKLOG);
    checkError(r,-1,"error en el listen");

    pthread_attr_t thread_params;
    r=pthread_attr_init(&thread_params);
    r=pthread_attr_setstacksize(&thread_params, 1024 * 16);
    r=pthread_attr_setdetachstate(&thread_params, PTHREAD_CREATE_DETACHED);

    for(int i=0;i<BACKLOG;i++){
        //the server acepts the conexion with a client
        clientsfd[i]=accept(serverfd,(struct sockaddr*)(clients+i), &len);
        checkError(clientsfd[i],-1,"error en el accept");
        //init thread
        argument[i].client=clients[i];
        argument[i].clientfd=clientsfd[i];
        argument[i].table=table;

        //CAMBIO//
        argument[i].idThread=i;
        //CAMBIO//

        pthread_create(&tfd[i],&thread_params,thread,&argument[i]);
    }

    close(serverfd);
    free(table);
    r=destroy_lock(&dataDogs_sem,&dataDogs_mutex,dataDogs_pipe);
    checkError(r,-1,"error destroy datadogs lock");
    r=destroy_lock(&hash_sem,&hash_mutex,hash_pipe);
    checkError(r,-1,"error init datadogs lock");
    r=destroy_lock(&log_sem,&log_mutex,log_pipe);
    checkError(r,-1,"error destroy log lock");
    r=destroy_lock(&array_sem,&array_mutex,array_pipe);
    checkError(r,-1,"error destroy array lock");
    return 0;
}