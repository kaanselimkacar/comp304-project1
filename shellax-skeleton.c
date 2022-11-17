#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dirent.h>
#define MAX_STRINGS 100


#define READ_END 0
#define WRITE_END 1

const char *sysname = "shellax";

enum return_codes {
  SUCCESS = 0,
  EXIT = 1,
  UNKNOWN = 2,
};

struct command_t {
  char *name;
  bool background;
  bool auto_complete;
  int arg_count;
  char **args;
  char *redirects[3];     // in/out redirection
  struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
  int i = 0;
  printf("Command: <%s>\n", command->name);
  printf("\tIs Background: %s\n", command->background ? "yes" : "no");
  printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
  printf("\tRedirects:\n");
  for (i = 0; i < 3; i++)
    printf("\t\t%d: %s\n", i,
           command->redirects[i] ? command->redirects[i] : "N/A");
  printf("\tArguments (%d):\n", command->arg_count);
  for (i = 0; i < command->arg_count; ++i)
    printf("\t\tArg %d: %s\n", i, command->args[i]);
  if (command->next) {
    printf("\tPiped to:\n");
    print_command(command->next);
  }
}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
  if (command->arg_count) {
    for (int i = 0; i < command->arg_count; ++i)
      free(command->args[i]);
    free(command->args);
  }
  for (int i = 0; i < 3; ++i)
    if (command->redirects[i])
      free(command->redirects[i]);
  if (command->next) {
    free_command(command->next);
    command->next = NULL;
  }
  free(command->name);
  free(command);
  return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
  char cwd[1024], hostname[1024];
  gethostname(hostname, sizeof(hostname));
  getcwd(cwd, sizeof(cwd));
  printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
  return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
  const char *splitters = " \t"; // split at whitespace
  int index, len;
  len = strlen(buf);
  while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
  {
    buf++;
    len--;
  }
  while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
    buf[--len] = 0; // trim right whitespace

  if (len > 0 && buf[len - 1] == '?') // auto-complete
    command->auto_complete = true;
  if (len > 0 && buf[len - 1] == '&') // background
    command->background = true;

  char *pch = strtok(buf, splitters);
  if (pch == NULL) {
    command->name = (char *)malloc(1);
    command->name[0] = 0;
  } else {
    command->name = (char *)malloc(strlen(pch) + 1);
    strcpy(command->name, pch);
  }

  command->args = (char **)malloc(sizeof(char *));

  int redirect_index;
  int arg_index = 0;
  char temp_buf[1024], *arg;
  while (1) {
    // tokenize input on splitters
    pch = strtok(NULL, splitters);
    if (!pch)
      break;
    arg = temp_buf;
    strcpy(arg, pch);
    len = strlen(arg);

    if (len == 0)
      continue; // empty arg, go for next
    while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
    {
      arg++;
      len--;
    }
    while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
      arg[--len] = 0; // trim right whitespace
    if (len == 0)
      continue; // empty arg, go for next

    // piping to another command
    if (strcmp(arg, "|") == 0) {
      struct command_t *c = malloc(sizeof(struct command_t));
      int l = strlen(pch);
      pch[l] = splitters[0]; // restore strtok termination
      index = 1;
      while (pch[index] == ' ' || pch[index] == '\t')
        index++; // skip whitespaces

      parse_command(pch + index, c);
      pch[l] = 0; // put back strtok termination
      command->next = c;
      continue;
    }

    // background process
    if (strcmp(arg, "&") == 0)
      continue; // handled before

    // handle input redirection
    redirect_index = -1;
    if (arg[0] == '<')
      redirect_index = 0;
    if (arg[0] == '>') {
      if (len > 1 && arg[1] == '>') {
        redirect_index = 2;
        arg++;
        len--;
      } else
        redirect_index = 1;
    }
    if (redirect_index != -1) {
      command->redirects[redirect_index] = malloc(len);
      strcpy(command->redirects[redirect_index], arg + 1);
      continue;
    }

    // normal arguments
    if (len > 2 &&
        ((arg[0] == '"' && arg[len - 1] == '"') ||
         (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
    {
      arg[--len] = 0;
      arg++;
    }
    command->args =
        (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
    command->args[arg_index] = (char *)malloc(len + 1);
    strcpy(command->args[arg_index++], arg);
  }
  command->arg_count = arg_index;
  return 0;
}

void prompt_backspace() {
  putchar(8);   // go back 1
  putchar(' '); // write empty over
  putchar(8);   // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
  int index = 0;
  char c;
  char buf[4096];
  static char oldbuf[4096];

  // tcgetattr gets the parameters of the current terminal
  // STDIN_FILENO will tell tcgetattr that it should write the settings
  // of stdin to oldt
  static struct termios backup_termios, new_termios;
  tcgetattr(STDIN_FILENO, &backup_termios);
  new_termios = backup_termios;
  // ICANON normally takes care that one line at a time will be processed
  // that means it will return if it sees a "\n" or an EOF or an EOL
  new_termios.c_lflag &=
      ~(ICANON |
        ECHO); // Also disable automatic echo. We manually echo each char.
  // Those new settings will be set to STDIN
  // TCSANOW tells tcsetattr to change attributes immediately.
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

  show_prompt();
  buf[0] = 0;
  while (1) {
    c = getchar();
    // printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

    if (c == 9) // handle tab
    {
      buf[index++] = '?'; // autocomplete
      break;
    }

    if (c == 127) // handle backspace
    {
      if (index > 0) {
        prompt_backspace();
        index--;
      }
      continue;
    }

    if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
      continue;
    }

    if (c == 65) // up arrow
    {
      while (index > 0) {
        prompt_backspace();
        index--;
      }

      char tmpbuf[4096];
      printf("%s", oldbuf);
      strcpy(tmpbuf, buf);
      strcpy(buf, oldbuf);
      strcpy(oldbuf, tmpbuf);
      index += strlen(buf);
      continue;
    }

    putchar(c); // echo the character
    buf[index++] = c;
    if (index >= sizeof(buf) - 1)
      break;
    if (c == '\n') // enter key
      break;
    if (c == 4) // Ctrl+D
      return EXIT;
  }
  if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
    index--;
  buf[index++] = '\0'; // null terminate string

  strcpy(oldbuf, buf);

  parse_command(buf, command);

  // print_command(command); // DEBUG: uncomment for debugging

  // restore the old settings
  tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  return SUCCESS;
}
int process_command(struct command_t *command);
int main() {
  while (1) {
    struct command_t *command = malloc(sizeof(struct command_t));
    memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

    int code;
    code = prompt(command);
    if (code == EXIT)
      break;

    code = process_command(command);
    if (code == EXIT)
      break;

    free_command(command);
  }

  printf("\n");
  return 0;
}

int process_command(struct command_t *command) {
  int r;
  if (strcmp(command->name, "") == 0)
    return SUCCESS;

  if (strcmp(command->name, "exit") == 0)
    return EXIT;

  if (strcmp(command->name, "cd") == 0) {
    if (command->arg_count > 0) {
      r = chdir(command->args[0]);
      if (r == -1)
        printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      return SUCCESS;
    }
  }

  pid_t pid = fork();
  if (pid == 0) // child
  {
    /// This shows how to do exec with environ (but is not available on MacOs)
    // extern char** environ; // environment variables
    // execvpe(command->name, command->args, environ); // exec+args+path+environ

    /// This shows how to do exec with auto-path resolve
    // add a NULL argument to the end of args, and the name to the beginning
    // as required by exec
    
    /********* need this commands for piping */

    // increase args size by 2
    command->args = (char **)realloc(
        command->args, sizeof(char *) * (command->arg_count += 2));

    // shift everything forward by 1
    for (int i = command->arg_count - 2; i > 0; --i)
      command->args[i] = command->args[i - 1];

    // set args[0] as a copy of name
    command->args[0] = strdup(command->name);
    // set args[arg_count-1] (last) to NULL
    command->args[command->arg_count - 1] = NULL;
    // do so by replacing the execvp call below
    
    char *dummystr = command->args[command->arg_count-2];
    int dummylen = strlen(dummystr)-1; 
	if (dummystr[dummylen] == '&' ){
		dummystr[dummylen] = '\0';
	}
	
	
	//if we are here we will execute and return anyways
	//else continue
	
	
	//int total_redirects = 0; // need this to run the program multiple times
	/******************************** handle redirects *********************************************************/
	if (command->redirects[0] != NULL){
		// "<" so input
		//doesnt work still :(
		//total_redirects++;
		int file_desc = open(command->redirects[0],O_RDONLY);
		int fd_dup1 = dup2(file_desc,0);
		close(file_desc);
    /*
		int index = 0;
		while(command->args[index] != NULL){
			printf("command->args[%d] = %s\n",index,command->args[index]);
			index++;
		}
    */
	}
	if (command->redirects[1] != NULL && command->redirects[2] != NULL){
		//if both > >> then give priority to > I guess idk
		pid_t pidalpha = fork();
		if (pidalpha == 0){
			if (access(command->redirects[1], F_OK) == 0) {
				pid_t terrorist = fork();
				if (terrorist == 0){
					strcpy(command->args[1],command->redirects[1]);
					strcpy(command->args[0],"rm");
					command->args[2] = NULL;
					execvp("rm",command->args);
					exit(0);
					}
				}
			wait(NULL);
			//total_redirects++;
			int file_desc1 = open(command->redirects[1],O_WRONLY | O_CREAT,0777) ;
			int fd_dup2 = dup2(file_desc1,1);
			close(file_desc1);
			//execute the code
			char *token = strtok(getenv("PATH"),":");
			while(token != NULL){
				char path[50];
				strcpy(path,token);	
				strcat(path,"/");
				strcat(path,command->args[0]);
				if (execv(path,command->args) != -1){ // do nothing
				}
				token = strtok(NULL,":");	
			}
		}
		int file_desc2 = open(command->redirects[2],O_WRONLY | O_APPEND | O_CREAT,0777);
		int fd_dup3 = dup2(file_desc2,1);
		close(file_desc2);
	}
    else if (command->redirects[1] != NULL){
		// ">" so input
		// works but dangerous as I use rm
		if (access(command->redirects[1], F_OK) == 0) {
			pid_t terrorist = fork();
			if (terrorist == 0){
				strcpy(command->args[1],command->redirects[1]);
				strcpy(command->args[0],"rm");
				command->args[2] = NULL;
				execvp("rm",command->args);
				exit(0);
				}
			}
		wait(NULL);
		//total_redirects++;
		int file_desc1 = open(command->redirects[1],O_WRONLY | O_CREAT,0777) ;
		int fd_dup2 = dup2(file_desc1,1);
		close(file_desc1);
	}
	else if (command->redirects[2] != NULL){
		// ">>" so input
		// works !!!!!!!!!!!
		//total_redirects++;
		int file_desc2 = open(command->redirects[2],O_WRONLY | O_APPEND | O_CREAT,0777);
		int fd_dup3 = dup2(file_desc2,1);
		close(file_desc2);
	}
	
	/************************ WISEMAN *************************************************/
	if (strcmp(command->args[0],"wiseman") == 0){
    int minutes;
    if (command->args[1] == NULL){
        minutes = 5;//arbitrary number
    }
    else{
      minutes = atoi(command->args[1]);
    }
    printf("Wiseman executed!\n");
    FILE * fp;
    fp = fopen("wisemancron", "w");
    char str[100];
    sprintf(str,"*/%d * * * * bash -c 'fortune | espeak'\n",minutes);
    fputs(str, fp);    
    fclose(fp);
    command->args[0] = "crontab";
    command->args[1] = "wisemancron";
    command->args[2] = NULL;
    char *token = strtok(getenv("PATH"),":");
    while(token != NULL){
      char path[50];
      strcpy(path,token);	
      strcat(path,"/");
      strcat(path,command->args[0]);
      if (execv(path,command->args) != -1){ // do nothing
      }
    token = strtok(NULL,":");	
    }
	}
  /************************* END WISEMAN *********************************************/
  if(strcmp(command->args[0],"wisemanoff") == 0){
    printf("Wisemanoff executed!\n");
    command->args[0] = "crontab";
    command->args[1] = "-r";
    command->args[2] = NULL;
    char *token = strtok(getenv("PATH"),":");
    while(token != NULL){
      char path[50];
      strcpy(path,token);	
      strcat(path,"/");
      strcat(path,command->args[0]);
      if (execv(path,command->args) != -1){ // do nothing
      }
    token = strtok(NULL,":");	
    }
  }
  /********************CHATROOM***************************************************/
  // TODO: add write your message here functionality
  if(strcmp(command->args[0],"chatroom") == 0){
    if(command->args[1] == NULL || command->args[2] == NULL){
      printf("chatroom requires 2 arguments\n");
      exit(0);
    }
    char room[50] = "chatroom-";
    strcat(room,command->args[1]);
    char *name = command->args[2];
    // create the room_address
    char room_address[50] = "/tmp/";
    strcat(room_address,room);
    //create the user_address
    char user_address[50] = "/tmp/";
    strcat(user_address,room);
    strcat(user_address,"/");
    strcat(user_address,name);

    if(access(room_address, F_OK) == -1){
      //if the chatroom doesn't exist
      //create the chatroom
      mkdir(room_address, 0777);
    }
    //if user exists in chatroom
    //remove the user
    if(access(user_address, F_OK) != -1){
      remove(user_address);
    }
    //create the user
    mkfifo(user_address, 0777);
 
    //continuosuly read from the user fifo
    //continuously write to the chatroom fifo
    
    char read_msg[100];
    char prev_msg[100];
    char get_msg[100];
    char write_msg[100];
    printf("Welcome to %s!\n",command->args[1]);
    pid_t reader_child = fork();
    if(reader_child == 0){
      int fd = open(user_address, O_RDONLY);
      while(1){
        read(fd, read_msg, 100);
        if(strcmp(read_msg,prev_msg) != 0){
          //printf("\r");
          printf("[%s] %s",command->args[1],read_msg);
          strcpy(prev_msg,read_msg);
        }
      }
    }
    else{//writer parent
      while(1){
        //printf("[%s] %s >> ",command->args[1],command->args[2]);
        strcpy(write_msg,name);
        strcat(write_msg,": ");
        fgets(get_msg, 100, stdin);
        strcat(write_msg,get_msg);
        //write to all other users' pipe
        DIR *d;
        struct dirent *dir;
        d = opendir(room_address);
        if (d){
          while ((dir = readdir(d)) != NULL){
            if(strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0){
              char user_address[50] = "/tmp/";
              strcat(user_address,room);
              strcat(user_address,"/");
              strcat(user_address,dir->d_name);
              int fd = open(user_address, O_WRONLY);
              write(fd, write_msg, 100);
            }
          }
          closedir(d);
        }
      }
    }  
  }
    
  /**********************custom command(dadjokes)*****************************/
  if (strcmp(command->args[0],"dadjokes") == 0){
    FILE *fp;
    fp = fopen("dadjokes.txt","r");
    char str[500];
    srand(time(NULL));
    int random = rand() % 250;
    int i = 0;
    while (fgets(str,500,fp) != NULL){
      if (i == random){
        printf("%s\n",str);
        break;
      }
      i++;
    }
    //exec cat with dad.txt
    command->args[0] = "cat";
    command->args[1] = "dad.txt";
    command->args[2] = NULL;
    char *token = strtok(getenv("PATH"),":");
    while(token != NULL){
      char path[50];
      strcpy(path,token);	
      strcat(path,"/");
      strcat(path,command->args[0]);
      if (execv(path,command->args) != -1){ // do nothing
      }
      token = strtok(NULL,":");
    }
    fclose(fp);
    exit(0);
  }
    /********************custom command (love)********************************/
  /*prints the content of the file with the specified name
  e.g love [name] */
  if (strcmp(command->args[0],"love")==0) {
  	if (command->args[1] != NULL) {
	  	printf("\n\n                %s\n",command->args[1]); //name that will printed	
	  	FILE *fptr;
	    	char filename[100], c;

	    	// Open file
	   	fptr = fopen(command->args[0], "r");
	    	if (fptr == NULL) {
			printf("Cannot open file \n");
			exit(0);
	    	}
	    	// Read contents from file
	    	c = fgetc(fptr);
	    	while (c != EOF) {
			printf ("%c", c);
			c = fgetc(fptr);
	    	}
	    	fclose(fptr);
	    	return 0;
  	}
  	else {
  		printf("Please enter a name..\n");
  	}
  }
	 /*****************************psvis************************************/
 	
  	if (strcmp(command->args[0],"psvis") == 0 ){
  		strcpy(command->args[0],"/home/sude/comp304project1/psvis");
        	execv("/home/sude/comp304project1/psvis",command->args);
    		
    		char *token = strtok(getenv("PATH"),":");
    			while(token != NULL){
      			char path[50];
      			strcpy(path,token);	
      			strcat(path,"/");
      			strcat(path,command->args[0]);
      			if (execv(path,command->args) != -1){ 
      				// do nothing
      			}
	  	}
  	}
  	
   	/*****************************************************************/
	/*********************** UNIQ ***************************************/
	
	if (strcmp(command->args[0],"uniq") == 0 ){
    		strcpy(command->args[0],"./uniq");
    		char *token = strtok(getenv("PATH"),":");
    		while(token != NULL){
      			char path[50];
      			strcpy(path,token);	
      			strcat(path,"/");
      			strcat(path,command->args[0]);
      			if (execv(path,command->args) != -1){ 
      				// do nothing
      			}
	  	}
  	}
	// for more than 2 pipes
	//getting total number of pipes
  /**************************************************************************/
	struct command_t *head = command;
	
	//if there are more than 2 pipes
	//get the total number of pipes
	
	int total_pipes = 0;
	
	while (command){ //counting total pipes
		total_pipes++;
		command = command->next;
		//printf("command: %s\n",command->name);
	}
	
	int iteration = total_pipes;
	command = head; 
  /***************** piping *****************************************/
  int write_to_pipes2 = 0;
  int read_from_pipes2 = 0;
	while (iteration > 1){
		pid_t pipebaby = fork();
		if (pipebaby == 0){
			if (iteration == total_pipes){ 
        //first iteration
				//******************write the output into a file then keep going from there************************//
				/****** delete the file if it exist */
				if (access("pipes.txt", F_OK) == 0) {
					remove("pipes.txt");
				}
        if (access("pipes2.txt", F_OK) == 0) {
					remove("pipes2.txt");
				}
        if(access("write_to_pipes2",F_OK) == 0){
            remove("write_to_pipes2");
        }

				/******************************************/
				/*** write to the file ****/
				int file_desc1 = open("pipes.txt",O_WRONLY | O_CREAT,0777);
				dup2(file_desc1,1);
				close(file_desc1);
        int fd_dummy = open("write_to_pipes2",O_WRONLY | O_CREAT,0777); // to create write_to_pipes2
        close(fd_dummy); 

				//execute the code
        /******** uniq *******************/
				if (strcmp(command->args[0],"uniq") == 0 ){
					strcpy(command->args[0],"/home/kaan/comp304/project1/uniq");
          execv("/home/kaan/comp304/project1/uniq",command->args);
				}
				char *token = strtok(getenv("PATH"),":");
				while(token != NULL){
					char path[50];
					strcpy(path,token);	
					strcat(path,"/");
					strcat(path,command->args[0]);
					if (execv(path,command->args) != -1){ // do nothing
					}
					token = strtok(NULL,":");	
				}
			}
			else{ 
				// not first iteration
				for (int i=0 ; i<(total_pipes-iteration);i++){
        			command = command->next;
        		}
				// increase args size by 2
    			command->args = (char **)realloc(
       				command->args, sizeof(char *) * (command->arg_count += 2));

				// shift everything forward by 1
				for (int i = command->arg_count - 2; i > 0; --i){
		   			command->args[i] = command->args[i - 1];
        }
		    	// set args[0] as a copy of name
			    command->args[0] = strdup(command->name);
			   	// set args[arg_count-1] (last) to NULL
			    command->args[command->arg_count - 1] = NULL;
			    ////////////////////////////////////////////////////////////  
          /* take inputs*/
          //printf("write  == %d\n",write_to_pipes2);
          if (access("write_to_pipes2",F_OK) == 0){//if it exists
            write_to_pipes2 = 1;
            read_from_pipes2 = 0;
          }
          else{
            write_to_pipes2 = 0;
            read_from_pipes2 = 1;
          }
          if(read_from_pipes2){
            int file_desc1 = open("pipes2.txt",O_RDONLY) ;
            dup2(file_desc1,0);
            close(file_desc1);
          }
          else{//read from pipes.txt
            int file_desc1 = open("pipes.txt",O_RDONLY) ;
            dup2(file_desc1,0);
            close(file_desc1);
          }

          /******** uniq *******************/
          if (strcmp(command->args[0],"uniq") == 0 ){
            strcpy(command->args[0],"/home/kaan/comp304/project1/uniq");
            execv("/home/kaan/comp304/project1/uniq",command->args);
          }
          /***********/

          /*write to the file*/
          if (write_to_pipes2){
            //remove pipes2.txt if it exist
            if(access("pipes2.txt",F_OK) == 0){
              remove("pipes2.txt");
            }
            int file_desc2 = open("pipes2.txt",O_WRONLY | O_CREAT,0777) ;
            dup2(file_desc2,1);
            close(file_desc2);
            remove("write_to_pipes2");
            //write_to_pipes2 = 0;
            //read_from_pipes2 = 1;
          }

          else{//write to pipes.txt
              //remove pipes.txt first if it exists
              if(access("pipes.txt",F_OK) == 0){
                remove("pipes.txt");
              }
            int file_desc2 = open("pipes.txt",O_WRONLY | O_CREAT,0777) ;
            dup2(file_desc2,1);
            close(file_desc2);
            //write_to_pipes2 = 1;
            //read_from_pipes2 = 0;
            int fd = open("write_to_pipes2",O_WRONLY | O_CREAT,0777);
            close(fd);           
          }
          //execute the code
          
          char *token = strtok(getenv("PATH"),":");
          while(token != NULL){
            char path[50];
            strcpy(path,token);	
            strcat(path,"/");
            strcat(path,command->args[0]);
            if (execv(path,command->args) != -1){ // do nothing
            }
            token = strtok(NULL,":");	
          }
        }
			}
		
		else{ // parent
			if(iteration != 2){
				wait(NULL);
				//do nothing and let the children execute
			}
			else{
				wait(NULL);
        /************************/
          for (int i=0 ; i<(total_pipes-iteration)+1 ;i++){
        			command = command->next;
            }
        	// increase args size by 2
    			command->args = (char **)realloc(
        			command->args, sizeof(char *) * (command->arg_count += 2));

				  // shift everything forward by 1
			    for (int i = command->arg_count - 2; i > 0; --i)
 			   		command->args[i] = command->args[i - 1];

			      // set args[0] as a copy of name
 			    command->args[0] = strdup(command->name);
 			      // set args[arg_count-1] (last) to NULL
 			    command->args[command->arg_count - 1] = NULL;
 			    //printf("command = %s\n",command->args[0]);
          /*          take input            */
          if (access("write_to_pipes2",F_OK) == 0){//if it exists
           //printf("WRITING TO PIPES 2 !\n");
            write_to_pipes2 = 1;
            read_from_pipes2 = 0;
          }
          else{
            //printf("WRITING TO PIPES!\n");
            write_to_pipes2 = 0;
            read_from_pipes2 = 1;
          }
          if(read_from_pipes2){
            int file_desc1 = open("pipes2.txt",O_RDONLY) ;
            dup2(file_desc1,0);
            close(file_desc1);
          }
          else{//read from pipes.txt
            int file_desc1 = open("pipes.txt",O_RDONLY) ;
            dup2(file_desc1,0);
            close(file_desc1);
          }
				pid_t pid_child = fork();
        if (pid_child == 0){
          // execute the code
          /******** uniq *******************/
          if (strcmp(command->args[0],"uniq") == 0 ){
            strcpy(command->args[0],"/home/kaan/comp304/project1/uniq");
            execv("/home/kaan/comp304/project1/uniq",command->args);
          }
          char *token = strtok(getenv("PATH"),":");
			  	while(token != NULL){
				  	char path[50];
				  	strcpy(path,token);	
				  	strcat(path,"/");
			  		strcat(path,command->args[0]);
			  		if (execv(path,command->args) != -1){ // do nothing
			  			}
				  	token = strtok(NULL,":");	
				  }
        }
        else{
          wait(NULL);
          /******if uniq*/
          if (strcmp(command->args[0],"uniq") == 0 ){
            pid_t child = fork();
            if (child == 0){
              if(write_to_pipes2){
                command->args[0] = "cat";
                command->args[1] = "pipes2.txt";
                command->args[2] = NULL;
                execvp("cat",command->args);
              }
              else{
                command->args[0] = "cat";
                command->args[1] = "pipes.txt";
                command->args[2] = NULL;
                execvp("cat",command->args);
              }
            }
            else{//parent of child
              wait(NULL);
            }
          }
          /****************************/
          
          if(access("pipes2.txt",F_OK) == 0){
            //printf("pipes2.txt exists\n");
            remove("pipes2.txt");
          }
           if(access("write_to_pipes2",F_OK) == 0){
            //printf("write_to_pipes2 exists\n");
            remove("write_to_pipes2");
            //printf("write_to_pipes2 removed\n");
          }
          if (access("pipes.txt", F_OK) == 0) {
            //printf("pipes.txt exists\n");

            remove("pipes.txt");
            //printf("pipes.txt removed\n");
			  	}      
        
          
          exit(0);
          return 0;
        } 
			}
		}
		iteration--;
	}
    char *token = strtok(getenv("PATH"),":");
    while(token != NULL){
      char path[50];
      strcpy(path,token);	
      strcat(path,"/");
      strcat(path,command->args[0]);
      if (execv(path,command->args) != -1){ // do nothing
        }
      token = strtok(NULL,":");	
      }
    //execvp(command->name, command->args); // exec+args+path
    exit(0);
  } 
  
  
  
  
  else {  // parent
  		if(command->background == true){
  			//do nothing
  		}
  		else{
  			wait(NULL);
  		}
    //wait(0); // wait for child process to finish
    return SUCCESS;
  }

  // TODO: your implementation here

  printf("-%s: %s: command not found\n", sysname, command->name);
  return UNKNOWN;
}
