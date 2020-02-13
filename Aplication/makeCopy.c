
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#define MAX_SIZE_NAME 100
#define MAX_SIZE_DATE 100
//date_of_last_change.dat contains: date of las change of file, date of creation of last copy
//global variable, stores path name inserted by user(just the name, not the .txt)
const char *path_name = NULL;
//Checks Errors
bool checkError(int number,int errorValue, char *message){
    if(number==errorValue){
        perror(message);
        exit(-1);
        return false;
    }
    return true;
}

//checks if argument was inserted by user
void check_valid_argument(int argc)
{
    if (argc != 2) 
        {
            fprintf(stderr, "please write ./ejecutable filePath\n");
            exit(-1);
        }
}
//checks if the file that user entered exists
bool file_exists(char *path)
{
    int ac=access(path, F_OK);
    if (ac==-1){
        return false;
    }
    else
    {
        return true;
    }
}
//returns time_t date of current day
time_t get_current_date()
{
    time_t t;
    t=time(&t);
    return t;
}
//returns time_t date of the last change made in file given by path
time_t get_date_of_last_change(const char *path)
{
    struct stat file_stat;
    int err = stat(path, &file_stat);
    if (err != 0) {
        perror(" [file_is_modified] stat");
        exit(errno);
    }
    return file_stat.st_mtime;
}
//puts string representation of date inside of str string
void get_date_format(char* str, time_t date)
{
    struct tm *tm;
    tm=localtime(&date);
    strftime(str,MAX_SIZE_DATE, "%Y%m%d-%H%M%S", tm); //here the format is given
   
}

//creates a new file of name: name_date, copy_path is filled with path of new file
void create_Copy(const char* name, char* date,char* copy_path){
    FILE *file;
    //appends name and date to create new file path_name
    sprintf(copy_path,"%s_%s.txt",name,date);
    //creates file
    file=fopen(copy_path, "ab+");
    fclose(file);
}

//puts contents of source_file to targe_file
void make_Copy(char *source_file, char *target_file)
{
   char ch;
   FILE *source, *target;
   source = fopen(source_file, "r");
   if( source == NULL )
   {
      printf("Error...\n");
      exit(-1);
   }
   target = fopen(target_file, "w"); 
   if( target == NULL )
   {
      fclose(source);
      printf("Error...\n");
      exit(-1);
   }
   while( ( ch = fgetc(source) ) != EOF )
      fputc(ch, target);
 
   printf("copy made successfully.\n");
 
   fclose(source);
   fclose(target);
}
//writes date of last change in file on date_last_change.dat file
void write_date_last_change(time_t date)
{
    FILE *file;
    file=fopen("date_of_last_change.dat","w+");
    if(file==NULL)
    {
        printf("error opening file");
        exit(-1);
    }
    int r = fwrite(&date,sizeof(date),1,file);
    checkError(r,0,"error writting");
    fclose(file);
}
void write_current_date(time_t date)
{
    FILE *file;
    file=fopen("date_of_last_change.dat","a");
    if(file==NULL)
    {
        printf("error opening file");
        exit(-1);
    }
    int r = fwrite(&date,sizeof(date),1,file);
    checkError(r,0,"error writting");
    fclose(file);
}
//gets the time of modification of file
time_t read_date_last_change()
{
    time_t date;
    FILE *file;
    file=fopen("date_of_last_change.dat","r");
    if(file==NULL)
    {
        printf("error opening file");
        exit(-1);
    }
    int r = fread(&date,sizeof(date),1,file);
    checkError(r,0,"error writting");
    fclose(file);
    return date;
}
//gets the time of creation of most recent copy
time_t read_date_last_copy()
{
    time_t date;
    FILE *file;
    file=fopen("date_of_last_change.dat","r");
    if(file==NULL)
    {
        printf("error opening file");
        exit(-1);
    }
    //sets file position indicator where the current date is (8 bytes ahead of beginig of file).
    fseek(file,sizeof(date), SEEK_SET);
    int r = fread(&date,sizeof(date),1,file);
    checkError(r,0,"error writting");
    fclose(file);
    return date;
}
//checks if file given by path has been modified since last copy
bool file_is_modified(const char *path) 
{
    char last_saved_str[MAX_SIZE_DATE];
    char curr_str[MAX_SIZE_DATE];
    if(!file_exists("date_of_last_change.dat"))
    {
        //there are not copys,so, file has been modify by default
        return true;
    }
    else
    {
        //time of last copy
        time_t last_saved=read_date_last_change();
        //time of file
        time_t curr_date=get_date_of_last_change(path);
        get_date_format(curr_str,curr_date);
        printf("Date of %s last change: %s\n",path,curr_str);
        get_date_format(last_saved_str,last_saved);
        printf("Date of most recent copy: %s\n",last_saved_str);
        //if curr_date is greater it means the file has been modify ater the most recent copy made
        return curr_date > last_saved;
    }
}
//creates a hard_link of name: name_currentdate, from file of path name_copydate (name of last copy)
void create_hard_link(const char* name, char* copy_date, char* current_date){
    char link_command[MAX_SIZE_NAME];
    //appends name and date to create shell command to create link
    sprintf(link_command,"ln %s_%s.txt %s_%s",name, copy_date, name, current_date);
    //creates hard_link
    system(link_command);
    //link_ command has the form ln name_copyDate.txt name_currentDate
    
}
int main(int argc, char const *argv[])
{
    path_name = argv[1];
    char copy_path[MAX_SIZE_NAME];
    char path[MAX_SIZE_NAME];
    char last_change_date_str[MAX_SIZE_DATE];
    char curr_date_str[MAX_SIZE_DATE];
    char last_copy_date_str[MAX_SIZE_DATE];
    time_t last_change;
    time_t curr_date;
    time_t last_copy;

    check_valid_argument(argc);
    //creates new string with .txt append to path_name
    sprintf(path,"%s.txt",path_name);
    if(!file_exists(path))
    {
        fprintf(stderr, "file does not exist\n");
        exit(-1);
    }
    //get date of last change
    last_change = get_date_of_last_change(path);
    get_date_format(last_change_date_str,last_change);
    //get current date
    curr_date = get_current_date();
    get_date_format(curr_date_str,curr_date);
    if(file_is_modified(path))
    {
        //creates copy file named: path_name_curr_date
        create_Copy(path_name,curr_date_str,copy_path);
        //makes the copy of path file onto copy_path file
        make_Copy(path,copy_path);
        //saves date of last change, this will be usefull to know if the file was modified in the future
        write_date_last_change(last_change);
        //saves date of creation of new copy,this will be usefull to know find the most recent copy
        write_current_date(curr_date);
    }
    else
    {
        printf("file has not been changed since last update\n");
        //get date of creation of last copy
        last_copy=read_date_last_copy();
        get_date_format(last_copy_date_str,last_copy);
        //creates hard link
        create_hard_link(path_name,last_copy_date_str,curr_date_str);
        printf("new link has been made to last copy\n");
    }
    
}
