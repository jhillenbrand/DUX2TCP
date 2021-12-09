#include <stdio.h>
#include <comedilib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netdb.h> 
#include <netinet/in.h>  
//#include <stdint.h>

extern comedi_t *device;

struct server_options {
	char *filename;
	double value;
	int subdevice;
	int channel;
	int aref;
	int range;
	int physical;
	int verbose;
	int n_chan;
	int maxChannels;
	int n_samples;
	double freq;
    int serverPort;
    int tcpBufferSize;
	int duxBufferSize;
};

// STATE ENUMS
enum {	
	START_SERVER=0,
	ACCEPT_CLIENT=1,
	WAIT=10,
	SEND_DATA=100,
	SET_PARAM=101,
	LOOP_DATA=200,
	WRONG_HANDSHAKE=400,
	SHUTDOWN=999
};

#define SA struct sockaddr 

struct server_options initServerOptions(int argc, char **argv);
int openTcpServerSocket(int port);
int acceptTcpClient(int sockfd);
char * waitForHandshake(char *recBuffer, int sockfd, struct server_options options);
int processHandshake(char *handshake);
struct server_options changeServerOptions(char *handshake,struct server_options options);

comedi_t * openDuxDevice(struct server_options options);
int prepareDuxCmd(comedi_t *device, struct server_options options, comedi_cmd *cmd);
int startRecording(comedi_t *device, comedi_cmd *cmd);
int startTcpDataStreamLoop(int sockfd, comedi_t *device, struct server_options options);
int startTcpDataStreamLoop2(int sockfd, comedi_t *device, struct server_options options);

int main(int argc, char **argv){
	// init variables
	struct server_options options;
    int sockfd, connfd;	
	comedi_t *dev;
	comedi_cmd c,*cmd=&c;	
	int state = 0;
	int ret = 0;
	int isRunning = 1;
	int w = 0;

	// STATE MACHINE FOR HANDLING CLIENT HANDSHAKES
	while(isRunning){
		fprintf(stderr, "----------------\n");
		w = w + 1;
		fprintf(stderr, "STATE Loop: %d\n", w);
		// recBuffer and hs must be reset each loop, otherwise stack smashing is detected ????!!!
		char recBuffer[options.tcpBufferSize];
		char *hs;	
		switch (state){
			case START_SERVER:
				fprintf(stderr, "STATE %d [%s]: \n\tstarting server and open USB DUX\n", state, "START_SERVER");
				// init server config options
				options = initServerOptions(argc, argv);   
				// open tcp server socket	
				sockfd  = openTcpServerSocket(options.serverPort);				
				// open and prepare dux device
				dev = openDuxDevice(options);
				// prepare next record command
				prepareDuxCmd(dev, options, cmd);
				state = ACCEPT_CLIENT;
				break;

			case ACCEPT_CLIENT:
				fprintf(stderr,"STATE %d [%s]: \n\twaiting for a client\n", state, "ACCEPT_CLIENT");
				// accept client connection
				connfd = acceptTcpClient(sockfd);
				state = WAIT;
				break;	

			case WAIT:
				fprintf(stderr,"STATE %d [%s]: \n\twaiting for new Handshake\n", state, "WAIT");
				// waiting for next handshake
				hs = waitForHandshake(recBuffer, connfd, options);	
				if (hs == NULL){
					// trigger ACCEPT_CLIENT if waiting for HANDSHAKE was interrupted
					state = ACCEPT_CLIENT;
				} else {
					state = processHandshake(hs);
				}
				//state = SEND_DATA;		 
				break;			

			case SEND_DATA:		
				fprintf(stderr,"STATE %d [%s]:\n", state, "SEND_DATA");	
				startRecording(dev, cmd);
				startTcpDataStreamLoop(connfd, dev, options);
				state = WAIT;	
				break;
			
			case SET_PARAM:
				fprintf(stderr,"STATE %d [%s]: \n\tedit server options\n", state, "SET_PARAM");
				options = changeServerOptions(hs, options);
				prepareDuxCmd(dev, options, cmd);
				state = WAIT;
				break;			

			case LOOP_DATA:
				fprintf(stderr,"STATE %d [%s]: \n\tloop for sending data\n", state, "LOOP_DATA");
				startRecording(dev, cmd);
				startTcpDataStreamLoop2(connfd, dev, options);
				state = LOOP_DATA;
				break;

			case WRONG_HANDSHAKE:
				fprintf(stderr,"STATE %d [%s]: \n\tunknown handshake was sent\n", state, "WRONG_HANDSHAKE");
				state = WAIT;
				break;

			case SHUTDOWN:
				fprintf(stderr,"STATE %d [%s]: \n\tclosing server socket and terminating application\n\n", state, "SHUTDOWN");
				close(connfd);
				close(sockfd);
				//close(dev);
				isRunning = 0;
				break;
		}
		fprintf(stderr, "----------------\n");
	}
	return 0;
}

/*
 * initializes the default config options for server
 */
struct server_options initServerOptions(int argc, char **argv){
	struct server_options options;
	options.filename = "/dev/comedi0";
	options.subdevice = 0;
	options.range = 0;	// 0 = -0.75..0.75V, 1 = -0.5..0.5V
	options.aref = AREF_GROUND;
	options.physical = 0;	//default the raw integer values and not the voltage 
	options.n_chan = 1;	//default value, used if no argument is passed for: -c
	options.maxChannels = 256;	// maximum numbver of Channels [UNKNOWN USAGE]
	options.n_samples = 1000000;	//default value, used if no argument is passed for: -n
	options.freq = 2000000.0;	//default value, used if no argument is passed for: -f [freq / n_samples = seconds (of measurement)]
    options.channel = 0; // default value for starting channel after: -k
	options.serverPort = 2020; // default port for tcp traffic: -p
    options.tcpBufferSize = 512; // default buffer size of tcp message: -t [shall equal the BUFSZ of the pipe read from USBDUX [max. 512]]
	options.duxBufferSize = 512; // default buffer size of dux data: -d [max. 512 bytes]
	
	// parse commandline arguments
	int arguments;
    while ((arguments = getopt (argc, argv, "f:n:c:k:p:t:d")) != -1){
        switch (arguments){
            case 'f':
                options.freq = atof(optarg);
                break;
            case 'n':
                options.n_samples = atoi(optarg);
                break;
            case 'c':
                options.n_chan = atoi(optarg);
                break;
            case 'k':
                options.channel = atoi(optarg);
                break;
            case 'p':
                options.serverPort = atoi(optarg);
                break;
            case 't':
                options.tcpBufferSize = atoi(optarg);
                break;
			case 'd':
                options.duxBufferSize = atoi(optarg);
                break;
            default:
                abort();
        }
    }
	return options;
}

int openTcpServerSocket(int port){
	//  init tcp server
    fprintf(stderr,"\tinitializing tcp server socket\n");
	// socket create and verification 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        fprintf(stderr, "\tsocket creation failed\n"); 
		close(sockfd);
        exit(0); 
    } else {
        fprintf(stderr, "\tSocket successfully created\n");
	}

    struct sockaddr_in servaddr; 
    bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(port);
	// Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        fprintf(stderr, "\tsocket bind failed...\n");
		close(sockfd); 
        exit(0); 
    } else {
        fprintf(stderr, "\tsocket successfully bound\n"); 
	}
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        fprintf(stderr, "\tlistening failed\n"); 
		close(sockfd);
        exit(0); 
    } else {
        fprintf(stderr, "\tserver is listening\n");  
	}
	return sockfd;
}

int acceptTcpClient(int sockfd){	
	struct sockaddr_in cliaddr;
	int len;   
	len = sizeof(cliaddr);  //len is value/result
	// Accept the data packet from client and verification 
    int connfd;
	connfd = accept(sockfd, (SA*)&cliaddr, &len); 
    if (connfd < 0) { 
        fprintf(stderr, "\tserver did not acccept client\n");
		close(sockfd); 
        exit(0); 
    } else {
        fprintf(stderr, "\tserver acccepted client\n"); 
	}
	return connfd;
}

/*
 * waits for next Handshake and returns send string  
 */
char* waitForHandshake(char *recBuffer, int sockfd, struct server_options options){
	// wait for client to send message to start data streaming, recvfrom blocks until first message from client is received
    // IMPORTANT: server must be started and running before client sends his handshake (message)    
	//char recBuffer[options.tcpBufferSize];
	for (;;) {         
		bzero(recBuffer, options.tcpBufferSize);
  
        // read the message from client and copy it in buffer 
        //read(sockfd, recBuffer, sizeof(*recBuffer));
		int readStatus = read(sockfd, recBuffer, options.tcpBufferSize);
		if (readStatus == 0){
			fprintf(stderr, "\tdid not receive handshake, client connection was probably interrupted.\n\tTCP server requires new CLIENT_ACCEPT.\n");
			return NULL;
		} 
        // print buffer which contains the client contents 
        fprintf(stderr, "\treceived client handshake: %s\n", recBuffer);
		if (strstr(recBuffer, "#HANDSHAKE#") != NULL){
			fprintf(stderr, "\thandshake seems valid and will be further processed\n");
			return recBuffer;
		} else {
			return NULL;			
		}
    }
}

int processHandshake(char *handshake){
	// depending on content of recBuffer the correct state is returned for further processing in STATE MACHINE
	int state = WAIT;
	char *str;	
	str = strstr(handshake, "#HANDSHAKE#SEND_DATA");
	if (str != NULL){ 	
		return SEND_DATA;
	}
	str = strstr(handshake, "#HANDSHAKE#SET_PARAM");
	if(str != NULL){
		return SET_PARAM;
	}
	str = strstr(handshake, "#HANDSHAKE#LOOP_DATA");
	if (str != NULL){
		return LOOP_DATA;
	}
	str = strstr(handshake, "#HANDSHAKE#SHUTDOWN");
	if(str != NULL){
		return SHUTDOWN;
	} else {
		fprintf(stderr, "\thandshake syntax is unknown, wait for next handshake\n\n");
		return WRONG_HANDSHAKE;
	}
}

struct server_options changeServerOptions(char *handshake, struct server_options options){	
	if (strstr(handshake, "#HANDSHAKE#SET_PARAM:CH=") != NULL){
		char *s = strchr(handshake, '=');
		memmove(s, s + 1, strlen(s));
		if(s != NULL){
			int numOfChan = atoi(s);
			options.n_chan = numOfChan;
			fprintf(stderr, "\tchanged numberOfCHannels to %d\n\n", numOfChan);
		} else {
			fprintf(stderr, "\terror during SET_PARAM for numberOfCHannels, nothing is changed\n\n");
		}
	} else if (strstr(handshake, "#HANDSHAKE#SET_PARAM:SR=") != NULL){
		char *s = strchr(handshake, '=');
		memmove(s, s + 1, strlen(s));
		if(s != NULL){
			int sampleRate = atoi(s);
			options.freq = sampleRate;
			fprintf(stderr, "\tchanged sampleRate to %d\n\n", sampleRate);
		} else {
			fprintf(stderr, "\terror during SET_PARAM Handshake for SampleRate, nothing is changed\n\n");
		}
	} else if (strstr(handshake, "#HANDSHAKE#SET_PARAM:N=") != NULL){		
		char *s = strchr(handshake, '=');
		memmove(s, s + 1, strlen(s));
		//fprintf(stderr, "extracted: %s\n", s);
		if(s != NULL){
			int numberOfSamples = atoi(s);
			options.n_samples = numberOfSamples;
			fprintf(stderr, "\tchanged numberOfSamples to %d\n\n", numberOfSamples);
		} else {
			fprintf(stderr, "\terror during SET_PARAM Handshake for Number of Samples, nothing is changed\n\n");
		}
		
	} else {
		fprintf(stderr, "\tunkown SET_PARAM Handshake, nothing is changed\n\n");
	}
	return options;
}

comedi_t * openDuxDevice(struct server_options options){
	comedi_t *dev;
	dev = comedi_open(options.filename);
	if(!dev){
		comedi_perror(options.filename);
		exit(1);
	}

	// Print numbers for clipped inputs
	comedi_set_global_oor_behavior(COMEDI_OOR_NUMBER);
	fprintf(stderr, "\topened dux device\n");
	return dev;
}

/*
 * This prepares a command in a pretty generic way.  We ask the
 * library to create a stock command that supports periodic
 * sampling of data, then modify the parts we want. 
 */
int prepareDuxCmd(comedi_t *dev, struct server_options options, comedi_cmd *cmd){
	/* prepare_cmd_lib() uses a Comedilib routine to find a good command for the device.  prepare_cmd() explicitly creates a command, which may not work for your device. */
	unsigned scan_period_nanosec = 1e9 / options.freq;
    fprintf(stderr, "\tsample period in [ns]: %u\n", scan_period_nanosec);
	unsigned int chanlist[options.maxChannels];
	comedi_range * range_info[options.maxChannels];
	lsampl_t maxdata[options.maxChannels];
	/* Set up channel list */
	for(int i = 0; i < options.n_chan; i++){
		chanlist[i] = CR_PACK(options.channel + i, options.range, options.aref);
		range_info[i] = comedi_get_range(dev, options.subdevice, options.channel, options.range);
		maxdata[i] = comedi_get_maxdata(dev, options.subdevice, options.channel);
	}

	memset(cmd, 0, sizeof(*cmd));

	/* This comedilib function will get us a generic timed
	 * command for a particular board.  If it returns -1,
	 * that's bad. */
	
	int ret = comedi_get_cmd_generic_timed(dev, options.subdevice, cmd, options.n_chan, scan_period_nanosec);
	if(ret<0){
		fprintf(stderr, "\tcomedi_get_cmd_generic_timed failed\n");
		return 1;
	}

	/* Modify parts of the command */
	cmd->chanlist = chanlist;
	cmd->chanlist_len = options.n_chan;
	if(cmd->stop_src == TRIG_COUNT) {
		cmd->stop_arg = options.n_samples;
	}

	/* comedi_command_test() tests a command to see if the
	 * trigger sources and arguments are valid for the subdevice.
	 * If a trigger source is invalid, it will be logically ANDed
	 * with valid values (trigger sources are actually bitmasks),
	 * which may or may not result in a valid trigger source.
	 * If an argument is invalid, it will be adjusted to the
	 * nearest valid value.  In this way, for many commands, you
	 * can test it multiple times until it passes.  Typically,
	 * if you can't get a valid command in two tests, the original
	 * command wasn't specified very well. */
	ret = comedi_command_test(dev, cmd);
	if(ret < 0){
		comedi_perror("comedi_command_test");
		if(errno == EIO){
			fprintf(stderr,"\nUmmm... this subdevice doesn't support commands\n");
		}
		exit(1);
	}
	ret = comedi_command_test(dev, cmd);
	if(ret < 0){
		comedi_perror("comedi_command_test");
		exit(1);
	}
	//fprintf(stderr,"second test returned %d (%s)\n", ret, cmdtest_messages[ret]);
	if(ret!=0){
		fprintf(stderr, "\tError preparing command\n");
		exit(1);
	}
	fprintf(stderr, "\tprepared dux command\n");
	fprintf(stderr,"\tsampling frequency rounded to: ");
	double sampling_frequency = 1e9 / (cmd->convert_arg);	
	fprintf(stderr,"%f",sampling_frequency);
	fprintf(stderr,"Hz\n\n");
	return 0;
}

int startRecording(comedi_t *dev, comedi_cmd *cmd){
	fprintf(stderr,"\tstart recording\n");
	/* start the command */
	int ret = comedi_command(dev, cmd);
	if(ret < 0){
		fprintf(stderr, "FAIL\n");
		comedi_perror("comedi_command");
		exit(1);
	}	
	// Print numbers for clipped inputs
	comedi_set_global_oor_behavior(COMEDI_OOR_NUMBER);
	return 0;
}

int startTcpDataStreamLoop(int sockfd, comedi_t *device, struct server_options options){	
	struct timeval start, end;
	int sp = 0; // sent packets
	int firstRead = 0;
	int firstSend = 0;
	int ret;
	int tcpSendOk = 1;
	// init buffer	
	char duxBuf[options.duxBufferSize];
	bzero(duxBuf, options.duxBufferSize);

	// start time
	gettimeofday(&start, NULL);

    fprintf(stderr,"\tstart tcp data streaming:\n");
    while(1){
		// read raw data from pipe into buffer duxBuf and process it
        ret = read(comedi_fileno(device), duxBuf, options.duxBufferSize);
		if (!firstRead){
			firstRead = 1;
			fprintf(stderr,"\tfirst data read\n");
		}
        //printf("read from pipe\n");
        //fprintf(stderr, "bytesRead=%d\n", ret);
		if(ret < 0){
			/* some error occurred */
			fprintf(stderr,"\terror during reading of device buffer\n");
			// exit method with error code 1
			return 1;
		} else if(ret == 0){
			/* reached stop condition */
			fprintf(stderr,"\tno more data in device buffer, stopped reading\n");
			break;
		} else {
			sp = sp + 1;
			tcpSendOk = write(sockfd, duxBuf, sizeof(duxBuf)); 
			if (tcpSendOk < 0){
				fprintf(stderr,"\terror writing to socket\n");
				// exit method with error code 1
				return 1;
			}
			if (!firstSend){
				firstSend = 1;
				fprintf(stderr,"\tfirst packet send\n");
			}
		}
	}
	fprintf(stderr,"\tstopped tcp data streaming\n");

	//end time
	gettimeofday(&end,NULL);

	end.tv_sec -= start.tv_sec;
	if(end.tv_usec < start.tv_usec){
		end.tv_sec--;
		end.tv_usec += 1000000;
	}
	end.tv_usec -= start.tv_usec;
	fprintf(stderr,"\telapsed time: %ld.%06ld\n", end.tv_sec, end.tv_usec);
	fprintf(stderr, "\tsent packets: %d\n", sp);
	// exit method with success code 0
	return 0;
}

/*****************************************************************************
 * startTcpDataStreamLoop2(int sockfd, comedi_t *device, struct server_options options) 
 * @sockfd 
 * @*device  
 * @options    
 * @return int return code, succes = 0, error = 0  
 *****************************************************************************/
int startTcpDataStreamLoop2(int sockfd, comedi_t *device, struct server_options options){	
	int firstRead = 0;
	int firstSend = 0;
	int ret;
	int tcpSendOk = 1;
	// init buffer	
	char duxBuf[options.duxBufferSize];
	bzero(duxBuf, options.duxBufferSize);

    //fprintf(stderr,"\tstart tcp data streaming:\n");
    
	while(1){
		// read raw data from pipe into buffer duxBuf and process it
        ret = read(comedi_fileno(device), duxBuf, options.duxBufferSize);
		if (!firstRead){
			firstRead = 1;
			//fprintf(stderr,"\tfirst data read\n");
		}
        //printf("read from pipe\n");
        //fprintf(stderr, "bytesRead=%d\n", ret);
		if(ret < 0){
			/* some error occurred */
			fprintf(stderr,"\terror during reading of device buffer\n");
			// exit method with error code 1
			return 1;
		} else if(ret == 0){
			/* reached stop condition */
			fprintf(stderr,"\tno more data in device buffer, stopped reading\n");
			break;
		} else {
			tcpSendOk = write(sockfd, duxBuf, sizeof(duxBuf)); 
			if (tcpSendOk < 0){
				fprintf(stderr,"\terror writing to socket\n");
				// exit method with error code 1
				return 1;
			}
			/*
			if (!firstSend){
				firstSend = 1;
				fprintf(stderr,"\tfirst packet send\n");
			}
			*/
		}
	}
	//fprintf(stderr,"\tstopped tcp data streaming\n");

	// exit method with success code 0
	return 0;
}