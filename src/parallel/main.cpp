/* 
 * A multithreaded TCP echo server
 * Usage: tcpserver <port>
 * 
 * Testing:
 * nc localhost <port> < input.txt
 */

// Server implementation
#include "Tcp_Server.hpp"

// C `std`-namespaced standard library headers
#include <cstdio>
#include <cstdlib>


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

    Tcp_Server server { portno };
    server.start();
    return EXIT_SUCCESS;
}
