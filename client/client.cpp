#include <iostream>
#include <string>
#include <fstream>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define EVENTS 2

// Generator, which based on /dev/urandom file
// He generate pseudo random sequence of bytes, based on strange algorithm in Linux Kernel
// PS: I know, that we can generate values with stdlib facilities
unsigned int generator()
{
    unsigned int seed;
    std::fstream generator;
    generator.open("/dev/urandom", std::ios::in);

    if (!generator.is_open())
    {
        std::cerr << "Generator file unavaliable" << std::endl;
        return seed;
    }

    generator.read(reinterpret_cast<char*>(&seed), sizeof(seed));
    generator.close();
    return seed;
}

std::string generate_math_expression(int n = 2, int max_boundary_power = 5, int max_boundary_value = 101)
{
    std::string operations = "+-*/^";
    std::string expression;

    int sign = (generator() % 2 == 0) ? 1 : -1;
    int random_value = sign * (generator() % max_boundary_value);

    expression += std::to_string(random_value);

    for (int i = 1; i < n; ++i)
    {
        random_value = generator() % operations.size();
        expression += operations[random_value];
        
        if (operations[random_value] == '^')
        {
            sign = (generator() % 2 == 0) ? 1 : -1;
            random_value = sign * generator() % max_boundary_power;
        }
        else
        {
            sign = (generator() % 2 == 0) ? 1 : -1;
            random_value = sign * (generator() % max_boundary_value);
        }

        expression += std::to_string(random_value);
    }

    return expression;
}

bool add_event_in_epoll(int epoll_fd, int event_fd)
{
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = event_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &ev) == -1) 
        return false;
    
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: ./client <count of values in expression> <server_ip_addr> <server_port>" << std::endl;
        return -1;
    }

    int n_values = std::stoi(argv[1]);
    int server_port = std::stoi(argv[3]);

    if (n_values < 2 && n_values > 100)
    {
        std::cerr << "Invalid amount of values in math expression :(" << std::endl;
        return -1;
    }

    if (server_port < 0 && server_port > 65535)
    {
        std::cerr << "Invalid server port :(" << std::endl;
        return -1;
    }

    if (std::system("./trusted_server_access.sh") != 0)
    {
        std::cerr << "Check your internet connection or switch off the VPN" << std::endl;
        std::cerr << "Trusted calculation server inaccessible :(" << std::endl;
        return -1;
    }

    int client_fd = 0;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::cerr << "Socket creation failed :(" << std::endl;
        return -1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port); 

    if (inet_pton(AF_INET, argv[2], &server_addr.sin_addr) <= 0) 
    {
        std::cerr << "Invalid server address :(" << std::endl;
        return -1;
    }

    if (connect(client_fd, (const sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
    {
        std::cerr << "Couldn\'t connect to the server :(" << std::endl;
        return -1;
    }
 
    std::cout << "Connection with server established successfully" << std::endl;
    std::cout << "Testing starts now" << std::endl << std::endl;
    std::string math_expr;

    // Create epoll (now we can watch events, which we want to trace (by file descryptor))
    int epoll_fd = epoll_create1(0);
    
    if (epoll_fd == -1) 
    {
        perror("epoll_create1");
        return -1;
    }

    // watch answers from server (python eval() function)
    if (!add_event_in_epoll(epoll_fd, client_fd))
    {
        perror("epoll_ctr: client_fd");
        return -1;
    }

    // Watch trusted answer from math server 
    int answer_fd = open("answer_client.txt", O_RDONLY | O_NONBLOCK);
    int test_case = 1;

    // Week place !!!
    // Don't use big numbers of test cases, especially infinity loop
    // It will be DoS attack!!! [ which i accidenttally did :) ]
    while (test_case <= 10)
    {
        math_expr = generate_math_expression(n_values);
        pid_t pid_expr_check = fork();

        if (pid_expr_check == 0)
        {
            // Just sent HTTP POST request with params in math server 
            // (PS: first link in browser which i opened [ru.numberempire.com])
            int code = system(("./client_calc.sh " + math_expr).c_str());
            
            // One request-response duration about 1s

            if (code != 0)
            {
                std::cerr << "Calculations failed. Connection loss :(" << std::endl;
                _exit(-1);
            }
            
            _exit(0);
        }
        else
        {
            send(client_fd, math_expr.c_str(), math_expr.size(), 0);
            epoll_event events[EVENTS];

            std::string server_answer;
            std::string script_answer;

            bool server_done = false;
            bool script_done = false;

            while (!(script_done && server_done)) 
            {
                int nfds = epoll_wait(epoll_fd, events, EVENTS, 0); 

                for (int i = 0; i < nfds; ++i) 
                {
                    if (events[i].data.fd == client_fd) 
                    {
                        char buffer[1024] {};

                        recv(client_fd, buffer, sizeof(buffer), 0);

                        int slice = 0;

                        for (int i = 0; i < sizeof(buffer); ++i)
                            if (buffer[i] == '\0')
                            {
                                slice = i;
                                break;
                            }

                        server_answer.append(buffer, slice);
                        server_done = true;
                    }
                }
                
                int status;
                waitpid(pid_expr_check, &status, 0);
        
                if (WIFEXITED(status) && WEXITSTATUS(status) != 0) 
                    return -1;
                else
                {
                    char buffer[1024] {};
                    ssize_t count = read(answer_fd, buffer, sizeof(buffer));
                    script_answer.append(buffer, count);
                    script_done = true;
                }
            }
            
            if (server_answer != script_answer) 
            {
                std::cerr << "Test No" << test_case << " was failed :( [" << math_expr << "]" << std::endl;
                std::cerr << "Expected: " << script_answer << std::endl;
                std::cerr << "  Actual: " << server_answer << std::endl;
            }
            else
                std::cout << "Test No" << test_case << " passed :) [" << math_expr << "]" << std::endl;
        }

        ++test_case;
    }

    return 0;
}