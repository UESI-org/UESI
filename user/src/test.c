#include <syscall.h>
#include <string.h>

int main(void) {
    const char *msg_parent = "Parent process\n";
    const char *msg_child = "Child process\n";
    const char *msg_fork_failed = "Fork failed\n";
    
    write(1, "About to fork...\n", 17);
    
    pid_t pid = fork();
    
    if (pid < 0) {
        write(1, msg_fork_failed, strlen(msg_fork_failed));
        exit(1);
    } else if (pid == 0) {
        write(1, msg_child, strlen(msg_child));
        write(1, "Child: My PID is ", 17);
        
        pid_t my_pid = getpid();
        char buf[32];
        int i = 0;
        do {
            buf[i++] = '0' + (my_pid % 10);
            my_pid /= 10;
        } while (my_pid > 0);
        
        for (int j = 0; j < i/2; j++) {
            char tmp = buf[j];
            buf[j] = buf[i-1-j];
            buf[i-1-j] = tmp;
        }
        buf[i] = '\n';
        
        write(1, buf, i+1);
        exit(0);
    } else {
        write(1, msg_parent, strlen(msg_parent));
        write(1, "Parent: Child PID is ", 21);
        
        char buf[32];
        int i = 0;
        do {
            buf[i++] = '0' + (pid % 10);
            pid /= 10;
        } while (pid > 0);
        
        for (int j = 0; j < i/2; j++) {
            char tmp = buf[j];
            buf[j] = buf[i-1-j];
            buf[i-1-j] = tmp;
        }
        buf[i] = '\n';
        
        write(1, buf, i+1);
        
        write(1, "Parent exiting\n", 15);
        exit(0);
    }
}
