#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>

//#define  TABLE_SIZE 100189
#define  TABLE_SIZE 999983

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
void getListOfDogs(long *table, int hashIndex,char *lowerName){
    //hash index is the value returned by the hash function after applying it to newDog name lowercased
    FILE *file;
    file = fopen("dataDogs.dat", "rb+");
    long sameNameAddr=*(table+hashIndex);
    struct dogType *sameNameDog;
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
        }
        free(nameTemp);
        if(sameNameDog->next==-1){
            break;
        }else{
            sameNameAddr=sameNameDog->next;
        }
    }
    fclose(file);
    free(sameNameDog);
}




//CASES FUNCTIONS
//CASE 1: Enter a new animal
void enterAnimal(long *table){
    struct dogType *data;
    FILE *file;
    data = (struct dogType*) malloc (sizeof(struct dogType));
    if (data==NULL){
        perror("enterAnimal: error in malloc\n");
        exit(-1);
    }
    //ID assignment
    data->id=getCurrId()+1;
    printf("Animal's ID: %d \n",data->id);
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

    file = fopen("dataDogs.dat", "ab+");
    if (file==NULL){
        perror("enterAnimal: error in fopen\n");
        exit(-1);
    }
    int r=fseek(file,0L,SEEK_END);
    checkError(r,-1,"enterAnimal: error in fseek\n");

    //getting the address of newDog in file
    long dogAddr=ftell(file);
    checkError(dogAddr,-1,"enterAnimal: error in ftell\n");
    data->addr=dogAddr;

    r=fwrite (data, sizeof(struct dogType), 1, file);
    checkError(r,0,"enterAnimal: error in fwrite\n");
    fclose(file);
    char *lowerName=toLowerCase(data->nombre,32);
    int hashIndex=hashFunction(lowerName);
    if(*(table+hashIndex)==-1){
        //There's no collision
        *(table+hashIndex)=dogAddr;
    }else{
        sameNameUpdate(table,hashIndex,dogAddr);
    }
    free(data);
    free(lowerName);
    printf("Animal successfully added\n");
    toContinue();
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
//CASE 3: Erase a register
int deleteRegister(int id,long *table){
    FILE *file;
    FILE *file_temp;
    int found = 0;
    struct dogType *registro;
    registro=malloc(sizeof(struct dogType));
    file=fopen("dataDogs.dat", "rb");
    if (file==NULL){
        printf("There are no records\n");
        return -1;
    }else{
        file_temp=fopen("tmp.dat", "wb");
        if (!file_temp){
            printf("Impossible to create the new file\n");
            return -1;
        }
        registro=getDogById(id);
        if(registro->id==-1){
            printf("Impossible to find a record with the requested ID\n");
            return 0;
        }
        printf("DELETED DOG INFORMATION:\nName: %s\nType: %s\nAge: %d\nBreed: %s\nHeight: %d\nWeight: %f\nGender: %c\n",
        registro->nombre, registro->tipo, registro->edad, registro->raza, registro->estatura, registro->peso,registro->sexo);
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
                if(deleteAddr==*(table+hashIndex)){   //if delete address y the the first ocurrence of the name
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
        if (!found){
            printf("Impossible to find a record with the requested ID\n");
        }
        fclose(file_temp);
        printf("The record with the requested ID was successfully found and deleted\n");
        remove("dataDogs.dat");
        rename("tmp.dat","dataDogs.dat");
        free(registro);
        return 0;
    }
    fclose(file);
}




//MAIN METHOD
int main(){
    long *table;
    table=malloc(TABLE_SIZE*sizeof(long));
    initTable(table);
    long numberOfDogs,nextId;
    int option, quantity, reg, id;

    do{
        printf("\nWelcome to PET FAMILY, your favorite veterinary");
        printf("\n 1. Enter a record");
        printf("\n 2. See a record");
        printf("\n 3. Delete a record");
        printf("\n 4. Search records by name");
        printf("\n 5. Exit the system\n");
        scanf("%d", &option);
        switch(option){
            case 1:
                printf("Enter a record\n");
                enterAnimal(table);
                break;

            case 2:
                numberOfDogs=getNumDogs();
                printf("The number of records is: %ld\n",numberOfDogs);
                printf("Enter the ID of the desired animal to see its medical history: ");
                scanf("%i",&id);
                struct dogType *dog=getDogById(id);
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
                break;

            case 3:
                numberOfDogs=getNumDogs();
                printf("The number of records is: %ld\n",numberOfDogs);
                printf("Enter the ID of the animal you would like to delete: ");
                scanf("%i",&id);
                deleteRegister(id,table);
                toContinue();
                break;

            case 4:
                printf("Enter a name to see all the pets registered with that name: ");
                char name[32];
                scanf("%s",name);
                char *lowerName=toLowerCase(name,32);
                int hashIndex=hashFunction(lowerName);
                if(*(table+hashIndex)==-1){
                    printf("There are no records with that name\n");
                }else{
                    printf("The pets and its IDs with the requested name are:\n");
                    getListOfDogs(table,hashIndex,lowerName);
                }
                free(lowerName);
                toContinue();
                break;
        }
    }while(option!=5);

    updateTable(table);
    free(table);
    return 0;
}