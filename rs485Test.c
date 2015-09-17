 /* Main.cpp
 */


/////////////////////////////////////////////////
// Serial port interface program               //
/////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <wait.h>
#include <time.h>   // time calls
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/serial.h>


static struct termios oldterminfo;
typedef unsigned char  Byte;

#define bool unsigned int
#define true 1
#define false 0

#define DEBUG

#ifdef DEBUG
#define dbgprintf  printf
#else
#define dbgprintf(...)
#endif

const char *device = "/dev/ttymxc4";
static int ckecksumError   = 0;
static int rxTimeoutError  = 0;
static int responseError   = 0;
static int  totalErrors    = 0;
static int transmitError = 0;
static int verbosity = 0;
static   int verbosityLevel = 0;
static int stopOnError = 0;

static int iteration = 0;
static unsigned int  command = 0xA8;
unsigned int addr = 0x5;

#define RX_TIMEOUT 1000

#ifdef USE_STTY
int open_port(const char *port)
{
	int fd, flags; // file description for the serial port
	char buf[120];
	unsigned char uport;

	fd = open(port, O_RDWR | O_NDELAY);
	if(fd < 1 ) // if open is unsuccessful
	{
		printf("open_port: Unable to open %s. \n", port);
		exit(1);
	}
	

	memset (buf, 0, 120);
	sprintf(buf, "stty 115200 cread ignbrk -echo -echok -icrnl -ixon -icanon -opost -onlcr clocal time 2 min 0  <  %s", port );
	dbgprintf("\n %s \n", buf );
	system(buf);

	flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
	return(fd);
} //open_port
#else
int open_port(const char *devicename)
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
#endif

//
//
//
void printData( unsigned char *data, int len)
{
	int i = 0;

    if (verbosity > 0 )
    {
	    for (i=0; i < len;  i++) {
		    printf("%02x ", data[i]);
	    }
	    printf("\n");
	}
}
//
void printtty()
{
	system("cat /proc/tty/driver/IMX-uart | grep -e ""4:"" ");
}

//
//

int transmit (int fd, unsigned char *data, int len)   
{
	int  n;
	
	if ( (verbosity > 0 ) && (verbosityLevel >= 10))
	{
	    printf("Transmitting %d bytes:", len );
	    printData(data, len);
	}

	n = write(fd, data, len);  //Send data
	if (n != len )
	{
		printf("\n Sending bytes failed: sent=%d, expected = %d /n", n, len);
	}

	return n;
}

//////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
int receive ( int sioFd,  Byte *RxBuff, int rdSize, int len)
{
    int     n= 0, i =0;
    int     total = 0;
    Byte    lastByte = 0;
    int     rxBytes = 0;
    int     maxWait = 0;
    Byte *pRxBuff = RxBuff;
    int maxRetry = 100;

    // check parms, min read size is 1
    if (0 == rdSize) rdSize = 1; 
  
    while (total < len )
    {
        rxBytes = read(sioFd , pRxBuff, rdSize);

        if ( rxBytes > 0)
        {
            total += rxBytes;
            maxWait = 0;        // reset count
            pRxBuff += rxBytes;
        }
        else
        {
            usleep(RX_TIMEOUT);
            if (maxWait ++ > maxRetry) 
                break;
        }

    }
    return(total);

} //query_modem





int getHexValue( char ch)
{
	struct CHexMap
	{
		char chr;
		int value;
	};
	const int HexMapL = 16;
	int i;
	int val = 0;

	struct CHexMap HexMap[] =
	{
		{'0', 0}, {'1', 1},
		{'2', 2}, {'3', 3},
		{'4', 4}, {'5', 5},
		{'6', 6}, {'7', 7},
		{'8', 8}, {'9', 9},
		{'A', 10}, {'B', 11},
		{'C', 12}, {'D', 13},
		{'E', 14}, {'F', 15}
	};

	for (i=0; i < HexMapL; i++)
	{
	  if (ch == HexMap[i].chr)
	  {
		  val =  HexMap[i].value;
		  break;
	  }
	}
	return val;
}

unsigned char getchecksum(unsigned char *data, int len)
{
	unsigned char checksum = data[0];
	int nb = 1;
	while ( nb  < len) {
		checksum ^= data[nb++];
	}


	return checksum;
}

void processRx ( int sioFd )
{
    unsigned char  rxData[512];
    unsigned char *prxdata = &rxData[0];
    int rxBytes = 0, frameLen=0, total = 0;
    int nbytesToRead;
    int isAnyError = 0;
    
    
    memset(rxData,0,512 );
    
    rxBytes = receive (sioFd, prxdata, 1, 6);
    
    if (rxBytes >= 6)
    {
        frameLen = prxdata[2];
        // Because we have  already received 6 bytes
        nbytesToRead = frameLen - 3;
        prxdata += rxBytes;
        total += rxBytes;
        

        if ( rxBytes < nbytesToRead )
        {

            // Read remaining bytes
            rxBytes =  receive( sioFd, prxdata, 1, nbytesToRead);
            total += rxBytes;
        }
        
        if ( rxBytes >= nbytesToRead )
        {
        	unsigned char csum, csumpkt;
        	csum = getchecksum(rxData,frameLen + 2 );
        	csumpkt = rxData[frameLen + 2];
           if ( csum == csumpkt )
           {
           
             if ( (rxData[1] != addr ) || (rxData[4] != command) )
             {
                responseError++;
                totalErrors++;
                isAnyError = 1;
             }
           }
           else
           {
        	 dbgprintf("!!!!!Checksum error csum=%02x != packet csum=%02x \n", csum, csumpkt);
             ckecksumError ++;
             totalErrors++;
             isAnyError = 1;
           }
        }
        else
        {
        	dbgprintf("!!!!!Received %d bytes(Rx Error : less than %d) \n", rxBytes, nbytesToRead);
            rxTimeoutError++;
            totalErrors++;
             isAnyError = 1;
        }
     }
     else
     {
    	 dbgprintf("!!!!!Received %d bytes(Rx Error : less than 6)\n", rxBytes );
         rxTimeoutError++;
         totalErrors++;
         isAnyError = 1;
     }
    
    if ( (isAnyError) && (verbosity > 0))
    {
    	printf("!!!!!Received %d bytes(Rx Error) :", total );
		printData(rxData, total);
    }

    if ( (!isAnyError) && (verbosityLevel >= 10))
    {
    	printf("Received %d bytes:", total );
    	printData(rxData, total);
    }

    if ( (isAnyError) && (stopOnError))
    {
    	printtty();
    	exit(1);
    }
}
		    
void emptyRxBuffer( int sioFd)
{
	  unsigned char  rxData[10];
	  unsigned char *prxdata = &rxData[0];
	  int rxBytes = 0;

	  do
	  {
		  rxBytes =  receive( sioFd, prxdata, 1, 1);
	  } while (rxBytes > 0 );
}




main (int argc, char **argv) 
{ 
	int loop = -1 ;  // -1 is infinite loop
	int n; 
	char	*addr_tail;
	bool commandFlag = false;
	bool addrFlag = false;
	bool echocmd = false;
	bool dataFlag = false;
	char *pdata;
	int fd;
	unsigned char  data[512];
	int len = 0;
	int	 c;
	int count = 0;
  

	while ((c = getopt(argc, argv, "si:c:a:d:u:P:" )) != EOF) {
			switch (c)
			{
			case 'u':
				device = optarg;
				continue;
			case 'c':
				commandFlag = true;
				command = strtol (optarg,&addr_tail,0);
				continue;;
			case 'i':
				loop = strtol (optarg,&addr_tail,0);
				continue;
			case 's':
				stopOnError = 1;
				continue;
			case 'a':
				addrFlag = 1;
				addr = strtol (optarg,&addr_tail,0);
				continue;
			case 'd':
				dataFlag = true;
				pdata = optarg;
				continue;;
			case 'P':
				verbosity = 1;
				verbosityLevel = strtol (optarg,&addr_tail,0);
				continue;;
			case '?':
			default:
				goto usage;
			}
		}

	printf("Testing device %s --Command = 0x%02x, Device address = %d , loop =%d\n", device, command, addr, loop);
	fd = open_port(device);
	
	if (addrFlag) {
		data[0] = 0;
		data[1] = addr;  // address
		data[2] = 3;     // len = 3
		data[3] = 0;     // status
		data[4] = command;

		if (dataFlag)
		{
			if (  (*pdata = '0' ) &&  (  ( *(pdata+1) == 'x') || ( *(pdata+1) == 'X' )  )  )
			{
				pdata +=2;
			    int pair = 0;
			    unsigned int  dataVal1, dataVal2;
				while (*pdata != '\0') 
				{
				    if (pair == 0 )
				    {
						 dataVal1 = getHexValue(*pdata);
						 pair = 1;
						 pdata++;
					}
					else
					{
					  dataVal2 =  getHexValue(*pdata);
					  data[5+len] =  dataVal2 ;
					  data[5+len] |= (dataVal1 << 4 );
					  pair = 0;
					  pdata++;
					  len++;
					}

				} // while 

			}
		data[2] += len;
	   }
      }

	  // Compute Checksum
	  data[5+len] = getchecksum(data, len + 5);

	  
	  do {
		  system("cat /proc/tty/driver/atmel_serial | grep -e ""1:"" ");
		  n = transmit(fd, data, len+ 6 );
		  if (n == (len+ 6 ) )
		  {
		    iteration++;
		    processRx (fd);
		   }
		   else
		   {
		     transmitError++;
		     totalErrors++;
		   }
		   
		  printf("\nIteration# %d, Total Errors = %d ( Tx=%d,Checksum=%d,timeout=%d, Msg=%d ) \n",
	      iteration,totalErrors ,transmitError, ckecksumError,rxTimeoutError, responseError );
	      emptyRxBuffer(fd);

	     if ( (loop != -1 ) && (loop <= iteration ))
	        break;
	  } while (1);


	close (fd);
	return(0);

usage:
		fprintf(stderr,
			"Usage: %s  -u device  -d addr -c command -d data -P n\n"
			"Options include:\n"
			"  -u /dev/ttyS1    specify USART to use \n"
			"                   (default is  /dev/ttyS1\n"
			"  -c command       one of the commands \n"
			"  -a address       slave device address \n"
			"  -d data          data in Hex ( 0x1133ffac...\n"
			"  -i iteration     how many times the test will run \n"
			"  -P n     print Debug info (n=1 minor ..n = 10 all debug \n"
			, argv[0]);

	
} //main

