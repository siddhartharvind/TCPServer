#if !defined TCP_SERVER_HPP
#define TCP_SERVER_HPP


#include <netinet/in.h>
#include <pthread.h>
#include <queue>
#include <string>
#include <unordered_map>


class Tcp_Server
{
public:
    int m_port;
    int m_socket;
    struct sockaddr_in m_sockaddr_info;

    pthread_attr_t attr;
    pthread_mutex_t MUTEX_FOR_KV_DATASTORE;
    pthread_mutex_t MUTEX_FOR_CLIENT_QUEUE;

    static constexpr int NUM_THREADS         = 5;
    static constexpr int CONN_REQ_QUEUE_SIZE = 5;

    pthread_t threads[NUM_THREADS];
    std::queue<int> client_queue;

    std::unordered_map<std::string, std::string> KV_DATASTORE;

    void handle_connection(int client_sock);

    static void *thread_function(void *arg);

    Tcp_Server(int port);

    ~Tcp_Server();

    void bind_to_socket(void);

    void listen_for_connections(void);

    void accept_connections(void);

    void start(void);
};


#endif /* if !defined TCP_SERVER_HPP */
