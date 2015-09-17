 #include <stdio.h> // standard input / output functions
#include <string.h> // string function definitions
#include <stdlib.h>
#include <unistd.h> // UNIX standard function definitions
#include <fcntl.h> // File control definitions
#include <errno.h> // Error number definitions
#include <termios.h> // POSIX terminal control definitionss
#include <time.h>   // time calls
#include <unistd.h>



#define bool unsigned int
#define true 1
#define false 0

//#define DEBUG
//#define CONTINUOUS_TRANSMIT

#ifdef DEBUG
#define dbgprintf  printf
#else
#define dbgprintf(...)
#endif



// Transmit and receive buffers
#define MAX_BUFFER 250
unsigned char rxBuffer[MAX_BUFFER];
unsigned char txBuffer[MAX_BUFFER];

bool dumpRxFlag = true;



void initPort(const char *port)
{
    char buf[120];
    memset (buf, 0, 120);
    //sprintf(buf, "stty 115200 sane -echo -echok -icrnl -ixon -icanon -opost -onlcr time 3 min 0 <  %s", port );
     sprintf(buf, "stty 115200 raw cread ignbrk -echo -echok -icrnl -ixon -icanon -opost -onlcr clocal time 3  min 0  <  %s", port );
    dbgprintf("\n %s \n", buf );
    system(buf);
}

int open_port(const char *port)
{
	int fd, flags; // file description for the serial port
	unsigned char uport;

	fd = open(port, O_RDWR | O_NDELAY);
	if(fd < 1 ) // if open is unsuccessful
	{
		printf("open_port: Unable to open %s. \n", port);
		exit(1);
	}
	
    initPort(port);

	flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
	printf("port %s is open with FD = %d\n", port, fd );
	uport = port[9];

	dbgprintf("\n Uport Index %c \n", uport );
	return(fd);
} //open_port

//
//
//
//
//
//
void printData( unsigned char *data, int len)
{
	int i = 0;
	int j = 0;
	for (i=0; i < len;  i++) {
		if ( ( i != 0 ) && (i %16 == 0))
		printf ("\n");
	    printf("%02x ", data[i]);
	}
	printf("\n");
}

//
//
//
//

int transmit (int fd, unsigned char *data, int len, int tailNb)   // query modem with an AT command
{
	int  n;
	struct timeval timeout;
	

	// Add two byte to the end of the command
	if (tailNb) {
		dbgprintf("\n adding  extra  %d bytes.",tailNb);
		fflush(stdout);
		do {
			data[len++] = 0xFF;
			tailNb--;
		} while (tailNb > 0 );
	}

	printf("\n Transmitting %d bytes:\n", len );
	printData(data, len);

	n = write(fd, data, len);  //Send data
	if (n != len )
	{
		printf("\n Sending bytes failed: sent=%d, expected = %d /n", n, len);
	}

	return n;
}

int test_nano_sleep ( unsigned int ns )
{

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = ns;
	nanosleep (&ts, NULL);
}

int receive(int fd, int max_rx_wait )
{
	int n = 0 , rxTotal = 0;
    unsigned char c;
    int rx_wait = max_rx_wait;

	test_nano_sleep (1000*1000*100);

	memset (rxBuffer,0,MAX_BUFFER );
	
	dbgprintf("\n Waiting for receiver:\n");
    fflush(stdout);
	rxTotal = 0;
	do {
		n = read( fd, &c, 1 );
		if (n > 0 )
		{
		   
			rxBuffer[rxTotal]= c;
			dbgprintf("%02x ", c );
			rxTotal += n;
			rx_wait++;
			if (rxTotal >= MAX_BUFFER)
			 break;
			fflush(stdout);
		}
		else
		{
			usleep (1000);
			rx_wait--;
			dbgprintf(".");
			fflush(stdout);
		}
	} while (rx_wait);

	if (dumpRxFlag)
     {
	    if (rxTotal > 0 )
	    {
	        printf("\nReceived Message %d bytes: \n", rxTotal);
	        printData(rxBuffer, rxTotal);
	    }
	else
	    printf("Nothing is Received");

        printf(" \n\n ");
     }

	return rxTotal ;
	
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

	 dbgprintf("Checksum = 0x%x \n", checksum);

	return checksum;
}


main (int argc, char **argv) 
{ 
	int iteration = 1;
	int retries   = 10;
	fd_set fds;
	int n; 
	int configure = 0;
	const char *device = "/dev/ttymxc4";
	char	*addr_tail;
	unsigned int  command = 0x0, addr = 0x9;
	bool commandFlag = false;
	bool addrFlag = false;
	bool echocmd = false;
	bool dataFlag = false;
	bool compareFlag= false;
	char *pdata;
	int fd;
	int len = 0;
	int	 c;
	int count = 0;
	unsigned int  tailNb = 0;
	int nbError = 0;
	int nbTx, nbRx;


	if (argc < 3 )
    {
	    goto usage;
    }

	while ((c = getopt(argc, argv, "ECesi:c:a:d:u:N:T:" )) != EOF) 
	{
		switch (c)
		{
		case 'u':
			device = optarg;
			continue;
		case 'e':
			 echocmd = true;
			 continue;
		case 'E':
		    dumpRxFlag = false;
		     continue;
		case 'c':
			commandFlag = true;
			command = strtol (optarg,&addr_tail,0);
			continue;
		case 'i':
			iteration = strtol (optarg,&addr_tail,0);
			continue;
		case 'a':
			addrFlag = 1;
			addr = strtol (optarg,&addr_tail,0);
			continue;
		case 'd':
			dataFlag = true;
			pdata = optarg;
			continue;
		case 'N':
			count = strtol (optarg,&addr_tail,0);
			continue;
		case 'T':
			tailNb = strtol (optarg,&addr_tail,0);
			continue;
		case 'C':
		    compareFlag = true;
            continue;
		case '?':
		default:
			goto usage;
		}
	}

	fd = open_port(device);



	if (addrFlag &&  commandFlag) {
		txBuffer[0] = 0;
		txBuffer[1] = addr;  // address
		if (echocmd)
			txBuffer[1] |= 0x40;
		txBuffer[2] = 3;     // len = 3
		txBuffer[3] = 0;     // status
		txBuffer[4] = command;

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
					  txBuffer[5+len] =  dataVal2 ;
					  txBuffer[5+len] |= (dataVal1 << 4 );
					  	 dbgprintf(" len = %d  (0x%02x ) ==> dataVal1 = 0x%02x , dataVa2 = 0x%02x \n",
					  	  len, txBuffer[4+len],  dataVal1, dataVal2);
					  pair = 0;
					  pdata++;
					  len++;
					}

				} // while 

			}
		txBuffer[2] += len;
	   }
      }

	  // Compute Checksum
	  txBuffer[5+len] = getchecksum(txBuffer, len + 5);

	  if (count)
	  {
		  for (n=0; n < count; n++)
		  {
			  txBuffer[5+len + n] = n;
		  }

		  txBuffer[2] += count;

		  txBuffer[5+ len + count] = getchecksum(txBuffer, len + 5 + count);
	  }


#ifdef CONTINUOUS_TRANSMIT
	  do {
		  transmit(fd, txBuffer, len+ 6 + count);
		  test_nano_sleep(1000*1000*100);
		  receive(fd, retries );
	  } while (1);
#else
	  // transmit
	  nbTx = len + 6 + count;
	  for (n= 0; n < iteration; n++ )
      {

	      transmit(fd, txBuffer, nbTx, tailNb);
	      fflush(stdout);
          sleep(1);
          nbRx = receive(fd, retries);
          fflush(stdout);
          test_nano_sleep(1000*1000*100);
          if (compareFlag & echocmd ) {
		  if ( memcmp(txBuffer, rxBuffer, nbTx ) != 0 )
		  {
			  nbError++;
		  }
              printf("\r Iteration %d--------------->Nb. Errors %d", n, nbError);
              fflush(stdout);
          }
	  }

	  if (compareFlag & echocmd )
	  {
	     printf("\n %d errors out of %d  ( %d %%  errors) \n",nbError,iteration, nbError*100/iteration  );
	   }
#endif


	close (fd);
	return(0);

usage:
		fprintf(stderr,
			"Usage: %s  -u device  -a addr -c command -d data -T n\n"
			"Options include:\n"
			"  -u /dev/ttyS1    specify USART to use \n"
			"                   (default is  /dev/ttyS2\n"
			"  -c command       one of the commands \n"
			"  -a address       slave device address \n"
			"  -d data          data in Hex ( 0x1133ffac...\n"
		    "  -e               send an echo message .\n"
		    "  -T  number       add a number of 0xFF to the message \n"
		    "                        (example -N 20 will add 20 byte ) \n"
		    "  -i  n            run the test for n iterations \n"
		    "  -E               disable printing received messages \n"
			, argv[0]);

	
} //main

