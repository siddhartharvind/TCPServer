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



    // 4. Accept connections
    struct sockaddr_in client_addr;
    int client_len = sizeof client_addr;

    int new_socketfd;
    while ((new_socketfd = accept(
        socket_fd,
        (struct sockaddr *)&client_addr,
        (socklen_t *)&client_len
    )) > 0)
    {

        printf("Connection with %d successfully established!\n", new_socketfd);


        // 5A. Send data to client
        char *message = "Hello connection! Enter something:\n";
        send(new_socketfd, message, strlen(message), 0);


        // 5B. Receive data from client
        char client_message[255];

        int read_size = recv(new_socketfd, &client_message, sizeof client_message, 0);

        if (read_size == 0)
        {
            puts("Client disconnected.");
            fflush(stdout);
        }
        else if (read_size < 0)
        {
            perror("ERROR: recv() failed");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Remove trailing newline -> '\n' or '\r\n'
            // strcspn() -> returns first index of '\r' or '\n'
            // By setting to '\0' => effectively remove from string
            int msg_len = strcspn(client_message, "\r\n");
            client_message[msg_len] = '\0';
            dprintf(new_socketfd, "You said: \"%*s\"\n", msg_len, client_message);
        }
    }

    if (new_socketfd < 0)
    {
        perror("ERROR: Server failed to accept connection");
        exit(EXIT_FAILURE);
    }


    return EXIT_SUCCESS;
}
