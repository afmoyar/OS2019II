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

#define PORT 3535
#define BACKLOG 32
#define TABLE_SIZE 999983
#define MESSAGE_SIZE 200



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



//HASH TABLE FUNCTIONS
//Fills the array with a given value
void fillArray(long *array,int size,int value){
    for(int i=0;i<size;i++){
        *(array+i)=value;
    }
}
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
        if(file==NULL) {perror("initTable: error in fopen when doesn't exist\n"); exit(-1);}
        int r=fwrite(table,sizeof(long),TABLE_SIZE,file);
        checkError(r,0,"initTable: error in fwrite\n");
        fclose(file);
    }
    file=fopen("hashTable.dat","rb");
    if(file==NULL) {perror("initTable: error in fopen\n"); exit(-1);}
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
    //char *buffer=malloc(MESSAGE_SIZE);
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
            printf("ID: %d, Name %s\n",sameNameDog->id,sameNameDog->nombre);
            //sends dogs one by one
            r=send(clientfd,&sameNameDog->id,sizeof(sameNameDog->id),0);
            checkError(r,-1,"error en send");    
            r=send(clientfd,&sameNameDog->nombre,sizeof(sameNameDog->nombre),0);
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
    //sends fake dog to identify end of message
    r=send(clientfd,&stop,sizeof(stop),0);
    checkError(r,-1,"error en send");
    r=send(clientfd,"x",1,0);
    checkError(r,-1,"error en send");
    fclose(file);
    free(sameNameDog);
}



//CASES FUNCTIONS
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
    checkError(r,-1,"error en el recv");
    do{
        //receive option
        r=recv(clientfd, &option, sizeof(option), 0);
        switch(option){

            //CASE 1: ENTER A REGISTER
            case 1:
                ;//semicolon avoids nasty bug
                FILE *file;
                dog=malloc(sizeof(struct dogType));
                //receives pet's information
                r=recv(clientfd, dog, sizeof(struct dogType), 0);
                checkError(r,-1,"Case 1: error in recv");
                
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
                //updates links of the pets with the same name
                lowerName=toLowerCase(dog->nombre,32);
                int hashIndex=hashFunction(lowerName);
                if(*(table+hashIndex)==-1){
                    //There's no collision
                    *(table+hashIndex)=dogAddr;
                }else{
                    sameNameUpdate(table,hashIndex,dogAddr);
                }
                free(lowerName);
                //sends "x" as a sucessful message
                r=send(clientfd,"x",1,0);
                checkError(r,-1,"error en el send");
                //makes a log register
                makeLog("Insert ",client.sin_addr,dog->nombre);
                free(dog);
            break;

            //CASE 2: SEE A MEDICAL HISTORY
            case 2:
                ;
                //sends number of pets
                numberOfDogs=getNumDogs();
                r=send(clientfd,&numberOfDogs,sizeof(numberOfDogs), 0);
                //receives id of pet
                r=recv(clientfd,&id,sizeof(id),0);
                struct dogType *dog=malloc(sizeof(struct dogType));
                dog=getDogById(id);
                //sends pet's information
                r=send(clientfd,dog,sizeof(struct dogType),0);
                char searched[32];
                sprintf(searched,"%d",id);
                makeLog("Read ", client.sin_addr,searched);
                free(dog);
            break;

            //CASE 3: DELETE A REGISTER
            case 3:
                ;
                char strid[10];
                numberOfDogs=getNumDogs();
                //sends number of pets
                r=send(clientfd,&numberOfDogs,sizeof(numberOfDogs),0);
                checkError(r,-1,"error en el send");
                //receives id of pet
                r=recv(clientfd, &id, sizeof(id), 0);
                printf("id recibido: %d: ",id);
                //sends pet's information
                success=deleteRegister(id,table,clientfd);
                if(success==0){
                    sprintf(strid,"%d",id);
                    fflush(stdout);
                    makeLog("Delete ",client.sin_addr,strid);
                }
                //sends a value that means success
                r=send(clientfd,&success,sizeof(success),0);
                checkError(r,-1,"error en el send");
            break;

            //CASE 4: LIST ALL DOGS WITH THE SEARCHED NAME
            case 4:
                ;//semicolon avoids nasty bug
                //receives name
                r=recv(clientfd,name,sizeof(name),0);
                checkError(r,-1,"error en el recv");
                lowerName=toLowerCase(name,32);
                hashIndex=hashFunction(lowerName);
                if(*(table+hashIndex)==-1){
                    //Sends flag to indicate dog wasn't found
                    success=-1;
                    r=send(clientfd,&success,sizeof(success),0);
                    checkError(r,-1,"error en el send");
                }else{
                    success=0;
                    //Sends flag to indicate dog was found
                    r=send(clientfd,&success,sizeof(success),0);
                    checkError(r,-1,"error en el send");
                    getListOfDogs(table,hashIndex,lowerName,clientfd);
                    name[r+1]='\0';
                    makeLog("Search ",client.sin_addr,name);
                }
                free(lowerName);
                //SEND INFO(por ahora solo es un numero sin importancia)
                r=send(clientfd,"x",1,0);
                checkError(r,-1,"error en el send");
            break;
        }
    } while (option!=5);
    updateTable(table);
}





//MAIN FUNCTION WHERE THE PROGRAM MAKES EVERYTHING
int main(){
    //Initialize hash table
    long *table;
    table=malloc(TABLE_SIZE*sizeof(long));
    initTable(table);
    //variables for trheads
    pthread_t tfd[BACKLOG];
    struct Argument argument[32];

    //NECESSARY CODE TO MAKE CONEXION WITH A CLIENT
    //necessary variables to make the conexion
    int serverfd,r;
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

    for(int i=0;i<BACKLOG;i++){
        //the server acepts the conexion with a client
        clientsfd[i]=accept(serverfd,(struct sockaddr*)(clients+i), &len);
        checkError(clientsfd[i],-1,"error en el accept");
        //init thread
        argument[i].client=clients[i];
        argument[i].clientfd=clientsfd[i];
        argument[i].table=table;
        pthread_create(&tfd[i],NULL,thread,&argument[i]);
    }
    for(int i=0;i<BACKLOG;i++){
        pthread_join(tfd[i],NULL);
        close(clientsfd[i]);
    }
    close(serverfd);
    free(table);
    return 0;
}