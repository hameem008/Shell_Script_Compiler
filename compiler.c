#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

// for signal CTRL + C
pid_t child_pid = -1;

const int command_line_maxsize = 100;            // max size of the input line
const int command_maxsize = 50;                  // max size of a command
const int history_cnt = 10;                      // maximum number of command it will sotre
int history_pointer = 0;                         // where the next command will be stored
char history[history_cnt][command_line_maxsize]; // storing 10 previous (max) command
char command_line[command_line_maxsize];         // storing the input command line
int command_semicolon_cnt, command_and_cnt, command_pipeline_cnt;
// how many semicolon in the input command line
// a command contains how many &&
// commands which are separated by |
char command_semicolon[10][command_maxsize]; // storing command separated by ; (these command contains && and |)
char command_and[10][command_maxsize];       // storing command separated by && (these command contains |)
char command_pipeline[10][command_maxsize];  // storing pure command which dosen't contain any && or |
char cmd[command_maxsize];                   // the pure command to store before actually running
// using global array can be helpful because it initializes with '\0' char and also we need them in multiple function
// so no need to pass through function which can make it complex a little bit

void trim(int command_no) // this function take a command and remove all unnecessary ' ' (space)
{
    int space = 0;         // no space is acceptable at the begaining of the command
    int current_index = 0; // for storing the last index of a valid char in a command
    // this loop removes multiple spaces between other charecters by replacing a single space
    for (int i = 0; i < command_maxsize; i++)
    {
        if (command_pipeline[command_no][i] == '\0')
            break;
        else
        {
            if (command_pipeline[command_no][i] == ' ')
            {
                if (space == 1)
                {
                    space = 0;
                    cmd[current_index] = command_pipeline[command_no][i];
                    current_index++;
                }
            }
            else
            {
                space = 1;
                cmd[current_index] = command_pipeline[command_no][i];
                current_index++;
            }
        }
    }
    // removing space if there is any at the last
    current_index--;
    if (cmd[current_index] == ' ')
        cmd[current_index] = '\0';
}

void split_by_pipeline(int command_no) // detecting multiple command separated by |
{
    command_pipeline_cnt = 0;
    int current_index = 0;
    // this loop stores them in command_pipeline array
    for (int i = 0; i < command_maxsize; i++)
    {
        if (command_and[command_no][i] == '\0')
            break;
        else if (command_and[command_no][i] == '|')
        {
            command_pipeline_cnt++;
            current_index = 0;
        }
        else
        {
            command_pipeline[command_pipeline_cnt][current_index] = command_and[command_no][i];
            current_index++;
        }
    }
}

// cmd1 && cmd2
// abc && def

void split_by_and(int command_no) // detecting multiple command separated by &&
{
    command_and_cnt = 0;
    int current_index = 0;
    // this loop stores them in command_and array
    for (int i = 0; i < command_maxsize; i++)
    {
        if (command_semicolon[command_no][i] == '\0')
            break;
        else if (command_semicolon[command_no][i] == '&' && command_semicolon[command_no][i + 1] == '&')
        {
            i++;
            current_index = 0;
            command_and_cnt++;
        }
        else
        {
            command_and[command_and_cnt][current_index] = command_semicolon[command_no][i];
            current_index++;
        }
    }
}

//  abcds arr[0] = a, arr[1] =b
// arr1[0] = arr2[1]
// command_semicolon[0] = abc
// command_semicolon[0][0] = a, command_semicolon[0][15] = b
// command_semicolon[1] = cmd1 command_semicolon[1][0] = a
// cmd0; cmd1; cmd2
// abc; def; ghi

// command_semicolon[0][0] = a
// command_semicolon[0][1] = b
// command_semicolon[0][2] = c
// command_semicolon[1][0] = d

void split_by_semicolon() // detecting multiple command separated by ;
{
    command_semicolon_cnt = 0;
    int current_index = 0;
    // this loop stores them in command_semicolon array
    for (int i = 0; i < command_line_maxsize; i++)
    {
        if (command_line[i] == '\n')
            break;
        else if (command_line[i] == ';')
        {
            current_index = 0;
            command_semicolon_cnt++;
        }
        else
        {
            command_semicolon[command_semicolon_cnt][current_index] = command_line[i];
            current_index++;
        }
    }
}

void run(int command_no)
{
    // initializing as it is also a global array
    for (int i = 0; i < command_maxsize; i++)
        cmd[i] = '\0';
    trim(command_no); // we need to remove unnecessary spaces to run
    // counting the token of a command
    int token_cnt = 0;
    for (int i = 0; i < command_maxsize; i++)
    {
        if (cmd[i] == '\0')
            break;
        else if (cmd[i] == ' ')
            token_cnt++;
    }
    token_cnt++;
    char *args[token_cnt + 1];               // this stores the tokenized command
    char tokens[token_cnt][command_maxsize]; // for storing the tokens
    for (int i = 0; i < token_cnt; i++)      // initializing
        for (int j = 0; j < command_maxsize; j++)
            tokens[i][j] = '\0';

    // tokenizing the command
    int current_token = 0;
    int current_index_in_token = 0;
    int null_index = token_cnt;
    for (int i = 0; i < command_maxsize; i++)
    {
        if (cmd[i] == '\0' || cmd[i] == ' ')
        {
            args[current_token] = tokens[current_token];
            current_token++;
            current_index_in_token = 0;
            if (cmd[i] == '\0')
                break;
        }
        else
        {
            tokens[current_token][current_index_in_token] = cmd[i];
            current_index_in_token++;
        }
    }
    // checking if there is any redirection of input/output
    int redirection_flag = -1; // no redirection -> -1, "<" -> 0, ">" -> 1, ">>" -> 2
    char *file_name;           // redirected file name
    for (int i = 0; i < token_cnt; i++)
    {
        if (redirection_flag != -1)
        {
            // we don't want any redirection part in the tokenized command
            // so we set the flag
            null_index = i - 1;
            file_name = args[i];
        }
        if (strcmp(args[i], "<") == 0)
        {
            redirection_flag = 0;
        }
        else if (strcmp(args[i], ">") == 0)
        {
            redirection_flag = 1;
        }
        else if (strcmp(args[i], ">>") == 0)
        {
            redirection_flag = 2;
        }
    }
    args[null_index] = NULL; // to execute we need a null which is a flag of the ending of a command
    if (redirection_flag != -1)
    {
        // for input redirection
        if (redirection_flag == 0)
        {
            int fd_in = open(file_name, O_RDONLY);
            if (fd_in < 0)
            {
                perror("open input");
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        // for output redirection
        else
        {
            int flags = O_WRONLY | O_CREAT;
            if (redirection_flag == 1)
            {
                flags |= O_TRUNC;
            }
            else if (redirection_flag == 2)
            {
                flags |= O_APPEND;
            }
            int fd_out = open(file_name, flags, 0644); // this number is for giving permission to write
            if (fd_out < 0)
            {
                perror("open output");
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
    }
    // running the tokenized command
    execvp(args[0], args);
    perror("exec failed"); // If exec fails
    exit(1);
}

int run_every_command(int command_no) // run every command in command_and array after spliting them by &&
{
    int ret = 0;
    split_by_pipeline(command_no); // we need to split as this command can contain pipeline
    // creating file descriptor
    // the first process dowsn't need any input file descriptor
    // the last process doesn't need any output file descriptor
    int pipefd[2 * command_pipeline_cnt];
    // (command_pipeline_cnt + 1) number of command are separated by command_pipeline_cnt "|"
    pid_t pids[command_pipeline_cnt + 1];
    // creating all pipes
    for (int i = 0; i < command_pipeline_cnt; i++)
    {
        if (pipe(pipefd + 2 * i) < 0)
        {
            perror("pipe");
            // if there is any error we will go to the end of the function
            ret = 1;
            goto lb;
        }
    }
    // iterating over the command_pipeline array
    for (int j = 0; j <= command_pipeline_cnt; j++)
    {
        pids[j] = fork();
        child_pid = pids[j];
        if (pids[j] == 0)
        {
            // if not the first command, get input from previous pipe
            if (j != 0)
            {
                dup2(pipefd[(j - 1) * 2], STDIN_FILENO);
            }

            // if not the last command, output to next pipe
            if (j != command_pipeline_cnt)
            {
                dup2(pipefd[j * 2 + 1], STDOUT_FILENO);
            }

            // close all pipe in child
            for (int i = 0; i < 2 * command_pipeline_cnt; i++)
            {
                close(pipefd[i]);
            }
            // passing the command no which needed to be execute
            run(j); // running the final command which dosen't contain any &&/;/|
        }
        // initializing the command_pipeline array as it is a global array
        for (int k = 0; k < command_maxsize; k++)
        {
            if (command_pipeline[j][k] == '\0')
                break;
            else
                command_pipeline[j][k] = '\0';
        }
    }
    // closing all the file descriptor
    for (int i = 0; i < 2 * command_pipeline_cnt; i++)
        close(pipefd[i]);
    // capturing the exit code of the last command among the all pipelined command
    int status;
    waitpid(pids[command_pipeline_cnt], &status, 0);
    if (WIFEXITED(status))
        ret = WEXITSTATUS(status);
    else
        ret = 1; // for any abnormal exit
    // wating for all the child to exit
    for (int i = 0; i <= command_pipeline_cnt; i++)
        waitpid(pids[i], NULL, 0);
lb:
    // initializing the command_and array as it is a global array
    for (int k = 0; k < command_maxsize; k++)
    {
        if (command_and[command_no][k] == '\0')
            break;
        else
            command_and[command_no][k] = '\0';
    }
    return ret;
}

void run_every_line() // running every command which are separated by ;
{
    int ret = 0;
    for (int i = 0; i <= command_semicolon_cnt; i++)
    {
        split_by_and(i); // command can contain &&, so we need to split by && in every command stored in command_semicolon array
        // iterating over the command_and array
        for (int i = 0; i <= command_and_cnt; i++)
        {
            ret = run_every_command(i); // after spliting by && all those command we need to run
            // if one command found with abnormal exit, rest of the command will not run, as there are && between them
            if (ret != 0)
                break;
        }
        // this loop initialize the command_semicolon as it is global varialbe and reusable
        for (int k = 0; k < command_maxsize; k++)
        {
            if (command_semicolon[i][k] == '\0')
                break;
            else
                command_semicolon[i][k] = '\0';
        }
    }
}

void get_command()
{
    fgets(command_line, sizeof(command_line), stdin); // taking the input
    // this history stores the last given command, not the last successfully executed command
    // if it is a history command
    if (strcmp(command_line, "history\n") == 0)
    {
        int curr_history_pointer = history_pointer;
        int curr_history_cnt = history_cnt;
        // printing all the stored previous command
        while (curr_history_cnt--)
        {
            curr_history_pointer--;
            // this 2d charecter array (history) is working like a circular array
            if (curr_history_pointer < 0)
                curr_history_pointer += history_cnt;
            for (int i = 0; i < command_line_maxsize; i++)
            {
                if (history[curr_history_pointer][i] == '\0')
                    break;
                printf("%c", history[curr_history_pointer][i]);
            }
        }
    }
    // if it is a normal command
    else
    {
        // storing the current command in position of current pointer
        for (int i = 0; i < command_line_maxsize; i++) // initializing the slot, as it can have any previous data
        {
            if (history[history_pointer][i] == '\0')
                break;
            history[history_pointer][i] = '\0';
        }
        for (int i = 0; i < command_line_maxsize; i++)
        {
            history[history_pointer][i] = command_line[i];
            if (command_line[i] == '\n') // breaking the loop as it's the end of a command
                break;
        }
        history_pointer++;
        // pointer goes back to zero, as it is working like a circular array
        if (history_pointer == history_cnt)
            history_pointer = 0;
        split_by_semicolon(); // spliting the command using semicolon and store them in command_semicolon array
        run_every_line();     // iterating over the command_semicolon array and running all command which are separated by ;
    }
    // initializing this array every time after the execution of a command to avoid error
    for (int i = 0; i < command_line_maxsize; i++)
    {
        if (command_line[i] == '\0')
            break;
        command_line[i] = '\0';
    }
}

void sigint_handler(int signo)
{
    if (child_pid > 0)
    {
        printf("\n");
        // killing the current process
        kill(child_pid, SIGINT);
    }
}

int main()
{
    // Set up shell's SIGINT handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    while (1)
    {
        printf("sh> ");
        get_command(); // geting the command from terminal, this should be in a infinity loop
    }
}

// copule of things in this code snippet has written with the help of chatgpt because of syntactical complexity
// signal handler(in main), execution of a single command(in run), execution of several pipelined command (in run_every_command)
// getting the exit code of a child process (in run_every_command)