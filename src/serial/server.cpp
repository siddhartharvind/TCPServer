/* 
 * A singlethreaded TCP echo server 
 * Usage: tcpserver <port>
 * 
 * Testing:
 * nc localhost <port> < input.txt
 */

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>


int main(int argc, char *argv[])
{
    int portno; /* Port to listen on */
    /* 
    * Check command line arguments
    */
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        std::exit(EXIT_FAILURE);
    }

    // Server port number taken as command line argument
    portno = std::atoi(argv[1]);

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
    server.sin_port        = htons( portno );
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
                        // Do **NOT** use ```value = KV_DATASTORE[key]```
                        // for idempotent reads! If the specified key does not exist,
                        //it inserts it (in this case, with value `""`).

                        // it => std::unordered_map<std::string, std::string>::iterator
                        //    => decltype(KV_DATASTORE)::iterator
                        //    => auto

                        value = "NULL"; // if key not found
                        decltype(KV_DATASTORE)::iterator it = KV_DATASTORE.find(key);
                        if (it != KV_DATASTORE.end()) {
                            // Key found in umap
                            value = it->second;
                        }
                        dprintf(new_socketfd, "%*s\n",
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

                        // Server responds with `FIN` after WRITE
                        std::string resp = "FIN\n";
                        send(new_socketfd, resp.c_str(), resp.length(), 0);
                    }
                    break; // out of switch

                case 'C':
                    if (std::strcmp(client_message, "COUNT") == 0) {
                        // Send no. of key-value pairs in `KV_DATASTORE`
                        dprintf(new_socketfd, "%ld\n", KV_DATASTORE.size());
                    }
                    break; // out of switch

                case 'D':
                    if (std::strcmp(client_message, "DELETE") == 0) {
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
                        auto key_found = KV_DATASTORE.erase(key);
                        // .erase():
                        //  takes key as argument, returns no. of keys erased
                        if (key_found) {
                            value = "FIN\n";
                        } else {
                            value = "NULL\n";
                        }
                        send(new_socketfd, value.c_str(), value.size(), 0);
                    }
                    break; // out of switch

                case 'E':
                    if (std::strcmp(client_message, "END") == 0) {

                        // Close the client socket FD.
                        if (close(new_socketfd) < 0) {
                            std::perror("ERROR: close() on client");
                            std::exit(EXIT_FAILURE);
                        }
                        read_size = 0; // for the `if` at label `after_recv:`
                        goto after_recv;
                        // break;
                        // No break needed here as the `goto` makes the PC
                        // jump out of the `while(recv())` loop itself.
                    }

                default:
                    continue; // while loop

            } // end switch
        } // end while(recv())

    after_recv:
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

        // The server will remain in the `accept()` while loop
        // even after a client ends its connection and control
        // reaches this point.

    } // end `while(accept())`

    if (new_socketfd < 0)
    {
        std::perror("ERROR: Server failed to accept connection");
        std::exit(EXIT_FAILURE);
    }


    return EXIT_SUCCESS;
}
