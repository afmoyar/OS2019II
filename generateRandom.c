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
#include <time.h>
#include <strings.h>
//#define  TABLE_SIZE 100189
#define  TABLE_SIZE 999983
#define BLOCK 1000
#define NUM_BLOCKS 10000

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




//Checks Errors
bool checkError(int number,int errorValue, char *message){
    if(number==errorValue){
        perror(message);
        exit(-1);
        return false;
    }
    return true;
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

//Fills the array with a given value
void fillArray(long *array,int size,int value){
    for(int i=0;i<size;i++){
        *(array+i)=value;
    }
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



int main(){
    long *table;
    table=malloc(TABLE_SIZE*sizeof(long));
    initTable(table);
    FILE *fnames;
    char str[BLOCK];
    char names[BLOCK][32];
    char chooseTypes[6][32]={"dog","cat","bird","hamster","fish","turtle"};
    char chooseBreeds[5][16]={"criollo","german","russian","little","big"};

    fnames=fopen("names.txt","r");
    struct dogType * dogArray = malloc(sizeof(struct dogType) * BLOCK*NUM_BLOCKS);
    int i=0;
    if (fnames==NULL){
        perror("MAIN: error in fopen of fnames\n");
        exit(-1);
    }
    int dogsFile=0;
    int numCollisions=0;
    for(int j=0;j<NUM_BLOCKS;j++){
        fseek(fnames,0L,SEEK_SET);
        dogsFile=0;
        while (fgets(str,BLOCK,fnames)!=NULL){
            dogsFile++;
            strcpy(str,strtok(str, "\n"));
            strcpy(dogArray[i].nombre,str);
            //strcpy(dogArray[i].tipo,str);
            strcpy(dogArray[i].tipo,chooseTypes[i*j%6]);
            dogArray[i].edad = i % 15;
            //strcpy(dogArray[i].raza,str);
            strcpy(dogArray[i].raza,chooseBreeds[i*j%5]);
            dogArray[i].id=(i+1);
            dogArray[i].estatura = i % 200;
            dogArray[i].peso = i % 100;
            dogArray[i].sexo = i % 2 ? 'H' : 'M';
            dogArray[i].addr=sizeof(struct dogType)*i;
            if(j==NUM_BLOCKS-1){
                dogArray[i].next=-1;
            }else{
                dogArray[i].next=sizeof(struct dogType)*BLOCK+dogArray[i].addr;
            }
            char *lowerName=toLowerCase(dogArray[i].nombre,32);
            int hashIndex=hashFunction(lowerName);
            if(*(table+hashIndex)==-1){
                //There are no collision 
                *(table+hashIndex)=dogArray[i].addr;
            }else if(j==0){
                numCollisions=numCollisions+1;
                printf("Dog id: %d ,table[%d]=%ld, nombre: %s\n",dogArray[i].id,hashIndex,*(table+hashIndex),dogArray[i].nombre);
            }
            free(lowerName);           
            i++;
        }

    }

    fclose(fnames);
    FILE *file ;
    file = fopen("dataDogs.dat", "wb+");
    if (fopen==NULL){
        perror("MAIN: error in fopen of dataDogs\n");
        exit(-1);
    }
    int numErrors=0;
    for(int j=0;j<NUM_BLOCKS*BLOCK;j++){
        int r=fwrite(dogArray+j,sizeof(struct dogType),1,file);
        if(r!=sizeof(struct dogType)){
            numErrors=numErrors+1;
        }
    }
    fclose(file);
    free(dogArray);
    updateTable(table);
    free(table);
    printf("Number of Collisions: %d",numCollisions);
    return 0;
}  
