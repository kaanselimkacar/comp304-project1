#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <fcntl.h>
int main(int argc, char* argv[]){

	int i=0;
	char arr[50]; //string array created to copy the content of the txt file
	int N=0; //length of the txt file will be stored in N

	FILE *fp;
	if (access("write_to_pipes2",F_OK) == 0){
		//read from pipes
		fp = fopen("pipes.txt","r");
		if ( fp == NULL){
			printf("Error: No such file\n");
			exit(1);
		}
		//write to pipes2
		//first if pipes2.txt exists delete it
		if (access("pipes2.txt",F_OK) == 0){
			remove("pipes2.txt");
		}
		int file_desc2 = open("pipes2.txt",O_WRONLY | O_CREAT,0777) ;
		dup2(file_desc2,1);
		close(file_desc2);
	}
	else{
		//read from pipes2
		fp = fopen("pipes2.txt","r");
		if ( fp == NULL){
			printf("Error: No such file\n");
			exit(1);
		}
		//write to pipes
		//first if pipes.txt exists delete it
		if (access("pipes.txt",F_OK) == 0){
			remove("pipes.txt");
		}
		int file_desc2 = open("pipes.txt",O_WRONLY | O_CREAT,0777) ;
		dup2(file_desc2,1);
		close(file_desc2);
	}
	if (argv[1] != NULL) {
	  	if (strcmp(argv[1],"-c")==0) { //if uniq -c command exists
	  		while(fgets(arr, 50, fp)!=NULL){ //counting the number of lines
			N++;	
		}
		char strings[100][50];
		rewind(fp);
		int duplicate_count=1;
		for(i=0;i<N /*&& i<MAX_STRINGS*/;i++){	//copy content of the file into the string array			
			fgets(arr, 50, fp);
			strcpy(strings[i], arr); 
		}
			i=0;
			while (i<N) {
				if (strcmp(strings[i],strings[i+1])==0 && (i+1)<N) { //counting for duplicates
					duplicate_count++;
					i++;
				}
				else {
					printf("%d %s",duplicate_count,strings[i]);
					i++;
					duplicate_count=1;
				}			      		
			}
					    	
		}
	}
	else { //if uniq command exists
		while(fgets(arr, 50, fp)!=NULL){ //counting the number of lines
			N++;	
		}
		char strings[100][50];
		rewind(fp);
		int duplicate_count=1;
		for(i=0;i<N /*&& i<MAX_STRINGS*/;i++){	//copy content of the file into the string array			
			fgets(arr, 50, fp);
			strcpy(strings[i], arr); 
		}
	   	for (i=0; i<N; i++) {
			if (i!=0) {	
				for (int j=i-1; j<i+1; j++) {					
					if (strcmp(strings[i],strings[j])==0) {
						//do nothing;
					}
					else {
						printf("%s",strings[i]);
					}
				}
			}
			else {
				printf("%s",strings[i]);
			}
		}
	}
	fclose(fp);
	
    return 0;
}
