#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstddef>
#include <netinet/in.h>
#include <netdb.h>
#include "pth.h"
#include <unistd.h>
#include <fstream>
#include <aio.h>
#include <string.h>

#define DELAYSECS 1
#define FIELDSN 15
#define STACKSIZE 65536

static int fieldXSize = 10, fieldYSize = 10;

struct list_node {
    list_node* next;
    bool* value;

    list_node() {
        value = new bool[fieldXSize * fieldYSize];
    }
};

static list_node* currentField;

void nextConfiguration(bool* lifeField, bool* newLifeField) {
    int i, j, nbQ, k;

    for (i = 0; i < fieldXSize * fieldYSize; i++) {
        nbQ = 0;

        if (i % fieldXSize) {
            if (lifeField[i - 1]) nbQ++;
            if (i > fieldXSize)
                if (lifeField[i - 1 - fieldXSize]) nbQ++;
            if (i <= fieldXSize * fieldYSize - fieldXSize)
                if (lifeField[i - 1 + fieldXSize]) nbQ++;
        }

        if (i + 1 % fieldXSize) {
            if (lifeField[i + 1]) nbQ++;
            if (i >= fieldXSize)
                if (lifeField[i + 1 - fieldXSize]) nbQ++;
            if (i < fieldXSize * fieldYSize - fieldXSize)
                if (lifeField[i + 1 + fieldXSize]) nbQ++;
        }

        if (i >= fieldXSize)
            if (lifeField[i - fieldXSize]) nbQ++;
        if (i <= fieldXSize * fieldYSize - fieldXSize)
            if (lifeField[i + fieldXSize]) nbQ++;



        if (!lifeField[i]) {

            if (nbQ == 3)
                newLifeField[i] = true;
            else newLifeField[i] = lifeField[i];
        } else {
            if (nbQ == 2 || nbQ == 3)
                newLifeField[i] = true;
            else newLifeField[i] = false;
        }
    }


}

static void *clientHandler(void *_arg) {


    int socket = (int) _arg;
    int code = 0;
    struct aiocb my_aiocb;
    memset(&my_aiocb, 0, sizeof (struct aiocb));
    my_aiocb.aio_fildes = socket;
    my_aiocb.aio_nbytes = fieldXSize * fieldYSize * sizeof (bool);
    my_aiocb.aio_offset = 0;

    while (1) {
        if (!pth_read(socket, &code, sizeof (int))) {
            close(socket);
            return NULL;
        }
        if (code != 666)
            continue;

        if (!pth_write(socket, &fieldXSize, sizeof (int)) ||
                !pth_write(socket, &fieldYSize, sizeof (int))) {
            close(socket);
            return NULL;
        }
        my_aiocb.aio_buf = (void*) currentField->value;
        aio_write(&my_aiocb);
    }
}

int theLifeProcess(void* arg) {

    // Do a delay for a sec and then recalculate the configuration
    while (1) {
        sleep(DELAYSECS);
        nextConfiguration(currentField->value, currentField->next->value);
        currentField = currentField->next;

    }
    _exit(0);
}

int main(int argc, char** argv) {
    struct sockaddr_in ssInfo;
    struct sockaddr_in peer_addr;
    struct protoent* pe;
    ssInfo.sin_family = AF_INET;
    ssInfo.sin_port = htons(3427);
    ssInfo.sin_addr.s_addr = htonl(INADDR_ANY);
    int i;


    std::ifstream lifeFile("./life.txt");
    lifeFile >> fieldXSize >> fieldYSize;

    // Allocate memory for the Life fields
    list_node* firstField = new list_node();
    firstField->next = firstField;

    list_node* field;
    for (i = 1, currentField = firstField; i < FIELDSN; i++) {
        field = new list_node();
        currentField->next = field;
        currentField = field;
    }
    currentField->next = firstField;

    for (i = 0; i < fieldXSize * fieldYSize; i++) {
        lifeFile >> currentField->value[i];
    }


    void* child_stack = (void*) malloc(STACKSIZE);
    if (clone(theLifeProcess, child_stack + STACKSIZE - 1, CLONE_VM, NULL) == -1) {
        perror("Error");
        _exit(1);
    }

    pth_attr_t attr;
    pth_init();

    // Configure the tcp server listener
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error");
        _exit(1);
    }

    if (bind(serverSocket, (struct sockaddr*) & ssInfo, sizeof (ssInfo)) < 0) {
        perror("Error");
        _exit(1);
    }
    if (listen(serverSocket, 1) < 0) {
        perror("Error");
        _exit(1);
    }

    // Do all the threads configure stuff
    attr = pth_attr_new();
    pth_attr_set(attr, PTH_ATTR_STACK_SIZE, 64 * 1024);
    pth_attr_set(attr, PTH_ATTR_JOINABLE, FALSE);
    pth_attr_set(attr, PTH_ATTR_NAME, "client");
    int peer_len = sizeof (peer_addr);
    while (1) {
        // Waiting for a new client
        int newClient = pth_accept(serverSocket,
                (struct sockaddr*) & peer_addr,
                (socklen_t*) & peer_len);
        if (newClient < 0) {
            perror("Error");
            _exit(1);
        }

        if (pth_spawn(attr, clientHandler, (void*) newClient) == NULL)
        	close(newClient);
    }

    return 0;
}
