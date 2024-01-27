#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>


int main(int argc, char *argv[])
{
    // 1. Create a socket
    int socket_fd = socket(
        AF_INET,     /* domain: IPv4 */
        SOCK_STREAM, /* type: TCP */
        0            /* protocol: choose automatically 
                        based on `domain` and `type`   */
    );

    if (socket_fd < 0)
    {
        perror("ERROR: Could not create socket");
        exit(EXIT_FAILURE);
    }

    printf("Socket successfully created: %d\n", socket_fd);

    return EXIT_SUCCESS;
}
