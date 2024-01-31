#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unordered_map>


int main(int argc, char *argv[])
{

    static std::unordered_map<std::string, std::string> KV_DATASTORE;

    // 1. Create a socket
    int socket_fd = socket(
        AF_INET,     /* domain: IPv4 */
        SOCK_STREAM, /* type: TCP */
        0            /* protocol: choose automatically 
                        based on `domain` and `type`   */
    );

    if (socket_fd < 0)
    {
        std::perror("ERROR: Could not create socket");
        std::exit(EXIT_FAILURE);
    }

    std::printf("Socket successfully created: %d\n", socket_fd);



    // 2. Bind socket to port/address
    struct sockaddr_in server;

    // Zero out the struct
    std::memset(&server, 0, sizeof server);

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
        std::perror("ERROR: Failed to bind socket");
        std::exit(EXIT_FAILURE);
    }

    std::puts("Server successfully bound!");



    // 3. Listen for connections
    if (listen(socket_fd, 5) < 0)
    {
        std::perror("ERROR: Server unable to listen on port");
        std::exit(EXIT_FAILURE);
    }

    std::puts("Server successfully listening!\nWaiting for incoming connections...");



    // 4. Accept connections
    struct sockaddr_in client_addr;
    socklen_t client_socklen = sizeof client_addr;

    int new_socketfd;
    while ((new_socketfd = accept(
        socket_fd,
        (struct sockaddr *)&client_addr,
        &client_socklen
    )) > 0)
    {

        std::printf("Connection with %d successfully established!\n", new_socketfd);


        // 5A. Send data to client
        const char *message = "Hello connection! Enter something:\n";
        send(new_socketfd, message, std::strlen(message), 0);


        // 5B. Receive data from client
        char client_message[255];
        int read_size;

        while ((read_size = recv(
            new_socketfd,
            &client_message,
            sizeof client_message,
            0
        )) > 0)
        {

            if (read_size == 1) {
                // Only character read is '\n'
                // No need for further action.
                continue; // while loop
            }

            // Zero out '\n' character at position (read_size-1)
            client_message[read_size-1] = '\0';

            static const char *const commands[] = {
                "READ",
                "WRITE",
                "COUNT",
                "DELETE",
                "END",
                NULL
            };

            static std::string key;
            static std::string value;

            char first_char = client_message[0];
            switch (first_char)
            {
                case 'R':
                    if (std::strcmp(client_message, "READ") == 0) {
                        read_size = recv(
                            new_socketfd,
                            &client_message,
                            sizeof client_message,
                            0
                        );
                        if (read_size <= 0) {
                            break; // out of switch => goes to if (read_size) ...
                        }
                        client_message[read_size-1] = '\0';
                        key = client_message;
                        value = KV_DATASTORE[key];

                        dprintf(new_socketfd, "Requested value: \"%*s\"\n",
                            (int)value.length(), value.c_str());
                    }
                    break; // out of switch

                case 'W':
                    if (std::strcmp(client_message, "WRITE") == 0) {
                        read_size = recv(
                            new_socketfd,
                            &client_message,
                            sizeof client_message,
                            0
                        );
                        if (read_size <= 0) {
                            break; // out of switch => goes to if (read_size) ...
                        }
                        client_message[read_size-1] = '\0';
                        key = client_message;

                        // Reading value to be written to key
                        read_size = recv(
                            new_socketfd,
                            &client_message,
                            sizeof client_message,
                            0
                        );
                        if (read_size <= 0) {
                            break; // out of switch => goes to if (read_size) ...
                        }
                        client_message[read_size-1] = '\0';
                        if (client_message[0] != ':' ) {
                            // start of value indicated by ':'
                            // format <KEY><newline>:<VALUE><newline>
                            continue; // while loop
                        }
                        value = client_message + 1; // skip the ':'
                        KV_DATASTORE[key] = value;
                    }
                    break; // out of switch

                case 'C':
                    if (std::strcmp(client_message, "COUNT") == 0) {
                    }
                    break; // out of switch

                case 'D':
                    if (std::strcmp(client_message, "DELETE") == 0) {
                    }
                    break; // out of switch

                case 'E':
                    if (std::strcmp(client_message, "END") == 0) {
                    }
                    break; // out of switch

                    // The reason for the double check (for loop to look
                    // up the command full name + strcmp()) is that using
                    // the first character as the index, like:
                    //     commands[] = { ['R'] = "READ", ... }
                    // inflates the size of the array drastically.
                    /*
                    if (strcmp(client_message, command_str) == 0) {
                        // (sizeof command)-1 => '-1' for NUL character

                        read_size = recv(
                            new_socketfd,
                            &client_message,
                            sizeof client_message,
                            0
                        );
                        if (read_size <= 0) {
                            break; // out of while (read_size > 0)
                        }

                        client_message[read_size-1] = '\0';
                        char key_str[32];
                        strncpy(key_str, client_message, read_size);
                        // we copy over `read_size` chars to include the NUL terminator
                        int key = atoi(key_str);
                        const char *value = database[key];

                        dprintf(new_socketfd, "Requested value: \"%*s\"\n",
                            (int)strlen(value), value);

                        break; // from case 'RWCDE'
                    }
                    */
                default:
                    continue; // while loop

            } // end switch
        } // end while

        if (read_size == 0)
        {
            std::puts("Client disconnected.");
            std::fflush(stdout);
        }
        else if (read_size < 0)
        {
            std::perror("ERROR: recv() failed");
            std::exit(EXIT_FAILURE);
        }   
    }

    if (new_socketfd < 0)
    {
        std::perror("ERROR: Server failed to accept connection");
        std::exit(EXIT_FAILURE);
    }


    return EXIT_SUCCESS;
}
