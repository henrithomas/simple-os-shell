//
//  myShell.c
//  OS - HW3
//
//  Created by Henri Thomas on 10/6/17.
//
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#include <ctype.h>
#define BUFFER_TOKENS 32
#define BUFFER_IO 1
#define DELIM_METACHAR "><|"
#define DELIM_WHITESPACE " \t\r\n\a"

int run_Standard(char **args)    //Run a standard terminal command
{
    
    int pid;
    if((pid = fork()) == -1){perror("myshell: rff - unable to fork");}
    if(pid == 0) //CHILD
    {
        int err = execvp(args[0], args);
        perror("myshell: rff - could not execvp.");
        if(err < 0)
        {
            return 0;
        }
        free(args);
    }
    int stat;
    waitpid(pid,&stat,0);
    return 1;
}

int run_RedirectToFile(char **args, char *fileOut) //Redirect output to file
{
    int pid, fOut;
    if((pid = fork()) == -1){perror("myshell: rtf - unable to fork");}
    else if(pid == 0) //CHILD
    {
        if((fOut = open(fileOut, O_CREAT | O_WRONLY, 0644)) < 1)
        {
            perror(fileOut);
            return 0;
        }
        dup2(fOut, STDOUT_FILENO);
        close(fOut);
        int err = execvp(args[0], args);
        perror("myshell: rtf - could not execvp.");
        if(err < 0)
        {
            return 0;
        }
        free(args);
    }
    else
    {
        wait(NULL);
    }
    return 1;
}

int run_RedirectFromFile(char **args, char *fileIn) //Take in input from a file
{
    int pid, fOut;
    if((pid = fork()) == -1){perror("myshell: rff - unable to fork");}
    else if(pid == 0) //CHILD
    {
        if((fOut = open(fileIn, O_RDONLY)) < 1)
        {
            perror(fileIn);
            return 0;
        }
        dup2(fOut, STDIN_FILENO);
        close(fOut);
        int err = execvp(args[0], args);
        perror("myshell: rff - could not execvp.");
        if(err < 0)
        {
            return 0;
        }
        free(args);
    }
    else
    {
        wait(NULL);
    }
    return 1;
}

int run_Pipe(char **args0, char **args1)
{
    int pipefd[2];
    int pid0, pid1;
    if(pipe(pipefd) == -1)    //Create a pipe for the two children
    {
        perror("myshell: pipe - could not pipe");
        return 0;
    }
    
    if((pid1 = fork()) == -1)
    {
        perror("myshell: pipe - unable to fork");
        return 0;
    }
    if(pid1 == 0)    //CHILD for left command
    {
        
        close(pipefd[0]);
        if(dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
            perror("myshell: pipe - child 1 dup2 error");
            return 0;
        }
        
        int err0 = execvp(args0[0], args0);
        
        perror("myshell: pipe - child 1 could not execvp");
        if(err0 < 0)
        {
            return 0;
        }
        close(pipefd[1]);
    }
    else
    {
        
        if((pid0 = fork()) == -1)
        {
            perror("myshell: pipe - unable to fork");
            return 0;
        }
        if(pid0 == 0)    //CHILD for right command
        {
            close(pipefd[1]);
            if(dup2(pipefd[0],STDIN_FILENO) == -1)
            {
                printf("myshell: pipe - child 2 dup2 error");
                return 0;
            }
            
            int err1 = execvp(args1[0], args1);
            
            if(err1 < 0)
            {
                printf("myshell: pipe - child 2 could not execvp");
                return 0;
            }
            close(pipefd[0]);
        }
        else        //Parent cleans up the children
        {
            int stat0;
            int stat1;
            close(pipefd[1]);
            close(pipefd[0]);
            free(args1);
            free(args0);
            waitpid(pid1,&stat1,0);
            waitpid(pid0,&stat0,0);
        }
    }
    
    return 1;
}


char *trim_WhiteSpaceFileName(char *line)    //Trim whitespace from filename
{
    char *trimmed = malloc(strlen(line) * sizeof(char*));
    int linePos = 0, trimPos = 0;
    while(line[linePos] != '\0')
    {
        if(!isspace(line[linePos]))
        {
            trimmed[trimPos] = line[linePos];
            trimPos++;
        }
        linePos++;
    }
    trimmed[trimPos] = '\0';
    trimmed = realloc(trimmed, sizeof(trimmed));
    return trimmed;
}

char **parse_WhiteSpace(char *line)    //Tokenize the command string
{
    int buff = BUFFER_TOKENS, numTokes = 0;
    char **tokens_WhiteSpace = malloc(buff * sizeof(char*));
    char *toke;
    int tokePos = 0;
    
    if(tokens_WhiteSpace == NULL)
    {
        printf("myshell: parse-ws: init - memory allocation error\n");
        exit(EXIT_FAILURE);
    }
    toke = strtok(line, DELIM_WHITESPACE);
    while(toke != NULL)
    {
        numTokes++;
        tokens_WhiteSpace[tokePos] = toke;
        tokePos++;
        if(tokePos >= buff)
        {
            buff += BUFFER_TOKENS;
            tokens_WhiteSpace = realloc(tokens_WhiteSpace, buff * sizeof(char*));
            if(tokens_WhiteSpace == NULL)
            {
                printf("myshell: parse-ws: realloc - memory allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        toke = strtok(NULL, DELIM_WHITESPACE);
    }
    tokens_WhiteSpace[tokePos] = NULL;
    return tokens_WhiteSpace;
}

char **parse(char *line, int lineSize)    //Split up the input, searching for metacharacters
{
    char **tokens_MetaC = malloc(BUFFER_TOKENS * sizeof(char*));
    char *toke = malloc(sizeof(line));
    char metaC = '0';
    int tokePos = 1;
    int buff = BUFFER_TOKENS;
    char c;
    if(tokens_MetaC == NULL)
    {
        printf("myshell: parse-mc: init - memory allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    for(int i = 0; i < lineSize; i++)    //Check for metacharacters
    {
        if(line[i] == '>')
        {
            metaC = '>';
        }
        else if(line[i] == '<')
        {
            metaC = '<';
        }
        else if(line[i] == '|')
        {
            metaC = '|';
        }
    }
    
    if(metaC != '0')    //Tokenize based on metacharacters
    {
        tokens_MetaC[0] = &metaC;
        toke = strtok(line,DELIM_METACHAR);
        while(toke != NULL)
        {
            tokens_MetaC[tokePos] = toke;
            tokePos++;
            
            if(tokePos >= buff)
            {
                buff += BUFFER_TOKENS;
                tokens_MetaC = realloc(tokens_MetaC, buff * sizeof(char*));
                if(tokens_MetaC == NULL)
                {
                    printf("myshell: parse-mc: realloc - memory allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }
            toke = strtok(NULL, DELIM_METACHAR);
        }
    }
    else
    {
        tokens_MetaC = realloc(tokens_MetaC,(strlen(line) + 1) * sizeof(char*));
        tokens_MetaC[0] = &metaC;
        tokens_MetaC[1] = line;
    }
    return tokens_MetaC;
}

char *read_LineInput(void)    //Snag the input line
{
    char *line = NULL;
    size_t buffer = 0;
    getline(&line, &buffer, stdin);
    return line;
}

int changeDir(char *dir)    //Change directory
{
    
    if (dir == NULL)
    {
        printf("myshell: cd: expected argument for cd\n");
    }
    else
    {
        if (chdir(dir) != 0)
        {
            printf("myshell: cd: unable to cd\n");
        }
        
    }
    return 1;
}

int exec_Shell(char **args_MetaC, char metaChar)    //Run all forms of supported inputs
{
    char **args0, **args1, *fileName;
    if(metaChar == '0')    //Run a standard input or directory change
    {
        args0 = parse_WhiteSpace(args_MetaC[1]);
        if(strcmp(args0[0], "cd") == 0)
        {
            return changeDir(args0[1]);
        }
        else
        {
            return run_Standard(args0);
        }
    }
    else if(metaChar == '>')    //Redirect output to a file
    {
        args0 = parse_WhiteSpace(args_MetaC[1]);
        fileName = trim_WhiteSpaceFileName(args_MetaC[2]);
        printf("file: %s\n",fileName);
        return run_RedirectToFile(args0,fileName);
    }
    else if(metaChar == '<')    //Redirect file to input
    {
        args0 = parse_WhiteSpace(args_MetaC[1]);
        fileName = trim_WhiteSpaceFileName(args_MetaC[2]);
        printf("file: %s\n",fileName);
        return run_RedirectFromFile(args0,fileName);
    }
    else if(metaChar == '|')    //Pipe between two commands
    {
        args0 = parse_WhiteSpace(args_MetaC[1]);
        args1 = parse_WhiteSpace(args_MetaC[2]);
        return run_Pipe(args0,args1);
    }
    else
    {
        return 1;
    }
}

void loop_MyShell(void)       //Loop forever until the status changes, or exit
{
    char *lineIn;
    char **args_MetaChar;
    int lineSize = 0;
    int status = 1;
    
    while(status)
    {
        printf("my-shell$ ");
        
        lineIn = read_LineInput();    //Take input from terminal
        lineSize = strlen(lineIn);    //Get the size of the line
        args_MetaChar = parse(lineIn, lineSize);    //Parse the input
        status = exec_Shell(args_MetaChar,*args_MetaChar[0]);    //Execute the commands
        free(lineIn);
        free(args_MetaChar);
    }
}

int main(int argc, char **argv)       //Execute the shell
{
    loop_MyShell();
    return 0;
}


