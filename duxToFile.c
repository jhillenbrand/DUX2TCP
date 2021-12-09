#include <stdio.h>
#include <comedilib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

extern comedi_t *device;

struct parsed_options
{
	char *filename;
	double value;
	int subdevice;
	int channel;
	int aref;
	int range;
	int physical;
	int verbose;
	int n_chan;
	int n_scan;
	double freq;

	int saveInfo;
};



#define BUFSZ 10000
char buf[BUFSZ];

#define N_CHANS 256
static unsigned int chanlist[N_CHANS];
static comedi_range * range_info[N_CHANS];
static lsampl_t maxdata[N_CHANS];


int prepare_cmd_lib(comedi_t *dev, int subdevice, int n_scan, int n_chan, unsigned period_nanosec, comedi_cmd *cmd);

void do_cmd(comedi_t *dev,comedi_cmd *cmd);

void print_datum(lsampl_t raw, int channel_index, short physical);

char *cmdtest_messages[]={
	"success",
	"invalid source",
	"source conflict",
	"invalid argument",
	"argument conflict",
	"invalid chanlist",
};

int main(int argc, char **argv){

	comedi_t *dev;
	comedi_cmd c,*cmd=&c;
	int ret;
	int total=0;
	int i;
	struct timeval start,end,ende;
	int subdev_flags;
	lsampl_t raw;

	struct parsed_options options;

	/* The following variables used in this demo
	 * can be modified by command line
	 * options.  When modifying this demo, you may want to
	 * change them here. */
	options.filename = "/dev/comedi0";
	options.subdevice = 0;
	options.channel = 0;
	options.range = 0;
	options.aref = AREF_GROUND;
	options.physical = 0;	//output the raw integer values and not the voltage 
	options.n_chan = 1;	//standard value, used if no argument is passed for -c
	options.n_scan = 2000000;	//standard value, used if no argument is passed for -n
	options.freq = 2000000.0;	//standard value, used if no argument is passed for -f
	options.saveInfo = 0;

	char filename_info[100];

    int arguments;
    while ((arguments = getopt (argc, argv, "f:n:c:k:h:s:")) != -1){
        switch (arguments){
            case 'f':
                options.freq = atof(optarg);
                break;
            case 'n':
                options.n_scan = atoi(optarg);
                break;
            case 'c':
                options.n_chan = atoi(optarg);
                break;
            case 'k':
                options.channel = atoi(optarg);
                break;
            case 'h':
                fprintf (stderr,"arguments:\n[-f] SAMPLING_FREQUENCY (in Hz with .0 at the end)\n[-n] NUMBER_OF_SAMPLES\n[-c] NUMBER OF CHANNELS \n if no arguments are passed f=2000000.0 , n=2000000 and c=1 are used.\nexample:   ./cmd > output_file -f 60000.0 -n 60000 -c 16\n");
                return 0;
            case 's':
                options.saveInfo = 1;
                printf(filename_info, 100, "%s", optarg);
                break;
            default:
                abort ();
      }
    }
	// printf('\n\t%s\n', argv[0]);
	// printf('\n\t%d\n', argc);
	
	/* open the device */
	dev = comedi_open(options.filename);
	if(!dev){
		comedi_perror(options.filename);
		exit(1);
	}

	// Print numbers for clipped inputs
	comedi_set_global_oor_behavior(COMEDI_OOR_NUMBER);

	/* Set up channel list */
	for(i = 0; i < options.n_chan; i++){
		chanlist[i] = CR_PACK(options.channel + i, options.range, options.aref);
		range_info[i] = comedi_get_range(dev, options.subdevice, options.channel, options.range);
		maxdata[i] = comedi_get_maxdata(dev, options.subdevice, options.channel);
	}

	/* prepare_cmd_lib() uses a Comedilib routine to find a
	 * good command for the device.  prepare_cmd() explicitly
	 * creates a command, which may not work for your device. */
	prepare_cmd_lib(dev, options.subdevice, options.n_scan, options.n_chan, 1e9 / options.freq, cmd);


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
			fprintf(stderr,"Ummm... this subdevice doesn't support commands\n");
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
		fprintf(stderr, "Error preparing command\n");
		exit(1);
	}

	fprintf(stderr,"sampling frequency rounded to: ");
	double sampling_frequency = 1e9 / (cmd->convert_arg);	
	fprintf(stderr,"%f",sampling_frequency);
	fprintf(stderr,"Hz\n");
	
	/* this is only for informational purposes */
	gettimeofday(&start, NULL);
	//fprintf(stderr,"start time: %ld.%06ld\n", start.tv_sec, start.tv_usec);
	fprintf(stderr,"start recording!\n");
	/* start the command */
	ret = comedi_command(dev, cmd);
	if(ret < 0){
		comedi_perror("comedi_command");
		exit(1);
	}
	subdev_flags = comedi_get_subdevice_flags(dev, options.subdevice);
    while(1){
        //fprintf(stderr, "start reading into buffer\n");
		// read raw data from pipe into buffer buf and process it
        ret = read(comedi_fileno(dev), buf, BUFSZ);
		if(ret < 0){
			// some error occurred
			perror("read");
			break;
		}else if(ret == 0){
			//reached stop condition
            //fprintf(stderr, "size of buf = %ld", strlen(buf)); 
			break;
		}else{
			static int col = 0;
			int bytes_per_sample;
			total += ret;
			//if(options.verbose)fprintf(stderr, "read %d %d\n", ret, total);
			if(subdev_flags & SDF_LSAMPL){				
                //fprintf(stderr, "ok1\n");
                bytes_per_sample = sizeof(lsampl_t);
			} else {
                //fprintf(stderr, "ok2\n");
				bytes_per_sample = sizeof(sampl_t);               
            }
			for(i = 0; i < ret / bytes_per_sample; i++){
				if(subdev_flags & SDF_LSAMPL) {
					raw = ((lsampl_t *)buf)[i];
				} else {
					raw = ((sampl_t *)buf)[i];
				}
				// printf("%f ",((raw-2048)*1.5/4096));		// convertBitToVolt (0..2¹²-1 -> +-0.75V) (-> geht net (BROKEN PIPE), PIPE zu langsam)
				// bytes are redirected to output stream / file if the script is called like "./readAtOnce > measure.dat ..."
                printf("%d ",raw);					// no conversion
				col++;
				if(col == options.n_chan){
					printf("\n");
					col=0;
				}                
			}
		}
	}

	/* this is only for informational purposes */
	gettimeofday(&end,NULL);
	gettimeofday(&ende,NULL);
	//fprintf(stderr,"end time: %ld.%06ld\n", end.tv_sec, end.tv_usec);

	end.tv_sec -= start.tv_sec;
	if(end.tv_usec < start.tv_usec){
		end.tv_sec--;
		end.tv_usec += 1000000;
	}
	end.tv_usec -= start.tv_usec;
	fprintf(stderr,"elapsed time: %ld.%06ld", end.tv_sec, end.tv_usec);
	fprintf(stderr,"s\nDone!\n");

	//for Information purposes - EG
	if(options.saveInfo){
		long startPOSIX = start.tv_sec*1000+start.tv_usec/1000;
		long endPOSIX = ende.tv_sec*1000+ende.tv_usec/1000;
		FILE *dateiw;
		fprintf(stderr,"Infos are saved under: %s\n", filename_info);
		dateiw = fopen(filename_info, "w");
		fprintf(dateiw, "startPOSIX_ms:%ld\nendPOSIX_ms:%ld\nsamplingFrequency:%f0\n", startPOSIX, endPOSIX, sampling_frequency);
		fclose(dateiw);
	}

	return 0;
}

/*
 * This prepares a command in a pretty generic way.  We ask the
 * library to create a stock command that supports periodic
 * sampling of data, then modify the parts we want. */
int prepare_cmd_lib(comedi_t *dev, int subdevice, int n_scan, int n_chan, unsigned scan_period_nanosec, comedi_cmd *cmd)
{
	int ret;

	memset(cmd,0,sizeof(*cmd));

	/* This comedilib function will get us a generic timed
	 * command for a particular board.  If it returns -1,
	 * that's bad. */
	ret = comedi_get_cmd_generic_timed(dev, subdevice, cmd, n_chan, scan_period_nanosec);
	if(ret<0){
		printf("comedi_get_cmd_generic_timed failed\n");
		return ret;
	}

	/* Modify parts of the command */
	cmd->chanlist = chanlist;
	cmd->chanlist_len = n_chan;
	if(cmd->stop_src == TRIG_COUNT) cmd->stop_arg = n_scan;

	return 0;
}
