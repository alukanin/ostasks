#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

int main(int argc, char** argv)
{
   
    const char* for_stderr = "A string written into STDERR\n";
    const char* for_stdout = "A string written into STDOUT\n";
    while(1)
    {
        sleep(1);
	fputs(for_stdout, stdout); fflush(stdout);
        fputs(for_stderr, stderr);
    }
    return 0;
}
