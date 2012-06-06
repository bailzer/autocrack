/*
 *	Autocrack - automatically crack everything throught CPU and GPU
 *	Copyright (C) 2012  Massimo Dragano <massimo.dragano@gmail.com>
 *
 *	Autocrack is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	Autocrack is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with Autocrack.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "autocrack.h"

struct _globals globals;

int main(int argc, char *argv[])
{
	// init memory
	memset(&globals,0,sizeof(struct _globals));
	globals.online = globals.rain = globals.dict = globals.gpu = true;
	globals.log_level = info;
	globals.err_buff = malloc((MAX_BUFF+1)*sizeof(char));
	report_error(NULL,0,0,debug);
	P_path(argv[0]);
	signal(SIGINT, signal_handler);
	// handle args
	option_handler(argc,argv);
	// start engine
	engine();
	if(globals.log_level >= info)
		print_hash_list();
	destroy_all();
	exit(EXIT_SUCCESS);
}

void option_handler(int argc, char *argv[])
{
	static struct option long_options[] =
	{
		/* These options set a flag. */
		{"local", no_argument,	NULL, 'l'},
		{"no-rain", no_argument,	NULL, 'r'},
		{"no-dict", no_argument,	NULL, 'd'},
		{"no-gpu", no_argument,	NULL, 'g'},
		{"no-cow", no_argument, NULL, 'C'},
		{"no-john",no_argument, NULL, 'J'},
		{"verbose", no_argument,	NULL, 'v'},
		{"quiet", no_argument,	NULL, 'q'},
		{"debug", no_argument,	NULL, 'D'},
		/* These options don't set a flag. */
		{"hash", required_argument, NULL, 'H'},
		{"type", required_argument, NULL, 't'},
		{"infile", required_argument, NULL, 'i'},
		{"outfile", required_argument, NULL, 'o'},
		{"capture", required_argument, NULL, 'c'},
		{"wordlist", required_argument, NULL, 'w'},
		{"essid", required_argument, NULL, 'e'},
		{"rt-root", required_argument, NULL, 'R'},
		{"onlinedb", required_argument, NULL, 'O'},

		{"help", no_argument, NULL, 'h'},
		{NULL, no_argument, NULL, 0}
	};

	int option_index, c;
	bool bad_option;

	globals.tpool = malloc(sizeof(struct t_info));
	pthread_create(&(globals.tpool->thread), NULL, P_online, NULL);

	option_index = 0;
	bad_option=false;
	c = getopt_long(argc, argv, "vqDH:t:i:o:c:w:e:R:O:hlrdgCJ", long_options, &option_index);

	while(c!=-1 && bad_option==false)
	{
		switch(c)
		{
			case 0:
				break;

			case 'v':
				if( globals.log_level < verbose3 )
					globals.log_level++;
				else if( globals.log_level == debug )
				{
					report_error("already in debug mode.",0,0,warning);
				}
				else if( globals.log_level == verbose3 )
				{
					report_error("maximum verbose level reached.",0,0,info);
					report_error("use --debug or -D if you want more output",0,0,info);
				}

				break;

			case 'q':
				if(globals.log_level > info)
					globals.log_level -= info; // keep the previous verbosity offset
				else
					globals.log_level = quiet;
				break;

			case 'D':
				globals.log_level = debug;
				break;

			case 'H':
				add_hash(NONE,optarg);
				break;

			case 't':
				if( strlen(optarg) == 4 && (!strncmp(optarg,"list",4) || !strncmp(optarg,"LIST",4)))
					print_type_list();
				else
					add_hash(P_type(optarg),NULL);
				break;

			case 'i':
				P_infile(optarg);
				break;

			case 'o':
				P_outfile(optarg);
				break;

			case 'c':
				P_capture(optarg);
				break;

			case 'w':
				P_wordlist(optarg);
				break;

			case 'e':
				P_essid(optarg);
				break;

			case 'R':
				if(globals.rain==true)
					P_rt_root(optarg);
				break;
			case 'O':
				if(globals.online == true)
					P_odb(optarg);
				break;

			case 'h':
				usage(EXIT_SUCCESS,argv[0]);
				break;

			case 'l':
				globals.online = false;
				report_error("switching OFF all online features.",0,0,verbose);
				break;

			case 'r':
				globals.rain = false;
				report_error("switching OFF rainbowtable features.",0,0,verbose);
				break;

			case 'd':
				globals.dict = false;
				report_error("switching OFF dictionary features.",0,0,verbose);
				break;

			case 'g':
				globals.gpu = false;
				report_error("switching OFF GPU features.",0,0,verbose);
				break;

			case 'C':
				if(globals.bins.cow!=NULL)
					free((void *) globals.bins.cow);
				globals.bins.cow = NULL;
				report_error("use this function only for testing.",0,0,warning);
				break;

			case 'J':
				if(globals.bins.jtr!=NULL)
					free((void *) globals.bins.jtr);
				globals.bins.jtr = NULL;
				report_error("use this function only for testing.",0,0,warning);
				break;

			default:
				bad_option=true;
		}
		c = getopt_long(argc, argv, "vqDH:t:i:o:c:w:e:R:O:hlrdgCJ", long_options, &option_index);
	}

	// fix missing option and other stuff
	P_hash_list();
	P_wpa_list();
	P_defaults();

	if(globals.online==false || bad_option == true) // if user disable network features or a problem shut it off.
	{
		if(pthread_kill(globals.tpool->thread,0) == 0) // thread is running
			pthread_cancel(globals.tpool->thread);
		pthread_join(globals.tpool->thread,NULL);
	}
	else
	{
		for(option_index=0;pthread_kill(globals.tpool->thread,0) == 0 && option_index<NET_CHK_TIMEOUT;option_index++)
			usleep(1000);
		if(option_index < NET_CHK_TIMEOUT)
		{
			report(debug,"I've wait internet check for %d ms.",option_index);
			pthread_join(globals.tpool->thread, (void **) &option_index);
		}
		else
			option_index = ETIMEDOUT;
		if(option_index!=0)
		{
			report_error("network check fails.",0,0,error);
			if(option_index == -1)
				report_error("failed to resolve IP for mom Google.",0,0,error);
			else
			{
				errno = option_index;
				report_error("parser_online()",1,0,warning);
			}
			report_error("switching OFF online features.",0,0,info);
			globals.online = false;
		}
	}
	free(globals.tpool);
	globals.tpool=NULL;

	if(bad_option==true)
		usage(EXIT_FAILURE,argv[0]);
	return;
}

void usage(int ret, char *bin_name)
{
	int i;
	char  *developers[] = DEVELOPERS ,
				*msg =
"\nUsage:\t%s [-hHioOtcwReldDvq] options"/* executable name */
"\n      "
"\n      option         argument        description"
"\n      ---------------+---------------+----------------------------------"
"\n      -h, --help     none            display this help"
"\n      -H, --hash     HASH            perform a single hash crack."
"\n      -t, --type     TYPE            specify the single hash type. give \"list\" for a list of supported hash."
"\n      -i, --infile   FILE            pass a hashlist file in format $TYPE$HASH."
"\n      -o, --oufile   FILE            write all founded passwrods and passpharses to FILE."
"\n      -c, --capture  FILE            tcpdump file which contains the wpa handshake."
"\n      -w, --wordlist FILE            wordlist to use in dictionary attacks."
"\n      -R, --rt-root  DIR             rainbow table root directory."
"\n      -O, --onlinedb FILE            database file for online attacks."
"\n      -e, --essid    ESSID           the essid of the AP."
"\n      -l, --local    none            disable online functions."
"\n      -r, --no-rain  none            disable rainbowtable attacks."
"\n      -d, --no-dict  none            disable dictionary attacks."
"\n      -g, --no-gpu   none            disable GPU features."
"\n      -q, --quiet    none            suppress all the output"
"\n      -D, --debug    none            enable debug output."
"\n      -v, --verbose  none            produce more output."
"\n      ---------------+---------------+----------------------------------"
"\n      %s - v%s - %s" /*PROGRAM_NAME - vVERSION - RELEASE_DATE*/
"\n      "
"\n      NOTES:"
"\n      the option \"--essid\" is only required if you want to manually"
"\n      specify the access points to test."
"\n      if this option is not used we will parse the pcap files,"
"\n      obtaining the valid ESSIDs."
"\n      "
"\n      Main developers:";
	/*print help*/
	printf(msg,basename(bin_name),PACKAGE,VERSION,RELEASE_DATE);
	for(i=0;developers[i] != NULL;i++)
		printf("\n\t\t%s",developers[i]);
	printf("\n");
	destroy_all();
	exit(ret);
}
