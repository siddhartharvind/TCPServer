/* 
 * A multithreaded TCP echo server
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
#include <pthread.h>
#include <queue>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>


// Global variables
// Probably not good practice, but easier for the time being
static std::unordered_map<std::string, std::string> KV_DATASTORE;
static pthread_mutex_t MUTEX_FOR_KV_DATASTORE = PTHREAD_MUTEX_INITIALIZER;

static std::queue<int> client_queue;
static pthread_mutex_t MUTEX_FOR_CLIENT_QUEUE = PTHREAD_MUTEX_INITIALIZER;

#if !defined NUM_THREADS
# define NUM_THREADS 5
#endif

void handle_connection(int client_sock)
{
    std::printf("Connection with %d successfully established!\n", client_sock);

    // Receive data from client
    std::string client_message { "" };
    int read_size;
    // size_t end_pos = 0;

    static const std::string END_MESSAGE = "END\r\n";

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
            return;
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

                    ///////////////////////////////
                    // Start of Critical Section //
                    ///////////////////////////////
                    pthread_mutex_lock(&MUTEX_FOR_KV_DATASTORE);

                    KV_DATASTORE[key] = value;

                    /////////////////////////////
                    // End of Critical Section //
                    /////////////////////////////
                    pthread_mutex_unlock(&MUTEX_FOR_KV_DATASTORE);

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


                    ///////////////////////////////
                    // Start of Critical Section //
                    ///////////////////////////////
                    pthread_mutex_lock(&MUTEX_FOR_KV_DATASTORE);

                    auto key_found = KV_DATASTORE.erase(key);

                    /////////////////////////////
                    // End of Critical Section //
                    /////////////////////////////
                    pthread_mutex_unlock(&MUTEX_FOR_KV_DATASTORE);

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
                    // Final newline for test cases
                    reply << '\n';

                    // Dispatch complete reply to client
                    size_t reply_len = reply.str().length();
                    send(client_sock, reply.str().c_str(), reply_len, 0);

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
}



void *thread_function(void *unused)
{
    while (1)
    {
        // Lock queue access with the mutex
        pthread_mutex_lock(&MUTEX_FOR_CLIENT_QUEUE);
        if (!client_queue.empty())
        {
            // Pop client from queue to start serving it
            int client_sock = client_queue.front();
            client_queue.pop();
            pthread_mutex_unlock(&MUTEX_FOR_CLIENT_QUEUE);

            // Call the function handling the client connection
            handle_connection(client_sock);
        }
        else {
            pthread_mutex_unlock(&MUTEX_FOR_CLIENT_QUEUE);
        }
    }
    pthread_exit(NULL);
}


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

    // Create a socket
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



    // Bind socket to port/address
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


    // Prevents `bind()` from throwing an error by saying "Address already in use"
    // Normally, the socket is still associated with the old connection for a
    // short period before it is available for use again.
    // This setting is useful during development.
    int yes = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
    {
        std::perror("ERROR: setsockopt()");
        std::exit(EXIT_FAILURE);
    }

    if (bind(server_sock, (struct sockaddr *)&server, sizeof server) < 0)
    {
        std::perror("ERROR: Failed to bind socket");
        std::exit(EXIT_FAILURE);
    }

    std::puts("Server successfully bound!");



    // Listen for connections
    if (listen(server_sock, 5) < 0)
    {
        std::perror("ERROR: Server unable to listen on port");
        std::exit(EXIT_FAILURE);
    }

    std::puts("Server successfully listening!\nWaiting for incoming connections...");


    // Accept connections
    struct sockaddr_in client_addr;
    socklen_t          client_socklen = sizeof client_addr;
    int                client_sock;

    // Setting the client threads created with this `attr` to be detached.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // Creating and initializing array of N Pthreads
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_create(
            &threads[i],
            NULL,
            &thread_function,
            &attr
        );
    }


    while (1)
    {
        int *p_client_sock = static_cast<int*>(std::malloc(sizeof(int)));
        // Accept new incoming client
        *p_client_sock = accept(
            server_sock,
            (struct sockaddr *)&client_addr,
            &client_socklen
        );

        if (*p_client_sock < 0) {
            std::perror("ERROR: accept()");
            std::free(p_client_sock);
            continue;
        }

        // Add new client to queue
        pthread_mutex_lock(&MUTEX_FOR_CLIENT_QUEUE);
        client_queue.push(*p_client_sock);
        pthread_mutex_unlock(&MUTEX_FOR_CLIENT_QUEUE);
    }


    // Necessary to ultimately destroy the mutexes. Note that one must
    // initialize a mutex before using it; not doing so is Undefined
    // Behaviour (UB). Destroying the mutex at the end of use is required to
    // be done as part of the Pthreads API.
    pthread_mutex_destroy(&MUTEX_FOR_KV_DATASTORE);
    pthread_mutex_destroy(&MUTEX_FOR_CLIENT_QUEUE);

    pthread_attr_destroy(&attr);

    return EXIT_SUCCESS;
}
