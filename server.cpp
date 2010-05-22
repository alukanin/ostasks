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
#include <iostream>
#include <fstream>
#include <aio.h>
#include <string.h>

#define DEBUG
#define DELAYSECS 1
#define FIELDSN 15
#define STACKSIZE 65536
#define CLEANERDELAY 5

/*
 * The Life game state class
 */
class LifeState {
private:
    bool* m_field;
    static int COUNTER;
    static void calculate(bool* lifeField, bool* newLifeField);
    int m_id;
    int m_locks;
public:
    static int FIELD_Y_SIZE;
    static int FIELD_X_SIZE;

    LifeState() {
        m_field = new bool[FIELD_Y_SIZE * FIELD_X_SIZE];
        m_id = COUNTER++;
        m_locks = 0;
    }

    virtual ~LifeState() {
        delete m_field;
#ifdef  DEBUG
        printf("State #%d is dead now.\n", m_id);
#endif
    }

    LifeState* getNext() const {
        LifeState* nextState = new LifeState();
        calculate(m_field, nextState->m_field);
#ifdef  DEBUG
        printf("State #%d has been calculated.\n", nextState->m_id);
#endif
        return nextState;
    }

    const bool* getArray() const {
        return (const bool*) m_field;
    }

    void lock() {
        m_locks++;
    }

    void release() {
        if (m_locks)
            m_locks--;
    }

    bool isFree() const {
        return !((bool) m_locks);
    }

    friend std::istream &operator>>(std::istream& file, LifeState& ls);
};

std::istream &operator>>(std::istream& file, LifeState& ls) {
    for(int i = 0 ; i < LifeState::FIELD_X_SIZE * LifeState::FIELD_Y_SIZE ; i++) {
	file >> ls.m_field[i];    
    }
}

void LifeState::calculate(bool* lifeField, bool* newLifeField) {
    int i, j, nbQ, k;

    for (i = 0; i < FIELD_X_SIZE * FIELD_Y_SIZE; i++) {
        nbQ = 0;

        if (i % FIELD_X_SIZE) {
            if (lifeField[i - 1]) nbQ++;
            if (i > FIELD_X_SIZE)
                if (lifeField[i - 1 - FIELD_X_SIZE]) nbQ++;
            if (i <= FIELD_X_SIZE * FIELD_Y_SIZE - FIELD_X_SIZE)
                if (lifeField[i - 1 + FIELD_X_SIZE]) nbQ++;
        }

        if (i + 1 % FIELD_X_SIZE) {
            if (lifeField[i + 1]) nbQ++;
            if (i >= FIELD_X_SIZE)
                if (lifeField[i + 1 - FIELD_X_SIZE]) nbQ++;
            if (i < FIELD_X_SIZE * FIELD_Y_SIZE - FIELD_X_SIZE)
                if (lifeField[i + 1 + FIELD_X_SIZE]) nbQ++;
        }

        if (i >= FIELD_X_SIZE)
            if (lifeField[i - FIELD_X_SIZE]) nbQ++;
        if (i <= FIELD_X_SIZE * FIELD_Y_SIZE - FIELD_X_SIZE)
            if (lifeField[i + FIELD_X_SIZE]) nbQ++;

    
	if ((lifeField[i] && nbQ == 2) || nbQ == 3) {
		newLifeField[i] = true;
	}
          
            else newLifeField[i] = false;
    }
}


int LifeState::FIELD_Y_SIZE = 10;
int LifeState::FIELD_X_SIZE = 10;
int LifeState::COUNTER = 0;

/*
 *
 */
struct list_node {
    LifeState* value;
    list_node *next;

    list_node() {
        value = NULL;
        next = NULL;
    }
};
static list_node* currentField;

static void *clientHandler(void *_arg) {


    int socket = (int) _arg;
    int code = 0;
    list_node *field;
    int fieldSize = LifeState::FIELD_X_SIZE * LifeState::FIELD_Y_SIZE * sizeof (bool);

    //struct aiocb my_aiocb;
    //memset(&my_aiocb, 0, sizeof (struct aiocb));
    //my_aiocb.aio_fildes = socket;
    //my_aiocb.aio_nbytes = fieldSize;
    //my_aiocb.aio_offset = 0;


    while (1) {
        if (!pth_read(socket, &code, sizeof (int))) {
            break;
        }

        if (code != 666) {
            break;
	}

        if (!pth_write(socket, &LifeState::FIELD_X_SIZE, sizeof (int)) ||
                !pth_write(socket, &LifeState::FIELD_Y_SIZE, sizeof (int))) {
            break;
        }
        field = currentField;
        field->value->lock();
        //my_aiocb.aio_buf = (void*) field->value->getArray();
        //aio_write(&my_aiocb);
        //while (aio_error(&my_aiocb) == EINPROGRESS) {
        //   pth_yield(NULL);
        //}
        if (!pth_write(socket, field->value->getArray(), fieldSize)) {
            field->value->release();
            break;
        }
        field->value->release();
    }
    close(socket);
    return NULL;
}

int theLifeProcess(void* arg) {
    // Do a delay for a sec and then recalculate the configuration
    list_node* newField;
    while (1) {
        sleep(DELAYSECS);
        newField = new list_node();
        newField->next = currentField;
        newField->value = currentField->value->getNext();
        currentField = newField;
    }
    _exit(0);
}

static void *cleaner(void *_arg) {
    list_node* prev, *curr;
    while (1) {
        pth_sleep(CLEANERDELAY);
        prev = currentField;
        while ((curr = prev->next) != NULL) {
            if (curr->value->isFree()) {
                prev->next = curr->next;
                delete curr->value;
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
    lifeFile >> LifeState::FIELD_X_SIZE >> LifeState::FIELD_Y_SIZE;

    // Allocate memory for the Life fields
    currentField = new list_node();
    currentField->value = new LifeState();
    lifeFile >> *(currentField->value);


    void* child_stack = (void*) ((char*) malloc(STACKSIZE) - 1 + STACKSIZE);
    if (clone(theLifeProcess, child_stack, CLONE_VM, NULL) == -1) {
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

