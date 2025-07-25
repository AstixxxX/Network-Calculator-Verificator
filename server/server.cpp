#include <iostream>
#include <fstream>
#include <string>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define EVENTS 10
#define BUFFER_SIZE 1024

void set_nonblocking(int sockfd) 
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char** argv) 
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./server <listen_port>" << std::endl;
        return -1;
    }

    int listen_port = std::stoi(argv[1]);

    if (listen_port < 0 && listen_port > 65535)
    {
        std::cerr << "Invalid server port :(" << std::endl;
        return -1;
    }

    int server_fd, epoll_fd, nfds;
    sockaddr_in address;
    epoll_event ev, events[EVENTS];
    char buffer[BUFFER_SIZE] {};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::cerr << "Socket creation failed :(" << std::endl;
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(listen_port);

    if (bind(server_fd, (const sockaddr*)&address, sizeof(address)) < 0) 
    {
        std::cerr << "Invalid server address :(" << std::endl;
        return -1;
    }

    if (listen(server_fd, 5) < 0) 
    {
        perror("listen failed");
        return -1;
    }

    std::cout << "Server listening on port " << listen_port << std::endl << std::endl;

    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) 
    {
        perror("epoll_create1");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) 
    {
        perror("epoll_ctl: server_fd");
        return -1;
    }

    while (1) 
    {
        nfds = epoll_wait(epoll_fd, events, EVENTS, 0);

        for (int n = 0; n < nfds; ++n) 
        {
            if (events[n].data.fd == server_fd) 
            {
                int new_socket;
                socklen_t addrlen = sizeof(address);

                if ((new_socket = accept(server_fd, (sockaddr *)&address, &addrlen)) < 0) 
                {
                    perror("accept");
                    return -1;
                }

                set_nonblocking(new_socket);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = new_socket;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &ev) == -1) 
                {
                    perror("epoll_ctl: new_socket");
                    return -1;
                }

                std::cout << "New connection from " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
                std::cout << "Testing starts now" << std::endl << std::endl;
            } 
            else 
            {
                int client_fd = events[n].data.fd;
                ssize_t n = read(client_fd, buffer, BUFFER_SIZE);
                
                std::string math_expr = std::string(buffer, n);
                
                if (system(("./server_calc.sh " + math_expr).c_str()) != 0)
                {
                    std::cerr << "Server calculator down :(" << std::endl;
                    close(server_fd);
                    return -1;
                }

                int answer_fd = open("answer_server.txt", O_RDONLY | O_NONBLOCK);

                for (int i = 0; i < BUFFER_SIZE; ++i)
                    buffer[i] = 0;
                
                read(answer_fd, buffer, BUFFER_SIZE);
                
                for (int i = 0; i < BUFFER_SIZE; ++i)
                    if (buffer[i] == '\n')
                    {
                        buffer[i] = '\0';
                        break;
                    }
                    
                if (send(client_fd, buffer, BUFFER_SIZE, 0))
                    std::cout << "Calculations was send successfully" << std::endl;
                else
                {
                    std::cerr << "Connection with client was lost" << std::endl;
                    close(server_fd);
                    return 0;
                }
            }
        }
    }

    close(server_fd);
    return 0;
}