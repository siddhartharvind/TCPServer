#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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



    // 2. Bind socket to port/address
    struct sockaddr_in server;

    // Zero out the struct
    memset(&server, 0, sizeof server);

    // Prepare the structure
    server.sin_family      = AF_INET;
    server.sin_port        = htons( 8888 );
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    // The above binds the server to localhost. This means
    // that we can only connect to the server from the local
    // machine. To allow connecting from any interface:
    // server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, (struct sockaddr *)&server, sizeof server) < 0)
    {
        perror("ERROR: Failed to bind socket");
        exit(EXIT_FAILURE);
    }

    puts("Server successfully bound!");



    // 3. Listen for connections
    if (listen(socket_fd, 5) < 0)
    {
        perror("ERROR: Server unable to listen on port");
        exit(EXIT_FAILURE);
    }

    puts("Server successfully listening!\nWaiting for incoming connections...");

    return EXIT_SUCCESS;
}
