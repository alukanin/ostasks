#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

int fieldXSize = 10;
int fieldYSize = 10;
bool* field;

void myexit() {
    perror("Error");
    _exit(1);
}

int main() {
    int sock, i, j;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        myexit();
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(3427);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    field = new bool[fieldXSize * fieldYSize];

    if (connect(sock, (struct sockaddr *) & addr, sizeof (addr)) < 0) {
        myexit();
    }

    int getit = 666;

    while (1) {


        if (!send(sock, &getit, sizeof (int), 0))
            myexit();
        if (!recv(sock, &fieldXSize, sizeof (int), 0))
            myexit();
        if (!recv(sock, &fieldYSize, sizeof (int), 0))
            myexit();
        if (!recv(sock, field, sizeof (bool) * fieldXSize * fieldYSize, 0))
            myexit();

        int k = 0;
        for (j = 0; j < fieldYSize; j++) {
            for (i = 0; i < fieldXSize; i++) {

                if (field[k]) printf(" o");
                else printf(" *");
                k++;
            }
            printf("\n");
        }

        fflush(stdout);
        sleep(1);
        printf("\n\n\n");
    }

    close(sock);
    return 0;
}
