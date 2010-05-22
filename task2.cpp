#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

/*
 * process_A's default streams redirecting:
 * 	process_A:out - > process_B:in
 * 	process_A:err - > process_C:in
 */

#define BUFSIZE 200
int pipe_out_to_in[2];
int pipe_err_to_in[2];
int child1_pid, child2_pid, child3_pid;

void error() {
    printf("Use it this way: \"prA | prB prC\"\n");
}

inline void termprint(int pid)
{
	printf("Process %d terminated.\n", pid);
}

void myclose(int p) {
    switch (p) {
        case 3 :
            kill(0, SIGKILL);
        case 2 :
            close(pipe_err_to_in[0]);
            close(pipe_err_to_in[1]);
        case 1 :
            close(pipe_out_to_in[0]);
            close(pipe_out_to_in[1]);
    }
}

int main() {
    // Get an input string
    char* inpStr = new char[BUFSIZE];


    while (1) {
        printf("> ");
        memset(inpStr, 0, BUFSIZE * sizeof (char));
        fgets(inpStr, BUFSIZE, stdin);
        int str_length = strlen(inpStr);
        if (str_length == 1)
            _exit(0);

        str_length--;
        inpStr[str_length] = 0;

        int prA_offset = 0, prB_offset = 0, prC_offset = 0;
        int state = 0;

        // Parse the string
        for (int i = 0; i < str_length; i++) {
            switch (state) {
                case 0:

                    if (inpStr[i] != ' ') {
                        prA_offset = i;
                        state++;
                    }
                    break;
                case 1:

                    if (inpStr[i] == ' ')
                        inpStr[i] = 0;
                    else if (inpStr[i] == '|') {
                        state++;
                        inpStr[i] = 0;
                    } else if (inpStr[i - 1] == 0)
                        state = -1;
                    break;
                case 2:

                    if (inpStr[i] != ' ') {
                        state++;
                        prB_offset = i;
                    }
                    break;
                case 3:

                    if (inpStr[i] == ' ') {
                        inpStr[i] = 0;
                        state++;
                    }
                    break;
                case 4:

                    if (inpStr[i] != ' ') {
                        state++;
                        prC_offset = i;
                    }
                    break;
                case 5:

                    if (inpStr[i] == ' ') {
                        inpStr[i] = 0;
                        state++;
                    }
                    break;
                case 6:
                    if (inpStr[i] != ' ')
                        state = -1;

            }
        }

        if (state < 5) {
            error();
            continue;
        }

        printf("A: %s\nB: %s\nC: %s\n\n",
                (inpStr + prA_offset),
                (inpStr + prB_offset),
                (inpStr + prC_offset));

        // Create pipes
        if (pipe(pipe_out_to_in)) {
            error();
            continue;
        }

        if (pipe(pipe_err_to_in)) {
            error();
            myclose(1);
            continue;
        }

        // Create all the processes
        int child1_pid = fork();
        if (child1_pid == 0) {
            // It's process_A
            if (dup2(pipe_out_to_in[1], STDOUT_FILENO) == -1) {
                perror(inpStr + prA_offset);
                exit(0);
            }
            if (dup2(pipe_err_to_in[1], STDERR_FILENO) == -1) {
                perror(inpStr + prA_offset);
                exit(0);
            }
            myclose(2);
            execlp(inpStr + prA_offset, inpStr + prA_offset, NULL);
            exit(0);
        } else if (child1_pid < 0) {
            error();
            myclose(2);
            continue;
        }

        child2_pid = fork();
        if (child2_pid == 0) {
            // It's process_B
            if (dup2(pipe_err_to_in[0], STDIN_FILENO) == -1) {
                perror(inpStr + prA_offset);

            }
            myclose(2);
            execlp(inpStr + prB_offset, inpStr + prB_offset, NULL);
            exit(0);
        } else if (child2_pid < 0) {
            error();
            myclose(3);
            continue;
        }

        child3_pid = fork();
        if (child3_pid == 0) {
            // It's process_C
            if (dup2(pipe_out_to_in[0], STDIN_FILENO) == -1) {
                perror(inpStr + prA_offset);
                exit(0);
            }
            myclose(2);
            execlp(inpStr + prC_offset, inpStr + prC_offset, NULL);
            exit(0);
        } else if (child3_pid < 0) {
            error();
            myclose(3);
            continue;
        }

	myclose(2);
        int status;
        child1_pid = wait(&status);
        //termprint(child1_pid);
	child1_pid = wait(&status);
	//termprint(child1_pid);
        child1_pid = wait(&status);
	//termprint(child1_pid);
        //myclose(2);

    }


    _exit(0);
}
