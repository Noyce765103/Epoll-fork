#include <iostream>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#define PORT 8111
#define MESSAGE_LEN 1024
#define MAX_EVENTS 20
#define TIMEOUT 500
#define MAX_PROCESS 4
using namespace std;

int main()
{
    int ret = -1;
    int socket_fd, accept_fd;
    int on = 1;
    int backlog = 5;
    int flags;
    struct sockaddr_in localaddr, remoteaddr;
    char in_buff[MESSAGE_LEN] = {0};
    int epoll_fd;
    int event_number;
    epoll_event ev, events[MAX_EVENTS];

    pid_t pid = -1;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        cout << "Failed to create socket!" << endl;
        exit(-1);
    }
    flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    ret = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret == -1)
    {
        cout << "Failed to set socket options!" << endl;
    }
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = PORT;
    localaddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(localaddr.sin_zero), 8);
    ret = bind(socket_fd, (struct sockaddr *)&localaddr, sizeof(sockaddr_in));
    if (ret == -1)
    {
        cout << "Failed to bind addr!" << endl;
        exit(-1);
    }

    ret = listen(socket_fd, backlog);
    if (ret == -1)
    {
        cout << "Failed to listen socket!" << endl;
        exit(-1);
    }

    for (int i = 0; i < MAX_PROCESS; i++)
    {
        if (pid != 0)
        {
            pid = fork();
        }
    }

    if (pid == 0)
    {
        ev.events = EPOLLIN; //侦听一般不用边缘触发
        ev.data.fd = socket_fd;
        epoll_fd = epoll_create(256); //创建epoll文件描述符，在描述符中可以添加时间
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev);
        for (;;)
        {

            event_number = epoll_wait(epoll_fd, events, MAX_EVENTS, TIMEOUT);
            for (int i = 0; i < event_number; i++)
            {
                if (events[i].data.fd == socket_fd)
                {
                    //侦听fd，要建立连接
                    socklen_t addr_len = sizeof(struct sockaddr);
                    accept_fd = accept(socket_fd, (sockaddr *)&remoteaddr, &addr_len);
                    //设置成非阻塞才能加入epoll
                    flags = fcntl(accept_fd, F_GETFL, 0);
                    fcntl(accept_fd, F_SETFL, flags | O_NONBLOCK);
                    ev.events = EPOLLIN | EPOLLET; //数据可以使用边缘触发 ，效率高
                    ev.data.fd = accept_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, accept_fd, &ev);
                }
                else if (events[i].events & EPOLLIN) // 读入数据事件
                {
                    do
                    {
                        memset(in_buff, 0, MESSAGE_LEN);
                        ret = recv(events[i].data.fd, (void *)in_buff, MESSAGE_LEN, 0);
                        if (ret == 0)
                        {
                            close(events[i].data.fd);
                            break;
                        }
                        if (ret == MESSAGE_LEN)
                        {
                            cout << "maybe have data ..." << endl;
                        }

                    } while (ret < 0 && errno == EINTR);
                    if (ret < 0)
                    {
                        switch (errno)
                        {
                        case EAGAIN:
                            break;
                        default:
                            break;
                        }
                    }
                    if (ret > 0)
                    {
                        cout << "recv:" << in_buff << endl;
                        send(events[i].data.fd, (void *)in_buff, MESSAGE_LEN, 0);
                    }
                }
            }
        }
        cout << "quit server ... " << endl;
        close(socket_fd);
    }
    else
    {
        //父进程等待所有子进程结束
        do
        {
            pid = waitpid(-1, NULL, 0);

        } while (pid != -1);
    }
    return 0;
}
