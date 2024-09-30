#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


int is_pipe(int count, char** arglist) {
    int i;
    for (i = 1; i < count - 1 ; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            return i;
        }
    }
    return -1;
}

void process_fail() {
    fprintf(stderr, "%s", strerror(errno));
    exit(1);
}

int case1(int count, char** arglist){
    // '&' symbol
    int child_pid = fork();

    if (child_pid < 0){
        // child process failed
        process_fail();
    }

    if (child_pid == 0) {
        // child proccess success

        arglist[count - 1] = NULL; // do not pass & to execvp
        if (execvp(arglist[0], arglist) < 0) {
            fprintf(stderr, "%s", strerror(errno));
            exit(1);
        }
    }
    return 1;
}

int case2(int count, char** arglist, int pipe_indx){
    // '|' symbol
    int child_pid_out = fork();
    int status;
    int pipe_fd[2];

    if(pipe(pipe_fd) < 0){
        process_fail();
    }

    if (child_pid_out < 0){
        // child process in or out failed
        process_fail();
    }

    if (child_pid_out == 0) {
        // child proccess out success
        if (signal(SIGINT, SIG_DFL) == SIG_ERR || close(pipe_fd[0]) <0 ||
            dup2(pipe_fd[1], STDOUT_FILENO) < 0 || close(pipe_fd[1]) < 0) {
            process_fail();
        }
        arglist[pipe_indx] = NULL; // do not pass | to execvp
        if(execvp(arglist[0], arglist) < 0){
            process_fail();
        }
    }

    else {
        // parent process
        int child_pid_in = fork();

        if (child_pid_in < 0) {
            // child process in or out failed
            process_fail();
        }

        if (child_pid_in == 0) {
            // child proccess in success
            if (signal(SIGINT, SIG_DFL) == SIG_ERR || close(pipe_fd[1]) < 0 ||
                dup2(pipe_fd[0], STDIN_FILENO) < 0 || close(pipe_fd[0]) < 0) {
                process_fail();
            }
            arglist[pipe_indx] = NULL; // do not pass | to execvp
            if (execvp(arglist[pipe_indx + 1], (arglist + pipe_indx + 1)) < 0) {
                process_fail();
            }
            close(pipe_fd[0]);
            close(pipe_fd[1]);
        }

        else {
            // parent process
            if(signal(SIGINT, SIG_DFL) == SIG_ERR || close(pipe_fd[0]) < 0 || close(pipe_fd[1]) < 0){
                process_fail();
            }
            if ((waitpid(child_pid_out, &status, 0) == -1 && (errno != ECHILD && errno != EINTR))
                || (waitpid(child_pid_in, &status, 0) == -1 && (errno != ECHILD && errno != EINTR))) {
                fprintf(stderr, "%s", strerror(errno));
                return 0;
            }
        }
    }
    return 1;
}

int case3(int count, char** arglist){
    // '<' symbol
    int child_pid = fork();
    int status;

    if (child_pid < 0){
        // child process failed
        process_fail();
    }

    if (child_pid == 0){
        // child proccess success
        int file = open(arglist[count - 1], O_RDONLY );

        if (signal(SIGINT, SIG_DFL) == SIG_ERR|| file < 0 || dup2(file, STDIN_FILENO) < 0 ||
            close(file) < 0) {
            process_fail();
        }
        arglist[count - 2] = NULL; // do not pass > to execvp
        if(execvp(arglist[0], arglist) < 0){
            process_fail();
        }
    }

    else{
        // parent process
        if(waitpid(child_pid, &status, 0) == -1 && (errno!=ECHILD && errno!=EINTR)){
            fprintf(stderr, "%s", strerror(errno));
            return 0;
        }
    }
    return 1;
}

int case4(int count, char** arglist){
    // '>' symbol
    int child_pid = fork();
    int status;

    if (child_pid == 0){
        // child proccess success
        int file = open(arglist[count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);

        if (signal(SIGINT, SIG_DFL) == SIG_ERR || file < 0 || dup2(file, STDOUT_FILENO) < 0 || close(file) < 0) {
            process_fail();
        }
        arglist[count - 2] = NULL; // do not pass > to execvp
        if(execvp(arglist[0], arglist) < 0){
            process_fail();
        }
    }

    if (child_pid < 0){
        // child process failed
        process_fail();
    }
    else{
        // parent process
        if(waitpid(child_pid, &status, 0) == -1 && (errno!=ECHILD && errno!=EINTR)){
            fprintf(stderr, "%s", strerror(errno));
            return 0;
        }
    }
    return 1;
}

int case5(int count, char** arglist){
    // no special symbol
    int child_pid = fork();
    int status;

    if (child_pid < 0){
        // child process failed
        process_fail();
    }

    if (child_pid == 0) {
        // child proccess success
        if(signal(SIGINT, SIG_DFL) == SIG_ERR || (execvp(arglist[0], arglist) < 0)) {
            process_fail();
        }
    }

    else{
        // parent process
        if(waitpid(child_pid, &status, 0) == -1 && (errno!=ECHILD && errno!=EINTR)){
            fprintf(stderr, "%s", strerror(errno));
            return 0;
        }
    }
    return 1;
}


// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char** arglist){
    int special_char = 1;
    int ret = 0;
    // case 1- '&'
    if (special_char && count >= 1 && strcmp(arglist[count - 1], "&") == 0) {
        special_char = 0;
        ret = case1(count, arglist);
    }

    // case 2- '|'
    int pipe_indx = is_pipe(count, arglist);
    if (special_char && count >= 3 && pipe_indx > 0) {
        special_char = 0;
        ret = case2(count, arglist, pipe_indx);
    }

    // case 3- '<'
    if (special_char && count >= 2 && strcmp(arglist[count - 2], "<") == 0) {
        special_char = 0;
        ret = case3(count, arglist);
    }

    // case 4- '>'
    if (special_char && count >= 2 && strcmp(arglist[count - 2], ">") == 0) {
        special_char = 0;
        ret = case4(count, arglist);
    }

    // case 5- no shell symbol
    if (special_char) {
        special_char = 0;
        ret = case5(count, arglist);
    }

    return ret;
}

void handle_sigchild(int signum){
    int* status = NULL;
    while(waitpid((pid_t)(-1), status, WNOHANG) > 0){}
}

// prepare and finalize calls for initialization and destruction of anything required
int prepare(void){

    // handle SIGINT - ignore
    if(signal(SIGINT, SIG_IGN) == SIG_ERR){
        process_fail();
    }

    // handle zombies
    struct sigaction sigchild;
    sigchild.sa_handler = &handle_sigchild;
    sigemptyset(&sigchild.sa_mask);
    sigchild.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if(sigaction(SIGCHLD, &sigchild, NULL) == -1){
        process_fail();
    }

    return 0;
}

int finalize(void){
    return 0;
}

