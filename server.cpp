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
#define CLEANERDELAY 5

static int fieldXSize = 10, fieldYSize = 10;

struct list_node {
private:
    static int counter;
public:
    list_node* next;
    bool* value;
    int clients;
    int num;

    list_node() {
        num = counter++;
        clients = 0;
        next = NULL;
        value = new bool[fieldXSize * fieldYSize];
    }

    virtual ~list_node() {
        delete value;
        printf("Field #%d is dead now.\n", num);
    }
};
int list_node::counter = 0;

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
    list_node *field;

    //struct aiocb my_aiocb;
    //memset(&my_aiocb, 0, sizeof (struct aiocb));
    //my_aiocb.aio_fildes = socket;
    //my_aiocb.aio_nbytes = fieldXSize * fieldYSize * sizeof (bool);
    //my_aiocb.aio_offset = 0;
    int fieldSize = fieldXSize * fieldYSize * sizeof (bool);

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
        //my_aiocb.aio_buf = (void*) currentField->value;
        //aio_write(&my_aiocb);
        field = currentField;
        field->clients++;
        if (!pth_write(socket, field->value, fieldSize)) {
            field->clients--;
            close(socket);
            return NULL;
        }
        field->clients--;
    }
}

int theLifeProcess(void* arg) {
    // Do a delay for a sec and then recalculate the configuration
    list_node* newField;
    while (1) {
        sleep(DELAYSECS);
        newField = new list_node();
        nextConfiguration(currentField->value, newField->value);
        newField->next = currentField;
        currentField = newField;
        printf("State #%d 's been calculated.\n", currentField->num);
    }
    _exit(0);
}

static void *cleaner(void *_arg) {
    list_node* prev, *curr;
    while (1) {
        pth_sleep(CLEANERDELAY);
        prev = currentField;
        while ((curr = prev->next) != NULL) {
            if (curr->clients == 0) {
                prev->next = curr->next;
                delete curr;
            } else prev = curr;
        }

    }
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
    currentField = new list_node();
    for (i = 0; i < fieldXSize * fieldYSize; i++) {
        lifeFile >> currentField->value[i];
    }


    void* child_stack = (void*) malloc(STACKSIZE);
    if (clone(theLifeProcess, child_stack + STACKSIZE - 1, CLONE_VM, NULL) == -1) {
        perror("Error");
        _exit(1);
    }


    // Pth configuring
    pth_attr_t attr;
    pth_init();
    attr = pth_attr_new();
    pth_attr_set(attr, PTH_ATTR_STACK_SIZE, 64 * 1024);
    pth_attr_set(attr, PTH_ATTR_JOINABLE, FALSE);
    pth_attr_set(attr, PTH_ATTR_NAME, "cleaner");

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


    if (pth_spawn(attr, cleaner, NULL) == NULL)
        printf("Cleaner starting failed. There's gonna be memory leaks.");


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
