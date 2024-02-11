#include "Tcp_Server.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <sstream>


Tcp_Server::Tcp_Server(int port)
:
    m_port(port),
    MUTEX_FOR_KV_DATASTORE(PTHREAD_MUTEX_INITIALIZER),
    MUTEX_FOR_CLIENT_QUEUE(PTHREAD_MUTEX_INITIALIZER)
{
    // Create a socket
    int server_sock = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );
    if (server_sock < 0) {
        std::perror("ERROR: could not create socket");
        std::exit(EXIT_FAILURE);
    }
    m_socket = server_sock;
    std::printf("Socket successfully created: %d\n", server_sock);

    // Set socket options to reuse address
    int yes = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
        std::perror("ERROR: setsockotp()");
        std::exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    std::memset(&m_sockaddr_info, 0, sizeof(m_sockaddr_info));
    m_sockaddr_info.sin_family = AF_INET;
    m_sockaddr_info.sin_port = htons(m_port);
    m_sockaddr_info.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Set the client threads to be created as detached.
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // Create and initialize array of N Pthreads
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_create(
            &threads[i],
            &attr,
            &thread_function,
            this
        );
    }
}


Tcp_Server::~Tcp_Server()
{
    // Destroy mutexes
    pthread_mutex_destroy(&MUTEX_FOR_KV_DATASTORE);
    pthread_mutex_destroy(&MUTEX_FOR_CLIENT_QUEUE);

    // Destroy thread attribute object
    pthread_attr_destroy(&attr);

    // Close server socket
    close(m_socket);
}


void
Tcp_Server::bind_to_socket(void)
{
    if (bind(
        m_socket,
        (struct sockaddr *)&m_sockaddr_info,
        sizeof(m_sockaddr_info)
    ) < 0)
    {
        std::perror("ERROR: Failed to bind socket");
        std::exit(EXIT_FAILURE);
    }
}


void
Tcp_Server::listen_for_connections(void)
{
    if (listen(m_socket, CONN_REQ_QUEUE_SIZE) < 0)
    {
        std::perror("ERROR: Server unable to listen on port");
        std::exit(EXIT_FAILURE);
    }
}


void
Tcp_Server::accept_connections(void)
{
    // Accept connections
    struct sockaddr_in client_addr;
    socklen_t          client_socklen = sizeof(client_addr);
    int                client_sock;

    while (1)
    {
        int *p_client_sock = static_cast<int *>(std::malloc(sizeof(int)));
        *p_client_sock = accept(
            m_socket,
            (struct sockaddr *)&client_addr,
            &client_socklen
        );

        if (*p_client_sock < 0)
        {
            std::perror("ERROR: accept()");
            std::free(p_client_sock);
            continue;
        }

        // Add new client to queue
        pthread_mutex_lock(&MUTEX_FOR_CLIENT_QUEUE);
        client_queue.push(*p_client_sock);
        pthread_mutex_unlock(&MUTEX_FOR_CLIENT_QUEUE);
    }
}


void
Tcp_Server::start(void)
{
    bind_to_socket();
    listen_for_connections();
    accept_connections();
}


void *
Tcp_Server::thread_function(void *arg)
{
    Tcp_Server *server = static_cast<Tcp_Server *>(arg);
    while (1)
    {
        // Lock queue access with the mutex
        pthread_mutex_lock(&server->MUTEX_FOR_CLIENT_QUEUE);
        if (!server->client_queue.empty())
        {
            // Pop client from queue to start serving it
            int client_sock = server->client_queue.front();
            server->client_queue.pop();
            pthread_mutex_unlock(&server->MUTEX_FOR_CLIENT_QUEUE);

            // Call the function handling the client connection
            server->handle_connection(client_sock);
        }
        else {
            pthread_mutex_unlock(&server->MUTEX_FOR_CLIENT_QUEUE);
        }
    }
    pthread_exit(NULL);
}


void
Tcp_Server::handle_connection(int client_sock)
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

                    value = "NULL"; // if key not found
                    auto iterator = KV_DATASTORE.find(key);
                    if (iterator != KV_DATASTORE.end()) {
                        // Key found in umap
                        value = iterator->second;
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
