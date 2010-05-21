#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

void sig_handler(int sig) {
    int m_pid = getpid();
    printf("Process ( pid = %i, gid = %i ) got a sig (%i).\n", m_pid, getpgid(m_pid), sig);
}

void iamchild() {
    while (1) {
        // Pause and wait until get a signal
        pause();
    }
}

int main(int argc, char** argv) {
    int k_forks = 2;
    int m_pid = getpid();

    if (argc > 1) {
        k_forks = atoi(argv[1]);
        if (k_forks < 1) {
            perror("wrong arguments");
            exit(0);
        }
    }

    // let's create a new group
    if (setpgid(0, m_pid) != 0) {
        perror("unable to create a new group");
        exit(0);
    }


    printf("Session number: %i\n", getsid(m_pid));
    printf("Group number: %i\n", m_pid);

    struct sigaction sa;
    memset(&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = sig_handler;
    sigaction(SIGHUP, &sa, 0);


    for (int i = 0; i < k_forks - 1; i++) {
        int fork_result = fork();
        if (fork_result == 0) {
            iamchild();
        } else
            if (fork_result < 0) {
            perror("unable to fork a process");
            exit(0);
        }
    }

    while (1) {
        getc(stdin);
        kill(-m_pid, SIGHUP);
    }

    return 0;
}
