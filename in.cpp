#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    char* buffer = new char[50];
    
    while(1)
    {
        if (fgets(buffer, 50, stdin) == NULL)
	{
		_exit(1);
	}
        printf("Got from STDIN: %s", buffer);
    }
}
