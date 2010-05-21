#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    char* buffer = new char[50];
    while(1)
    {
        fgets(buffer, 50, stdin);
        printf("Got from STDIN: %s", buffer);
    }
}
