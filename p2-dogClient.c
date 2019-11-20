#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>

#define PORT 3535
#define BACKLOG 2
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
//Waits for a key to be pressed
int keypress(unsigned char echo){
    struct termios savedState, newState;
    int c;
    if (-1 == tcgetattr(STDIN_FILENO, &savedState)){
        return EOF;     /* error on tcgetattr */
    }
    newState = savedState;
    if ((echo = !echo)){ /* yes i'm doing an assignment in an if clause */
        echo = ECHO;    /* echo bit to disable echo */
    }
    /* disable canonical input and disable echo.  set minimal input to 1. */
    newState.c_lflag &= ~(echo | ICANON);
    newState.c_cc[VMIN] = 1;
    if (-1 == tcsetattr(STDIN_FILENO, TCSANOW, &newState)){
        return EOF;     /* error on tcsetattr */
    }
    c = getchar();      /* block (withot spinning) until we get a keypress */
    /* restore the saved state */
    if (-1 == tcsetattr(STDIN_FILENO, TCSANOW, &savedState)){
        return EOF;     /* error on tcsetattr */
    }
    return c;
}
//Continues after a key is pressed
void toContinue(){
    printf("Press any key to continue\n");
    char ch;
    scanf("%c", &ch);
    keypress(0);
}



//CASES FUNCTIONS
//CASE 1: Enter a new animal
struct dogType* enterAnimal(){
    struct dogType *data;
    data = (struct dogType*) malloc (sizeof(struct dogType));
    if (data==NULL){
        perror("enterAnimal: error in malloc\n");
        exit(-1);
    }
    //ID assignment -1 by default
    data->id=-1;
    //By default the "next" address is -1
    data->next=-1;
    //Enter Data
    printf("Name: ");
    scanf("%s", data->nombre);
    printf("Type: ");
    scanf("%s", data->tipo);
    printf("Age: ");
    scanf("%i", &data->edad);
    printf("Breed: ");
    scanf("%s", data->raza);
    printf("Height: ");
    scanf("%i", &data->estatura);
    printf("Weight: ");
    scanf("%f", &data->peso);
    printf("Gender: ");
    scanf(" %c", &data->sexo);
    return data;
}
//CASE 2: Open the medical history in a text editor
void openEditor(int id,struct dogType *dog){
    char *path;
    path=malloc(15*sizeof(char));
    sprintf(path,"%d.dat",id);
    FILE *file;
    int ac=access(path, F_OK);
    if (ac==-1){
        printf("Medical History is opened for the first time\n");
        if(dog->id!=-1){
            //perro existe
            char *path;
            path=malloc(15*sizeof(char));
            sprintf(path,"%d.dat",id);
            file=fopen(path, "ab+");
            fprintf(file,"MECICAL HISTORY\n");
            fprintf(file,"Name: %s\nType: %s\nAge: %d\nBreed: %s\nHeight: %d\nWeight: %f\nGender: %c\nNEXT: %ld\nADDRESS: %ld",dog->nombre, dog->tipo, dog->edad, dog->raza, dog->estatura, dog->peso, dog->sexo,dog->next,dog->addr);
            fclose(file);
            free(path);
            free(dog);
        }else{
            printf("The requested record doesn't exist\n");
            free(dog);
        }
        if(!fork()){
            execlp("gedit", "gedit", path, NULL);
        }
    }else{
        printf("The medical history was opened before\n");
        if(!fork()){
            execlp("gedit", "gedit", path, NULL);
        }
    }
    free(path);
    toContinue();
}





//MAIN FUNCTION WHERE THE PROGRAM MAKES EVERYTHING
int main(int argc, char const *argv[]){
    struct dogType *dog;
    long numberOfDogs,nextId;
    int option, quantity, reg, id,success;
    int serverfd,r;
    struct sockaddr_in server;
    socklen_t len;
    char message[200];
    char flag;

    serverfd=socket(AF_INET, SOCK_STREAM, 0);
    checkError(serverfd,-1,"error en el socket");
    server.sin_family=AF_INET;
    server.sin_port = htons (PORT);
    server.sin_addr.s_addr = inet_addr(argv[1]);
    bzero((server.sin_zero), 8);
    len = sizeof(struct sockaddr);

    r=connect(serverfd,(struct sockaddr*)&server, len);
    checkError(r,-1,"error en el connect");
    
    do{
        printf("\nWelcome to PET FAMILY, your favorite veterinary");
        printf("\n 1. Enter a record");
        printf("\n 2. See a record");
        printf("\n 3. Delete a record");
        printf("\n 4. Search records by name");
        printf("\n 5. Exit the system\n");
        scanf("%d", &option);
        //SEND OPTION
        r=send(serverfd,&option,sizeof(option),0);
        checkError(r,-1,"error en el send");
        switch(option){

            //CASE 1: ENTER A REGISTER
            case 1:
                printf("Enter a record\n");
                dog=malloc(sizeof(struct dogType));
                dog = enterAnimal();
                dog->addr=-1;
                //sends pet's information
                r=send(serverfd,dog,sizeof(struct dogType),0);
                checkError(r,-1,"error en el send");
                free(dog);
                //receives success message
                r=recv(serverfd,&flag,1,0);
                checkError(r,-1,"error en el recv");
                printf("Animal successfully added\n");
                toContinue();
            break;

            //CASE 2: SEE A MEDICAL HISTORY
            case 2:
                dog=malloc(sizeof(struct dogType));
                //receives number of pets
                r=recv(serverfd,&numberOfDogs,sizeof(numberOfDogs),0);
                printf("The number of records is: %ld\n",numberOfDogs);
                printf("Enter the ID of the desired animal to see its medical history: ");
                scanf("%i",&id);
                //sends id of pet
                r=send(serverfd,&id,sizeof(id),0);
                //receives pet's information
                r=recv(serverfd, dog, sizeof(struct dogType), 0);
                //open a doc on GEDDIT to see the pet's medical history
                if(dog->id!=-1){
                    printf("Would you like to see the medical history? (Y/N): \n");
                    char ans;
                    scanf(" %c",&ans);
                    if(ans=='Y'){
                        openEditor(id,dog);
                    }else{
                        toContinue();
                    }
                }else{
                    printf("The requested record doesn't exist\n");
                    toContinue();
                }
                free(dog);
            break;

            //CASE 3: DELETE A REGISTER
            case 3:
                //receives num of pets
                r=recv(serverfd, &numberOfDogs, sizeof(numberOfDogs), 0);
                checkError(r,-1,"error en el recv");
                printf("The number of records is: %ld\n",numberOfDogs);
                printf("Enter the ID of the animal you would like to delete: ");
                scanf("%i",&id);
                //sends id of pet
                r=send(serverfd,&id,sizeof(id),0);
                checkError(r,-1,"error en el send");
                //receives a flag that means success
                r=recv(serverfd, &success, sizeof(success), 0);
                printf("flag recieved: %d\n",success);
                switch (success){
                    case 0:
                        ; //for avoiding bug
                        struct dogType *registro=malloc(sizeof(struct dogType));
                        r=recv(serverfd, registro, sizeof(struct dogType),0);
                        printf("number of recieved bytes: %d\n",r);
                        printf("DELETED DOG INFORMATION:\nName: %s\nType: %s\nAge: %d\nBreed: %s\nHeight: %d\nWeight: %f\nGender: %c\nPlease wait, deleting...\n",
                        registro->nombre, registro->tipo, registro->edad, registro->raza, registro->estatura, registro->peso,registro->sexo);
                        free(registro);
                    break;
                    case -1:
                        printf("There are no records\n");
                    break;
                    case -2:
                        printf("Impossible to create the new file\n");
                    break;
                    case -3:
                        printf("Impossible to find a record with the requested ID\n");
                    break;
                    default:
                    break;
                }
                //receives a flag that means the deletion is done    
                r=recv(serverfd, &success, sizeof(success), 0);
                if(success==0)
                    printf("The record with the requested ID was successfully found and deleted\n");
                toContinue();
            break;   

            //CASE 4: LIST ALL DOGS WITH THE SEARCHED NAME
            case 4:
                printf("Enter a name to see all the pets registered with that name: ");
                char name[32];
                int id;
                scanf("%s",name);
                //sends name
                r=send(serverfd,name,sizeof(name),0);
                checkError(r,-1,"error en el send");
                //receives a flags that tells if the dog was found
                r=recv(serverfd,&success,sizeof(r),0);
                checkError(r,-1,"error en el rcv");
                switch (success){
                    case -1:
                        printf("There are no records with that name\n");
                    break;
                    case 0:
                        do{
                            r=recv(serverfd,&id,sizeof(id),0);
                            checkError(r,-1,"error en send");
                            r=recv(serverfd,name,sizeof(name),0);
                            checkError(r,-1,"error en send");
                            if(id!=-1)
                                printf("ID: %d, Name %s\n",id,name);
                        } while (id!=-1);
                    break;
                    default:
                    break;
                }
                r=recv(serverfd,&flag,1,0);
                checkError(r,-1,"error en el send");
                toContinue();
            break;
        }
    }while(option!=5);
    close (serverfd);
    return 0;
}