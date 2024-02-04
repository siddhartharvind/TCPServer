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
#include <sstream>
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
    int server_sock = socket(
        AF_INET,     /* domain: IPv4 */
        SOCK_STREAM, /* type: TCP */
        0            /* protocol: choose automatically 
                        based on `domain` and `type`   */
    );

    if (server_sock < 0)
    {
        std::perror("ERROR: Could not create socket");
        std::exit(EXIT_FAILURE);
    }

    std::printf("Socket successfully created: %d\n", server_sock);



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

    if (bind(server_sock, (struct sockaddr *)&server, sizeof server) < 0)
    {
        std::perror("ERROR: Failed to bind socket");
        std::exit(EXIT_FAILURE);
    }

    std::puts("Server successfully bound!");



    // 3. Listen for connections
    if (listen(server_sock, 5) < 0)
    {
        std::perror("ERROR: Server unable to listen on port");
        std::exit(EXIT_FAILURE);
    }

    std::puts("Server successfully listening!\nWaiting for incoming connections...");



    // 4. Accept connections
    struct sockaddr_in client_addr;
    socklen_t client_socklen = sizeof client_addr;

    int client_sock;

accept_connections:
    while ((client_sock = accept(
        server_sock,
        (struct sockaddr *)&client_addr,
        &client_socklen
    )) > 0)
    {

        std::printf("Connection with %d successfully established!\n", client_sock);


        // Receive data from client
        std::string client_message { "" };
        int read_size;
        // size_t end_pos = 0;

        const std::string END_MESSAGE = "END\r\n";

        // while (client_message.find("END", end_pos) == std::string::npos)
        while (client_message.find("END") == std::string::npos)
        {
            char buffer[255];
            read_size = recv(client_sock, &buffer, sizeof(buffer), 0);

            if (read_size == 0)
            {
                // No data sent (e.g. Ctrl+C)
                std::puts("Client disconnected.");
                std::fflush(stdout);
                goto accept_connections;
            }
            else if (read_size < 0)
            {
                std::perror("ERROR: recv() failed");
                std::exit(EXIT_FAILURE);
            }

            client_message += std::string(buffer, read_size);
            // end_pos += client_message.length() - END_MESSAGE.length();
        }

        std::istringstream input { client_message };
        std::ostringstream reply;

        static const char *const commands[] = {
            "READ",
            "WRITE",
            "COUNT",
            "DELETE",
            "END",
            NULL
        };

        std::string command;
        std::string key;
        std::string value;

        while (input >> command)
        {
            char first_char = command[0];
            switch (first_char)
            {
                case 'R':
                    if (command == "READ") {
                        input >> key;

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
                        reply << value << '\n';
                    }
                    break; // out of switch

                case 'W':
                    if (command == "WRITE") {
                        input >> key;
                        input >> value;

                        if (value[0] != ':' ) {
                            // start of value indicated by ':'
                            // format <KEY><newline>:<VALUE><newline>
                            continue; // while loop
                        }

                        value.erase(0, 1); // skip the ':'
                        KV_DATASTORE[key] = value;

                        // Server responds with `FIN` after WRITE
                        reply << "FIN\n";
                    }
                    break; // out of switch

                case 'C':
                    if (command == "COUNT") {
                        // Send no. of key-value pairs in `KV_DATASTORE`
                        reply << KV_DATASTORE.size() << '\n';
                    }
                    break; // out of switch

                case 'D':
                    if (command == "DELETE") {
                        input >> key;
                        auto key_found = KV_DATASTORE.erase(key);
                        // .erase():
                        //  takes key as argument, returns no. of keys erased
                        if (key_found) {
                            value = "FIN\n";
                        } else {
                            value = "NULL\n";
                        }
                        reply << value;
                    }
                    break; // out of switch

                case 'E':
                    if (command == "END") {
                        // Dispatch complete reply to client
                        send(client_sock, reply.str().c_str(), reply.str().length(), 0);

                        // Close the client socket FD.
                        if (close(client_sock) < 0) {
                            std::perror("ERROR: close() on client");
                            std::exit(EXIT_FAILURE);
                        }
                    }

                default:
                    continue; // while loop

            } // end switch
        } // end while(input >> command)
    } // end `while(accept())`

    if (client_sock < 0)
    {
        std::perror("ERROR: Server failed to accept connection");
        std::exit(EXIT_FAILURE);
    }


    return EXIT_SUCCESS;
}
