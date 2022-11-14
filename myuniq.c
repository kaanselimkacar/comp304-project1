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
	char str[100];
	char old_str[100];
	//TODO: read with pipes
    bool iscount;
    int indexoffile = 1;

	if (argv[1] != NULL){
		if (strcmp(argv[1], "-c") == 0){
			indexoffile++;
			iscount = true;
		}
		if (strcmp(argv[1],"--count") == 0 ){
			indexoffile++;
			iscount = true;
		}
	}
    if (argv[2]){
   		if (strcmp(argv[2], "-c") == 0){
			iscount = true;
			}
		if (strcmp(argv[2],"--count") == 0 ){
			iscount = true;
    		}
		}
	FILE *fp;
	if (access("write_to_pipes2",F_OK) == 0){
		fp = fopen("pipes.txt","r");
		if ( fp == NULL){
			printf("Error: No such file\n");
			exit(1);
		}
	}
	if (access("write_to_pipes2",F_OK) != 0){
		fp = fopen("pipes2.txt","r");
		if ( fp == NULL){
			printf("Error: No such file\n");
			exit(1);
		}
	}
    int n = 1;
    int *occurances = (int *)malloc(n*sizeof(int));
    int index = -1;
    char *collector = (char *)malloc(n*100);
    //iterate once to get the occurance
  	 while(fgets(str,100,fp) != NULL){
		if (strcmp(str,old_str)== 0){
			occurances[index]++;
		}
		else{
			n++;
			index++;
			if (occurances[index] == 0){
				occurances[index]++;
			}
			occurances = (int *) realloc(occurances,n*sizeof(int));
		}
      	strcpy(old_str,str);
    }
    fclose(fp);
    
    //iterate to get the strings now
   FILE *fp2;
	if (access("write_to_pipes2",F_OK) == 0){
		fp2 = fopen("pipes.txt","r");
		if ( fp2 == NULL){
			printf("Error: No such file\n");
			exit(1);
		}
	}
	if (access("write_to_pipes2",F_OK) != 0){
		fp2 = fopen("pipes2.txt","r");
		if ( fp2 == NULL){
			printf("Error: No such file\n");
			exit(1);
		}
	}
    index = -1;
    n = 1;
    while(fgets(str,100,fp2) != NULL){
		if (strcmp(str,old_str)== 0){
			//occurances[index]++;
		}
		else{
			n++;
			index++;
			collector = (char *)realloc(collector,n*100);
			if (iscount){
				char useless[10];
				//printf("useless : %s\n",useless);
				sprintf(useless,"%d",occurances[index]);
				strcat(collector,useless);
				strcat(collector," ");
			}
			//strcat(collector,"\t");
			strcat(collector,str);
		}
      	strcpy(old_str,str);
    }
    
    
    fclose(fp2);
    
    printf("%s",collector);
    free(collector);
    free(occurances);
    
    
    return 0;
}

