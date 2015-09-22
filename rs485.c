#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

static struct termios oldterminfo;
 
void closeserial(int fd)
{
	//printf("reset speed and termios \n");
    tcsetattr(fd, TCSANOW, &oldterminfo);
	cfsetospeed(&oldterminfo, B9600);
	cfsetispeed(&oldterminfo, B9600);
    
    //printf("closing fd \n");
    if (close(fd) < 0)
        perror("closeserial()");
}
 
int openserial(char *devicename)
{
    int fd;
    struct termios attr;
    struct serial_rs485 rs485conf;

	memset(&attr, 0, sizeof(attr));
    attr.c_iflag = 0;
    attr.c_oflag = 0;
    attr.c_cflag = CS8 | CREAD | CLOCAL;
    attr.c_lflag = 0;
    attr.c_cc[VMIN] = 1;
    attr.c_cc[VTIME] = 5;
    
    //printf("opening tty \n");
    if ((fd = open(devicename, O_RDWR)) == -1) {
        perror("openserial(): open()");
        return 0;
    }
    
    //printf("getting old termios \n");
    if (tcgetattr(fd, &oldterminfo) == -1) {
        perror("openserial(): tcgetattr()");
        return 0;
    }
    
    //printf("flusing fd \n");
    if (tcflush(fd, TCIOFLUSH) == -1) {
        perror("openserial(): tcflush()");
        return 0;
    }
    
    /* Enable RS-485 mode: */
	rs485conf.flags |= SER_RS485_ENABLED;

	/* Write the current state of the RS-485 options with ioctl. */
	//if (ioctl (fd, TIOCSRS485, &rs485conf) < 0) {
	//	perror("Error: TIOCSRS485 ioctl not supported.\n");
	//}
	
	//printf("setting speed \n");
	cfsetospeed(&attr, B115200);
	cfsetispeed(&attr, B115200);
	//printf("setting new termios \n");
	tcsetattr(fd, TCSANOW, &attr); 
    
    return fd;
}
 
int main()
{
    int fd, i;
    char *serialdev = "/dev/ttymxc4";
	char buf [1];
    
	fd = openserial(serialdev);
    if (!fd) {
        fprintf(stderr, "Error while initializing %s.\n", serialdev);
        return 1;
    }

	if(write(fd, "A", 1) < 0) {
		printf("write failed \n");
	}

    
    closeserial(fd);
    return 0;
}
