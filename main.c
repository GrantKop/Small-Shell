/*************************************************************
* Name: Grant Kopczenski
* Date: 11/09/2022
* Description: This program will act as a small shell, it has 3 built in functions as well as a call to excecvp to handle many more commands
* The shell will also:
* - handle redirection, background processes, and will ignore CTRL C and CTRL Z.
* - replace $$ with the PID of the shell
* - handle the status command to check for exit status codes and signals
* - handle the cd command to change the current working directory
* 
* This was an assignment for a class I took in college
*************************************************************/

/* 
* Referenced Programs found online:
* File redirection: https://www.geeksforgeeks.org/input-output-redirection-in-linux/
* Text Replacement: https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
* More signals: https://www.geeksforgeeks.org/signals-c-language/
* CD examples: https://www.geeksforgeeks.org/chdir-in-c-language-with-examples/
*/

//included libraries
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

//Global Variables (This was the easiest way to pass these values between functions, especially for SIGTSTP)
int SIGTSTPflag;
int exitCode;

/*
* Function: Handle SIGTSTP
* Parameters: int signo
* Description: Will switch between foreground-only mode on and off
*/
void handle_SIGTSTP(int signo) {

    //Code inspired from  canvas module Exploration in Signal Handeling
    //SIGTSTPflag will switch between 1 and 0 to turn on and off foreground only mode
    if (SIGTSTPflag == 0) {
        SIGTSTPflag = 1;
	    char* modeAlert = "Foreground-only mode enabled. (& is now ignored)\n";
	    write(STDOUT_FILENO, modeAlert, 49);
    } else {
        SIGTSTPflag = 0;
        char* modeAlert = "Foreground-only mode disabled. (& now works)\n";
	    write(STDOUT_FILENO, modeAlert, 45);
    }
}

/*
* Function: Create Child
* Parameters: char* arguments[], int backgroundFlag, int inputFlag, int outputFlag, int inputArg, int outputArg
* Description: This function will fork off new processes and create child processes with respect to <, >, and & flags
*/
int createChild(char* arguments[], int backgroundFlag, int inputFlag, int outputFlag, int inputArg, int outputArg) {

    //Code inspired from Exploration: Process API - Executing a new program and Geeks for Geeks file redirection
	int childStatus;
    int testR;
    int inputF;
    int outputF;

    //Ignores '&' if foreground only mode is on
    if (SIGTSTPflag == 1) {
        backgroundFlag = 0;
    }

	// Fork a new process
	pid_t spawnPid = fork();

    //Switch cases
	switch(spawnPid) {
	case -1:
		perror("fork()\n");
		break;
	case 0:
        //Sets CTRL C to be ignored
        signal(SIGTSTP, SIG_IGN);

        if (backgroundFlag == 0) {
            //Sets CTRL C to default
            signal(SIGINT, SIG_DFL);
        }

        //Checks if code is to be run in background, then checks other flags
        if (backgroundFlag == 1) {
            if (inputFlag == 0) {
                int inputF = open("/dev/null", O_RDONLY);
	            if (inputF == -1) { 
		            perror("source open()"); 
		            exit(1); 
	            }
	            testR = dup2(inputF, 0);
	            if (testR == -1) { 
		            perror("source dup2()"); 
		            exit(2); 
	            }
            }
            if (outputFlag == 0) {
	            int outputF = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	            if (outputF == -1) { 
		            perror("target open()"); 
		            exit(1); 
	            }
    
	            testR = dup2(outputF, 1);
	            if (testR == -1) { 
		            perror("target dup2()"); 
		            exit(2); 
	            }
            }
        }

        //Checks for input and output flags
        if (inputFlag == 1) {
            int inputF = open(arguments[inputArg], O_RDONLY);
	        if (inputF == -1) { 
		        perror("source open()"); 
		        exit(1); 
	        }
	        testR = dup2(inputF, 0);
	        if (testR == -1) { 
		        perror("source dup2()"); 
		        exit(2); 
	        }
        }
        if (outputFlag == 1) {
	        int outputF = open(arguments[outputArg], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	        if (outputF == -1) { 
		        perror("target open()"); 
		        exit(1); 
	        }
	        testR = dup2(outputF, 1);
	        if (testR == -1) { 
		        perror("target dup2()"); 
		        exit(2); 
	        }
        }

		//Child process
		execvp(arguments[0], arguments);
		perror("execve"); 
        exit(1);
        
	default:
		//Parent process
        if (backgroundFlag != 1) {
		    spawnPid = waitpid(spawnPid, &exitCode, 0);
        } else {
            printf("Background PID: %d\n", spawnPid);
            fflush(stdout);
        }
        break;
    }
}

/*
* Function: Status
* Description: This function will check for exit status codes and signals
*/
void status() {
    //Code from Geeks for Geeks
    if (WIFEXITED(exitCode)) {
        printf("Exit status %d\n", WEXITSTATUS(exitCode));
    } else if (WIFSIGNALED(exitCode)) {
        printf("Terminated by signal %d\n", WTERMSIG(exitCode));
    }
}

/*
* Function: CD
* Parameters: char* arguments[], int numArgs
* Description: This function will switch the current working directory (cwd) based on the second argument of the command line.
*/
void cd(char* arguments[], int numArgs) {

    //Code inspired from geeks for geeks
    //Sends to the 2nd argument directory, if it exists
    if (numArgs > 1) {
        if (chdir(arguments[1]) != 0) {
            printf("Failed to open directory: %s\n", arguments[1]);
        } 
        fflush(stdout);
    } else {
        //Sends to the home directory
        chdir(getenv("HOME"));
    }
}

/*
* Function: Replace Word
* Parameters: char* inputCommand, int pid
* Description: This function will switch any double $ in the command line with the PID.
*/
char* replaceWord(char* inputCommand, int pid) { 
    
    //Code inspired by Geeks for Geeks
    char charSearch[] = "$$";
    int i = 0;
    int counter = 0;
    int pidHolder = pid;

    //Gets the character length of int pid
    do {
        pid = pid/10;
        counter++;
    } while (pid > 0);

    char pidString[counter];
    sprintf(pidString, "%d", pidHolder);
    char* modifiedCommandLine; 
    int pidLen = counter; 
    counter = 0; 
    int charSearchLen = strlen(charSearch); 
  
    //Counts how many times $$ appears in the command line
    for (i = 0; inputCommand[i] != '\0'; i++) { 
        if (strstr(&inputCommand[i], charSearch) == &inputCommand[i]) { 
            counter++;  
            i = i + charSearchLen - 1; 
        } 
    } 
  
    modifiedCommandLine = (char*) malloc(i+counter*(pidLen-charSearchLen)+1);  
    i = 0;

    //Changes any $$ to the PID in the new temp string
    while (*inputCommand) { 
        if (strstr(inputCommand, charSearch) == inputCommand) { 
            strcpy(&modifiedCommandLine[i], pidString); 
            i = i + pidLen; 
            inputCommand = inputCommand + charSearchLen; 
        } 
        else {
            modifiedCommandLine[i++] = *inputCommand++; 
        }
    } 
    
    modifiedCommandLine[i] = '\0'; 
    return modifiedCommandLine; 
} 

/*
* Function: Check Command
* Parameters: char* inputCommand
* Description: This function will convert the command line into an array of arguments, and then check the entered commands to see whether or not it is an implemented command, 
* it will also set flags
*/
int checkCommand(char* inputCommand) {

    char* arguments[512];
    memset(arguments, 0, sizeof(arguments));

    //Declaring / resetting flags and argument values
    int inputArg = 0;
    int outputArg = 0;
    int numArgs = 0;
    int backgroundFlag = 0;
    int inputFlag = 0;
    int outputFlag = 0;
    int pid = getpid();
    int i;

    //replaces $$ with PID everywhere in the commandline
    char* updatedCommandLine = replaceWord(inputCommand, pid);
    strcpy(inputCommand, updatedCommandLine);
    free(updatedCommandLine);

    //Tokenizes the Command Line and stores each individual argument into a different element in an array
    char* token;
    token = strtok(inputCommand, "\n ");
    while (token != 0) {
        arguments[numArgs] = token;
        numArgs++;
        token = strtok(NULL, "\n ");
    }

    //Checking if & is the last argument in the array
    if (strcmp(arguments[numArgs-1], "&") == 0) {
        backgroundFlag = 1;
        arguments[numArgs-1] = NULL;
        numArgs--;
    }
    //Checking the whole arg array for < or > to change respective flags, will also store the location of the array where the arg lives
    for (i = 0; i < numArgs; i++) {
        if (strcmp(arguments[i], "<") == 0) {
            inputFlag = 1;
            inputArg = i+1;
            arguments[i] = NULL;
        } else if (strcmp(arguments[i], ">") == 0) {
            outputFlag = 1;
            outputArg = i+1;
            arguments[i] = NULL;
        }
    }

    //Checking to see if the command is built in or needs to be passed on
    if (strcmp(arguments[0], "cd") == 0) {
        cd(arguments, numArgs);
        return 1;
    } else if (strcmp(arguments[0], "status") == 0) {
        status();
        return 1;
    } else if (strcmp(arguments[0], "exit") == 0) {
        exit(0);
        return 0;
    } else {
        //Passes the command on
        createChild(arguments, backgroundFlag, inputFlag, outputFlag, inputArg, outputArg);
        return 1;
    }
}

/*
* Function: Check for Child Processes
* Description: This function will check any child processes running and will print the child PID that was terminated and its signal
*/
void checkForChildProcess() {

    pid_t cPID = 0;
    int backGES;
    while ((cPID = waitpid(-1, &backGES, WNOHANG)) > 0) {
        if (cPID > 0) {
            if (WIFEXITED(backGES)) {
                printf("Background PID: %d terminated. Exit value %d\n", (int) cPID, WEXITSTATUS(backGES));
            } else if (WIFSIGNALED(backGES)) {
                printf("Terminated by signal: %d\n", WTERMSIG(backGES));
            }
        }
    }
}

/*
* Function: Command Prompt
* Description: This function will loop and will take inputs from the user command line.
*/
void commandPrompt() {

    char inputCommand[2048];
    int keepLoop = 1;
    do {  

        memset(inputCommand, 0, sizeof(inputCommand));
        checkForChildProcess();
        printf(": ");
        fgets(inputCommand, 2048, stdin);

        if (inputCommand[0] != '\n' && inputCommand[0] != '#') {

            keepLoop = checkCommand(inputCommand);

            if (keepLoop == 0) {
                return;
            } 
        } 
    } while (keepLoop == 1);
}

//Main Function
int main () {

    exitCode = 0;

    //Sets CTRL C to be ignored
    signal(SIGINT, SIG_IGN);

    //Struct Declaration from exploration module (Shows up as errors for me, but it works fine)
    //Sets CTRL Z to turn on and off & working
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL); 

    commandPrompt();

    return 0;
}
