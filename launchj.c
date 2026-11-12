#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(){
    int fd_console = open("/dev/console", O_RDWR, 0);
    dup2(fd_console, STDOUT_FILENO);
    printf("================ Hello from launchj ================ \n");

    printf("about to prepare execve...\n");

    const char *argv[] = {
        "/sbin/launchd",
        NULL
    };

    const char *envp[] = {
        "DYLD_INSERT_LIBRARIES=/usr/lib/daemonloader.dylib",
        NULL
    };
    printf("Closing console, goodbye!\n");
    /*
    Launchd doesn't like it when the console is open already!
    */
    for (size_t i = 0; i < 10; i++) {
        close(i);
    }
    return execve(argv[0],argv,envp);
}
