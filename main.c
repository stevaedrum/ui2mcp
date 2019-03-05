/*  ###########################################################  */
/*  Programmer:    Stephan Allene <stephan.allene@gmail.com>                                                    */
/*                                                                                                                                                              */
/*  Project:    Ui MPC interface Midi Controler                                                                                     */
/*                                                                                                                                                              */
/*  ###########################################################   */

/*  standard include  */

#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h> /* gethostbyname */

/*  special include  */

#include <alsa/asoundlib.h>     /* Interface to the ALSA system */
#include <netinet/in.h>
#include <arpa/inet.h>
#include "includes/version.h"
#include "includes/ui.h"
#include "includes/b64.h"
#include "includes/readconfig.h"

/*  define  */

#define PORT 80
#define SOCKET_ERROR -1
#define TRUE = 1
#define FALSE = 0

#define  KORG
#define FILENAME "/home/pi/UI24r-Midi/ui2mcp/config.conf"
//#define MAXBUF 1024
#define DELIM "="

int debug = 0;

unsigned short stop = 0;                                                        // Flag of program stop, default value is 0.
FILE *hfErr;                                                                            // Declaration for log file.
char sa_LogMessage[262145] = "";                                        // Variable for log message.

static char *send_hex;                                                            // Variable for SysEx message in hexa.
static char *send_data;                                                           // Variable for SysEx message.
static int send_data_length;                                                    // Variable for len of SysEx message.
static int sysex_interval;

static snd_rawmidi_t* midiout = NULL;
static snd_rawmidi_t* midiin = NULL;

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
/*  Function  */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */

/*  get signal -- stop =1 when SIGINT Ctl^c.  */
void signalHandler(int signal){
	if(signal==SIGINT || signal==SIGKILL){
		static unsigned short nbSignal = 0;
		nbSignal++;
		printf("Signal Ctrl^c n°%i reçu\n",nbSignal);
		if(nbSignal==1) stop=1;
	}
}

/*  error -- Print an error message.  */
void errormessage(const char *format, ...){
   va_list ap;
   va_start(ap, format);
   vfprintf(stderr, format, ap);
   va_end(ap);
   putc('\n', stderr);
}

/*  Function for export message.  */
void LogTrace(FILE *p_File, int loglevel, int debug, char *p_Trace, ...){
    if( debug == 0 ){
        if( loglevel == 0 ){
            printf("%s",p_Trace);
        }
    }
    else if ( debug > 0 ){
        if( loglevel <= debug && loglevel != 0 ){
            if( loglevel == 0 ){fprintf(p_File,"%s", p_Trace);}
            if( loglevel == 1 ){fprintf(p_File,"%s", p_Trace);}
            if( loglevel == 2 ){fprintf(p_File,"%s", p_Trace);}
            if( loglevel == 3 ){fprintf(p_File,"%s", p_Trace);}
            if( loglevel == 4 ){fprintf(p_File,"%s", p_Trace);}
            if( loglevel == 5 ){fprintf(p_File,"%s", p_Trace);}
        }
        else if ( loglevel == 0 ){
            fprintf(p_File,"%s", p_Trace);
            printf("%s",p_Trace);
        }
    }
    return ;
}

/*  give time in miliseconds.  */
long currentTimeMillis(){
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

/*  split char with demiliter like Perl function.  */
char** split(char* chaine,const char* delim,int vide){

    char** tab=NULL;                    //tableau de chaine, tableau resultat
    char *ptr;                                  //pointeur sur une partie de
    int sizeStr;                                //taille de la chaine à recupérer
    int sizeTab=0;                          //taille du tableau de chaine
    char* largestring;                    //chaine à traiter

    int sizeDelim=strlen(delim);   //taille du delimiteur


    largestring = chaine;          //comme ca on ne modifie pas le pointeur d'origine
                                                //(faut ke je verifie si c bien nécessaire)


    while( (ptr=strstr(largestring, delim))!=NULL ){
           sizeStr=ptr-largestring;

           //si la chaine trouvé n'est pas vide ou si on accepte les chaine vide
           if(vide==1 || sizeStr!=0){
               //on alloue une case en plus au tableau de chaines
               sizeTab++;
               tab= (char**) realloc(tab,sizeof(char*)*sizeTab);

               //on alloue la chaine du tableau
               tab[sizeTab-1]=(char*) malloc( sizeof(char)*(sizeStr+1) );
               strncpy(tab[sizeTab-1],largestring,sizeStr);
               tab[sizeTab-1][sizeStr]='\0';
           }

           //on decale le pointeur largestring  pour continuer la boucle apres le premier elément traiter
           ptr=ptr+sizeDelim;
           largestring=ptr;
    }

    //si la chaine n'est pas vide, on recupere le dernier "morceau"
    if(strlen(largestring)!=0){
           sizeStr=strlen(largestring);
           sizeTab++;
           tab= (char**) realloc(tab,sizeof(char*)*sizeTab);
           tab[sizeTab-1]=(char*) malloc( sizeof(char)*(sizeStr+1) );
           strncpy(tab[sizeTab-1],largestring,sizeStr);
           tab[sizeTab-1][sizeStr]='\0';
    }
    else if(vide==1){ //si on fini sur un delimiteur et si on accepte les mots vides,on ajoute un mot vide
           sizeTab++;
           tab= (char**) realloc(tab,sizeof(char*)*sizeTab);
           tab[sizeTab-1]=(char*) malloc( sizeof(char)*1 );
           tab[sizeTab-1][0]='\0';

    }

    //on ajoute une case à null pour finir le tableau
    sizeTab++;
    tab= (char**) realloc(tab,sizeof(char*)*sizeTab);
    tab[sizeTab-1]=NULL;

    return tab;
}

/*  slice char with begin and len like Perl function.  */
char* substring(const char* str, size_t begin, size_t len)
{
  if (str == 0 || strlen(str) == 0 || strlen(str) < begin || strlen(str) < (begin+len))
    return 0;

  return strndup(str + begin, len);
}

/*  Memory allocation.  */
static void *my_malloc(size_t size){
	void *p = malloc(size);
	if (!p) {
		errormessage("out of memory");
		exit(EXIT_FAILURE);
	}
	return p;
}

/*  Send SysEx function.  */
static int send_midi_interleaved(void){
	int err;
	char *data = send_data;
	size_t buffer_size;
	snd_rawmidi_params_t *param;
	snd_rawmidi_status_t *st;

	snd_rawmidi_status_alloca(&st);

	snd_rawmidi_params_alloca(&param);
	snd_rawmidi_params_current(midiout, param);
	buffer_size = snd_rawmidi_params_get_buffer_size(param);

	while (data < (send_data + send_data_length)) {
		int len = send_data + send_data_length - data;
		char *temp;

		if (data > send_data) {
			snd_rawmidi_status(midiout, st);
			do {
				/* 320 µs per byte as noted in Page 1 of MIDI spec */
				usleep((buffer_size - snd_rawmidi_status_get_avail(st)) * 320);
				snd_rawmidi_status(midiout, st);
			} while(snd_rawmidi_status_get_avail(st) < buffer_size);
			usleep(sysex_interval * 1000);
		}

		/* find end of SysEx */
		if ((temp = memchr(data, 0xf7, len)) != NULL)
			len = temp - data + 1;

		if ((err = snd_rawmidi_write(midiout, data, len)) < 0)
			return err;

		data += len;
	}

	return 0;
}

/*  Convert ui[].MidiMix to db.  */
static float linear_to_db(float c){
	float db0 = .7647058823529421;

	if (c == db0)	                            return 0;
    else if (c > db0)	            		    return (pow(10,c) - pow(10,db0)) / 0.42;
	else if (c > 0.26 && c < db0)		return (65.7*log10(c) + (db0*10))+0.6;
	else if (c < 0.16 && c < db0)		return (65.7*log10(c) + (db0*10))+5.0;
	else if (c < 0.26 && c < db0)		return (65.7*log10(c) + (db0*10))+0.8;

	errormessage("invalid value %f", c);
	return -1;
}

/*  Convert char to hexa value.  */
static int hex_value(char c){
	if ('0' <= c && c <= '9')
		return c - '0';
	if ('A' <= c && c <= 'F')
		return c - 'A' + 10;
	if ('a' <= c && c <= 'f')
		return c - 'a' + 10;
	errormessage("invalid character %c", c);
	return -1;
}

/*  Create value for SysEx message.  */
static void parse_data(void){
	const char *p;
	int i, value;

	send_data = my_malloc(strlen(send_hex)); /* guesstimate */
	i = 0;
	value = -1; /* value is >= 0 when the first hex digit of a byte has been read */
	for (p = send_hex; *p; ++p) {
		int digit;
		if (isspace((unsigned char)*p)) {
			if (value >= 0) {
				send_data[i++] = value;
				value = -1;
			}
			continue;
		}
		digit = hex_value(*p);
		if (digit < 0) {
			send_data = NULL;
			return;
		}
		if (value < 0) {
			value = digit;
		} else {
			send_data[i++] = (value << 4) | digit;
			value = -1;
		}
	}
	if (value >= 0)
		send_data[i++] = value;
	send_data_length = i;
}

/*  Concate data for SysEx massage.  */
static void add_send_hex_data(const char *str){
    sprintf(sa_LogMessage,"UI2MCP --> MIDI : STR Hex : (%s)\n", str);
    LogTrace(hfErr, 5, debug, sa_LogMessage);

	int length;
	char *s;

	length = (send_hex ? strlen(send_hex) + 1 : 0) + strlen(str) + 1;

	//printf("lenght %i %i\n", strlen(str), length);

	s = my_malloc(length);
	if (send_hex) {
		strcpy(s, send_hex);
		strcat(s, " ");
	} else {
		s[0] = '\0';
	}
	strcat(s, str);
	free(send_hex);
	send_hex = s;
}

/*  Send midi out SysEx message.  */
static void SendSysExTextOut(snd_rawmidi_t *hwMidi, char *SysExHdr, char *SysExCmd, char *Text){
	int status;

	char SysExEox[3] = "F7";
	char Buff[1024];
	Buff[0] = '\0';

    unsigned BuffH[1024];
	BuffH[0] = '\0';
    int i,j;

	strcpy(Buff, SysExHdr);
	strcat(Buff, SysExCmd);

	//printf("%s\n", Text);
    if( strlen(Text) != 0 ){

		/*set strH with nulls*/
		memset(BuffH,0,sizeof(BuffH));

		/*converting str character into Hex and adding into strH*/
		for(i=0,j=0;i<strlen(Text);i++,j+=2)
		{
			sprintf((char*)BuffH+j,"%02X",Text[i]);
		}
		BuffH[j]='\0'; /*adding NULL in the end*/
		strcat(Buff, (char*)BuffH);
	}

	strcat(Buff, SysExEox);
	add_send_hex_data(Buff);
	parse_data();

    send_hex[0] = '\0';
    //send_data[0] = '\0';
    //send_data_length = 0;

	if ((status = send_midi_interleaved()) < 0) {
		errormessage("cannot send data: %s", snd_strerror(status));
		exit(1);
	}
}

/*  Send midi out message.  */
void SendMidiOut(snd_rawmidi_t *hwMidi, char *MidiOut){
	int status;

    sprintf(sa_LogMessage,"UI2MCP --> MIDI : Midi Out : %02X %02X %02X\n", MidiOut[0], MidiOut[1], MidiOut[2]);
    LogTrace(hfErr, 5, debug, sa_LogMessage);

	if ((status = snd_rawmidi_write(hwMidi, MidiOut, sizeof(MidiOut))) < 0){
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}
    usleep(320);
}

/*  Send message to LCD in text mode.  */
void SendLCDTxt(snd_rawmidi_t *hwMidi, char *SysExHdr, int Inv, int i_RowLCD, char *Text){
    char Cmd[32] = "";
    char c_Canal[32] = "";

    int col = 0;
    int i_SliceStep = 13;
    //int i_RowLCD = 1;

    for (int j = 0; j < strlen(Text); j +=i_SliceStep){
        strcpy(Cmd, "12");
        sprintf(c_Canal, "%02X", col);
        strcat(Cmd, c_Canal);
        sprintf(c_Canal, "%02X", i_RowLCD);
        strcat(Cmd, c_Canal);
        if( Inv == 0 ){
            strcat(Cmd, "01");
        }
        if( Inv == 1 ){
            strcat(Cmd, "05");
        }

        char* substr;
        if ( strlen(Text) - j <= i_SliceStep ){
            substr = substring(Text, j, strlen(Text) - j);
        }
        else{
            substr = substring(Text, j, i_SliceStep);
        }

        SendSysExTextOut(midiout, SysExHdr, Cmd, substr);
        col++;

        free(substr);
    }
}

/*  LCD function for row navigation on explorer.  */
void LcdExplorerUpdate(snd_rawmidi_t *hwMidi, char *SysExHdr, int Index, char (*List)[2048][256]){
    int Row = 0;

    //printf("--------------------------------------------------------------------------------\n");
    for (int c = 0 + 6 * (int)floor((double)Index/6); c < 6 + 6 * (int)floor((double)Index/6); c++){
        if( Index != c ){
            SendLCDTxt(hwMidi, SysExHdr, 0, Row, (*List)[c]);
            //printf("LCD Explorer %i %i [%s]\n",  c, Index, (*List)[c]);
        }
        else{
            SendLCDTxt(hwMidi, SysExHdr, 1, Row, (*List)[c]);
            //printf("LCD Explorer %i %i [\"%s\"]\n",  c, Index, (*List)[c]);
        }
        Row++;
    }
    //printf("--------------------------------------------------------------------------------\n");
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
/* main code */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {

    //argv = malloc(sizeof(argc));
    //argv = malloc(argv sizeof argv);

	for (int x = 0; x < argc; ++x){
        if(strcmp(argv[x],"-h") == 0 || strcmp(argv[x],"--help") == 0){
            printf ("Usage: ui2mcp options\n\n");
            printf ("-h, --help                             this help\n");
            printf ("-v, --version                         print current version\n");
            printf ("-d, --debug  [level]             debug information in the ui2mcp.log filen\n");
            printf ("                                           - 1  Action information\n");
            printf ("                                           - 2  Translated information\n");
            printf ("                                           - 3  Transmit message information\n");
            printf ("                                           - 4  Low level information\n");
            printf ("                                           - 5  Very low level information like websocket\n");
            exit(0);
        }else if (strcmp(argv[x],"-v") == 0 || strcmp(argv[x],"--version") == 0){
            printf ("Version %s\n", FULLVERSION_STRING);
            exit(0);
        }else if(strcmp(argv[x],"-d") == 0 || strcmp(argv[x],"--debug") == 0){
            printf ("Debug actived\n");
            debug++;
            if ( argc >= 3 ){
                if ( strlen(argv[x+1]) != 0 ){
                        debug = atoi(argv[x+1]);
                        printf ("Log level: %i\n", debug);
                }
            }
        }
	}

	/*  time variable for led bliking Tap button  */
	int status = 0;
	int current_time_init;
	int current_time;

	/*  variable for Tap function  */
    int tap[255];
    int i_NbTap = 0;

	/*  variable for watchdog function  */
	int i_Watchdog_init;
	int i_Watchdog;

	/*  variable for Update timer function  */
	int i_UpdateListTimer;

	/*  variable for Vu meter function  */
	int i_VuMeter_init;
	int i_VuMeter;

    char  c_StxBuffer[1024] = "";

	int init = -1;

     /*  file open for log information  */
     if ((hfErr = fopen("/tmp/ui2mcp.log","w" )) == (FILE *)NULL){
          errormessage("\nError opening log file\n");
          exit(1);
     }

    LogTrace(hfErr, 0, debug, "### Starting to UI to Midi Controler... ###\n" );

    struct config ControlerConfig;
    ControlerConfig = get_config(FILENAME);

    char *ControlerName = ControlerConfig.ControlerName;
	char *ControlerMode = ControlerConfig.ControlerMode;
	int Lcd = atoi(ControlerConfig.Lcd);

    sprintf(sa_LogMessage,"UI2MCP <-- CONFIG : Controler Midi : (%s) Protocol : (%s)\n", ControlerName, ControlerMode);
    LogTrace(hfErr, 0, debug, sa_LogMessage);

	// Midi connexion  variable
	const char* portname = "";
    portname = ControlerConfig.MidiPort;
    sprintf(sa_LogMessage,"UI2MCP <-- CONFIG : Midi Port (%s)\n", portname);
    LogTrace(hfErr, 0, debug, sa_LogMessage);

	int mode = SND_RAWMIDI_NONBLOCK;                                // SND_RAWMIDI_SYNC
	//snd_rawmidi_t* midiout = NULL;
	//snd_rawmidi_t* midiin = NULL;
//	unsigned char Midibuffer[2048] = "";                                   // Storage for input buffer received
	unsigned char Midibuffer[3] = "";                                          // Storage for input buffer received
	int InMidi, MidiValue, MidiCC;

    // Network websocket
	//struct sockaddr_in address;
    int sock = 0;
    struct sockaddr_in serv_addr;
    //typedef struct in_addr IN_ADDR;
	char *ack = "GET /raw HTTP1.1\n\n";
	char *command = NULL;
    char buffer[1024] = "";

 	int AdrSplitComa, ArraySplitDot;
	char** SplitArrayComa = NULL;
	char** SplitArrayDot = NULL;

	/*  UI device variable for number per type of channel  */
    // TODO (pi#1#12/28/18): Improve automaticly with UI model
	int UIChannel = 24;
	int UIMedia = 2;
	int UISubGroup = 6;
	int UIFx = 4;
	int UIAux = 10;
	int UIMaster = 2;
	int UILineIn = 2;
	int UIVca = 6;
	int UIAllStrip = 56;

	/* structure variable for UI  */
	struct Ui ui[UIAllStrip];

    /*  Configuration of default color for UI channel  */
    int Numb = 0;
    for(int c = 0; c < UIChannel; c++){
            ui[c].Color = 0;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "i");
    }
    Numb = 0;
    for(int c = UIChannel; c < UIChannel+UILineIn; c++){
            ui[c].Color = 3;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "l");
    }
    Numb = 0;
    for(int c = UIChannel+UILineIn; c < UIChannel+UILineIn+UIMedia; c++){
            ui[c].Color = 11;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "p");
    }
    Numb = 0;
    for(int c = UIChannel+UILineIn+UIMedia; c < UIChannel+UILineIn+UIMedia+UIFx; c++){
            ui[c].Color = 7;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "f");
    }
    Numb = 0;
    for(int c = UIChannel+UILineIn+UIMedia+UIFx; c < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup; c++){
            ui[c].Color  = 8;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "s");
    }
    Numb = 0;
    for(int c = UIChannel+UILineIn+UIMedia+UIFx+UISubGroup; c < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux; c++){
            ui[c].Color = 5;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "a");
    }
    Numb = 0;
    for(int c = UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux; c < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca; c++){
            ui[c].Color = 6;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "v");
    }
    Numb = 0;
    for(int c = UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca; c < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca+UIMaster; c++){
            ui[c].Color = 2;
            ui[c].Position = c;
            ui[c].Numb = Numb++;
            strcpy(ui[c].Type, "m");
            strcpy(ui[c].Name, "MASTER");
    }

	/*  variable for UI websocket message  */
	char UImsg[64] = "";
	char UIio[64] = "";
	char UIfunc[64] = "";
	char UIsfunc[64] = "";
	char UIval[64] = "";
	char UIchan[24] = "";

	//float zeroDbPos = .7647058823529421;   //   0 db
	//float maxDb = 1;					                    // +10 db

	char c_SyncId[256];
    sscanf(ControlerConfig.SyncId, "%s", c_SyncId);

    sprintf(sa_LogMessage,"UI2MCP <-- CONFIG : UI SyncId (%s)\n", c_SyncId);
    LogTrace(hfErr, 0, debug, sa_LogMessage);

	char *UIModel = NULL;
	char UIFirmware[32] = "";
	char *UICommand = NULL;

	//UI MuteMask
	char UIMuteMask[16] = "";
	unsigned GroupMaskMute[6];
	unsigned GroupMaskMuteAll;
	unsigned GroupMaskMuteFx;

	//UI  MTK Session
	char UIMtkSessionList [2048][256];
	int MtkSessionIndex = 0;
	int MtkSessionMax = 0;
	char MtkSessionCurrent[256] = "";

	//UI  Directories Player
	char UIDirPlayerList [2048][256];
	int DirPlayerIndex = 0;
	int DirPlayerMax = 0;
	char DirPlayerCurrent[256] = "";

	//UI  Files Player
	char UIFilesPlayerList [2048][256];
	int FilesPlayerIndex = 0;
	int FilesPlayerMax = 0;
	char FilesPlayerCurrent[256] = "";

	//UI Shows
	char UIShowsList [2048][256];
	int ShowsIndex = 0;
	int ShowsMax = 0;
	char ShowsCurrent[256] = "";

	//UI SnapShot
	char UISnapShotList [2048][256];
	int SnapShotIndex = 0;
	int SnapShotMax = 0;
	char SnapShotCurrent[256] = "";

	//UI Cues
	char UICuesList [2048][256];
	int CuesIndex = 0;
	int CuesMax = 0;
	char CuesCurrent[256] = "";

	// Other Setting variable
	int SoloMode = 0;
	int UIBpm = 120;
	int delay = 60000/UIBpm;

	// Common variable
	int AddrMidiTrack = 0;	                                           // value of translation fader
	int Canal;

	int MtkCurrentState = -1;						                   // Value for Mtk STOP/PLAY
	int MediaPlayerCurrentState = -1;		                   // Value for Media Player STOP/PLAY
	int MtkRecCurrentState = -1;					               // Value for Mtk REC
	int MediaRecCurrentState = 0; //-1;			               // Value for MEDIA REC
	int DimMaster;				                    		               // Value for Dim Master
	int SoundCheck = -1;                                                // varaible for Sound Chack function

	// Parameter of MIDI device
    //int MidiValueOn = 0x7F;
	//int MidiValueOff = 0x00;

	// Parameter of MIDI device
	int NbMidiFader = atoi(ControlerConfig.NbMidiFader);
	int NbMidiTrack = UIAllStrip/NbMidiFader;             // Number by modulo of number of Midi channel
	int PanPressed = 0;                                                    // Variable activate Pan mode with Select mode
	int ShiftLeftPressed = 0;
	int ShiftRightPressed = 0;
	int SelectButtonPressed = -1;
	int ModeShowsPressed = 0;
	int ModeSnapShotsPressed = 0;
	int ModeCuesPressed = 0;
	int ModeMasterPressed = 0;
	int ModeDirPlayerPressed = 0;
	int ModeFilesPlayerPressed = 0;
	int ModePlayer = 0;
	int ModeMtkSessionsPressed = 0;

	//mapping midi controler to UI for fader 0

	// Fader = Ex ll hh
	int AddrMidiMix = 0;
    sscanf(ControlerConfig.AddrMidiMix, "%x", &AddrMidiMix);

	// Encoder = B0 10 xx
	int AddrMidiEncoder = 0; //0xB0;
    sscanf(ControlerConfig.AddrMidiEncoder, "%x", &AddrMidiEncoder);
        int AddrMidiEncoderSession = 0;
        sscanf(ControlerConfig.AddrMidiEncoderSession, "%x", &AddrMidiEncoderSession);
        int AddrMidiEncoderPan = 0; //0x10;
        sscanf(ControlerConfig.AddrMidiEncoderPan, "%x", &AddrMidiEncoderPan);
        int NbPanButton = atoi(ControlerConfig.NbPanButton);
        int NbPanStep[UIAllStrip];

        sprintf(sa_LogMessage,"UI2MCP <-- CONFIG : Pan type : (%s)\n", ControlerConfig.TypePan);
        LogTrace(hfErr, 0, debug, sa_LogMessage);

	// Button/Led 90 ID CC
	int AddrMidiButtonLed = 0; //0x90;
    sscanf(ControlerConfig.AddrMidiButtonLed, "%x", &AddrMidiButtonLed);
        int AddrMidiRec = -1; //0x00;
        sscanf(ControlerConfig.AddrMidiRec, "%x", &AddrMidiRec);
        int NbRecButton = atoi(ControlerConfig.NbRecButton);
        int ArmRecAll = 0;

        int AddrMidiMute = 0; //0x10;
        sscanf(ControlerConfig.AddrMidiMute, "%x", &AddrMidiMute);
        int AddrMidiSolo = 0; //0x08;
        sscanf(ControlerConfig.AddrMidiSolo, "%x", &AddrMidiSolo);
        int AddrMidiMaster = 0;
        sscanf(ControlerConfig.AddrMidiMaster, "%x", &AddrMidiMaster);
        int AddrMidiSelect = 0; //0x18;
        sscanf(ControlerConfig.AddrMidiSelect, "%x", &AddrMidiSelect);

        //printf("%02X\n",AddrMidiSelect);

        int AddrMidiParamButton = 0; //0x20;
        sscanf(ControlerConfig.AddrMidiParamButton, "%x", &AddrMidiParamButton);
        int AddrMidiSessionButton = 0; //0x20;
        sscanf(ControlerConfig.AddrMidiSessionButton, "%x", &AddrMidiSessionButton);
        int AddrMuteClear = 0; //0x02;
        sscanf(ControlerConfig.AddrMuteClear, "%x", &AddrMuteClear);
        int AddrMuteSolo = 0; //0x01;
        sscanf(ControlerConfig.AddrMuteSolo, "%x", &AddrMuteSolo);
        int AddrShiftLeft = 0; //0x46;
        sscanf(ControlerConfig.AddrShiftLeft, "%x", &AddrShiftLeft);
        int AddrShiftRight = 0; //0x06;
        sscanf(ControlerConfig.AddrShiftRight, "%x", &AddrShiftRight);


        int AddrMidiTouch = 0;// = 0x68;
        sscanf(ControlerConfig.AddrMidiTouch, "%x", &AddrMidiTouch);

        //printf("%02X\n",AddrMidiButtonLed);

        int IdTrackPrev = 0; //0x2E;                           /*  Generic ID  */
        sscanf(ControlerConfig.IdTrackPrev, "%x", &IdTrackPrev);
        int IdTrackNext = 0; //0x2F;                          /*  Generic ID  */
        sscanf(ControlerConfig.IdTrackNext, "%x", &IdTrackNext);
        int IdLoop = 0; //0x56;                                   /*  Generic ID  */
        sscanf(ControlerConfig.IdLoop, "%x", &IdLoop);
        int IdMarkerSet = 0; //0x59;
        sscanf(ControlerConfig.IdMarkerSet, "%x", &IdMarkerSet);
        int IdMarkerLeft = 0; //0x58;
        sscanf(ControlerConfig.IdMarkerLeft, "%x", &IdMarkerLeft);
        int IdMarkerRight = 0; //0x5A;
        sscanf(ControlerConfig.IdMarkerRight, "%x", &IdMarkerRight);
        int IdRewind = 0; //0x5B;                               /*  Generic ID  */
        sscanf(ControlerConfig.IdRewind, "%x", &IdRewind);
        int IdForward = 0; //0x5C;                             /*  Generic ID  */
        sscanf(ControlerConfig.IdForward, "%x", &IdForward);
        int IdStop = 0; //0x5D;                                   /*  Generic ID  */
        sscanf(ControlerConfig.IdStop, "%x", &IdStop);
        int IdPlay = 0; //0x5E;                                    /*  Generic ID  */
        sscanf(ControlerConfig.IdPlay, "%x", &IdPlay);
        int IdRec = 0; //0x5F;                                     /*  Generic ID  */
        sscanf(ControlerConfig.IdRec, "%x", &IdRec);

	// Value Bar for FaderPort
	int AddrMidiBar = 0; //0xB0;
    sscanf(ControlerConfig.AddrMidiBar, "%x", &AddrMidiBar);
	int AddrMidiValueBar = 0; //0x30;
    sscanf(ControlerConfig.AddrMidiValueBar, "%x", &AddrMidiValueBar);

	// SysExHdr for FaderPort
	char SysExHdr[11] = "";
	strcpy(SysExHdr, ControlerConfig.SysExHdr);

//    printf("%s",ControlerConfig.i_Tap);
//    printf("%s",ControlerConfig.i_Dim);
//    printf("%s",ControlerConfig.i_SnapShotNavUp);
//    printf("%s",ControlerConfig.i_SnapShotNavDown);
//    printf("\n");

    /*  Link between button & led and ID  */
    int i_Tap = 0; //IdLoop;
    sscanf(ControlerConfig.i_Tap, "%x", &i_Tap);
    int i_Dim = 0; //IdMarkerSet;
    sscanf(ControlerConfig.i_Dim, "%x", &i_Dim);
    int i_SnapShotNavUp = 0; //IdMarkerLeft;
    sscanf(ControlerConfig.i_SnapShotNavUp, "%x", &i_SnapShotNavUp);
    int i_SnapShotNavDown = 0; //IdMarkerRight;
    sscanf(ControlerConfig.i_SnapShotNavDown, "%x", &i_SnapShotNavDown);
    int i_StopUI2Mcp = 0; //IdRewind;
    sscanf(ControlerConfig.i_StopUI2Mcp, "%x", &i_StopUI2Mcp);
    int i_Validation = 0; //IdForward;
    sscanf(ControlerConfig.i_Validation, "%x", &i_Validation);

    int AddrSoundCheck = 0; //IdForward;
    sscanf(ControlerConfig.AddrSoundCheck, "%x", &AddrSoundCheck);
    int AddrShowsSelect = 0;
    sscanf(ControlerConfig.AddrShowsSelect, "%x", &AddrShowsSelect);
    int AddrSnapShotsSelect = 0;
    sscanf(ControlerConfig.AddrSnapShotsSelect, "%x", &AddrSnapShotsSelect);
    int AddrCuesSelect = 0;
    sscanf(ControlerConfig.AddrCuesSelect, "%x", &AddrCuesSelect);
    int AddrPanSelect = 0;
    sscanf(ControlerConfig.AddrPanSelect, "%x", &AddrPanSelect);
    int AddrMediaSelect = 0;
    sscanf(ControlerConfig.AddrMediaSelect, "%x", &AddrMediaSelect);
    int AddrSessionSelect = 0;
    sscanf(ControlerConfig.AddrSessionSelect, "%x", &AddrSessionSelect);
    int AddrTransportModeSelect = 0;
    sscanf(ControlerConfig.AddrTransportModeSelect, "%x", &AddrTransportModeSelect);

    /*  Configure LCD in text mode.  */
    void ConfigLCDTxtMode(){
            char Cmd[32] = "";
            char c_Canal[32] = "";

            for (int j = 0; j < NbMidiFader; j++){
                strcpy(Cmd, "13");
                sprintf(c_Canal, "%02X", j);
                strcat(Cmd, c_Canal);
                strcat(Cmd, "19");
                SendSysExTextOut(midiout, SysExHdr, Cmd, "");
            }
    }

    /*  Configure LCD in mixer mode.  */
    void ConfigLCDMixerMode(){
            char Cmd[32] = "";
            char c_Canal[32] = "";

            for (int j = 0; j < NbMidiFader; j++){
                strcpy(Cmd, "13");
                sprintf(c_Canal, "%02X", j);
                strcat(Cmd, c_Canal);
                strcat(Cmd, "18");
                SendSysExTextOut(midiout, SysExHdr, Cmd, "");

                char BarSet[3] = {0xB0, 0x38+j, 0x01};
                SendMidiOut(midiout, BarSet);

                char Bar[3] = {0xB0, 0x30+j, 0x3F};
                SendMidiOut(midiout, Bar);
        }
    }

/*  Update the Midi controler light, button, LCD with struture UI variable.  */
void UpdateMidiControler(){

  if(init >= 1){

    // Select
    for( int j=0; j < NbMidiFader; j++ ){
        if( ui[j].Color == 0 ){  // BUG a Verifier Canal change par j
            char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSelect+j, 0x00};
            SendMidiOut(midiout, MidiArray);
        }
        else{
            char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSelect+j, 0x7F};
            SendMidiOut(midiout, MidiArray);
        }
    }
    SelectButtonPressed = -1;

    if ( ModeMasterPressed == 1 ){

        //printf("Mode Master [%i]\n", NbMidiFader);

        // Mix for Master Mode
        int MidiValue = 0;
        MidiValue = (127 * ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].MixMidi);
        char MidiArray[3] = {AddrMidiMix+NbMidiFader, MidiValue, MidiValue};
        SendMidiOut(midiout, MidiArray);

        // Color for Master Mode
        char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+NbMidiFader , 0x7F};
        SendMidiOut(midiout, MidiArrayOn);
        char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+NbMidiFader , (int)floor((double)127/(double)255*139)};
        SendMidiOut(midiout, MidiArrayR);
        char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+NbMidiFader , (int)floor((double)127/(double)255*0)};
        SendMidiOut(midiout, MidiArrayG);
        char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+NbMidiFader , (int)floor((double)127/(double)255*0)};
        SendMidiOut(midiout, MidiArrayB);

        // Name to LCD for Master Mode
        if ( Lcd == 1 ){
            char Cmd[32] = "";
            char c_Canal[32] = "";
            char c_CanalText[32] = "";

            strcpy(Cmd, "12");
            sprintf(c_Canal, "%02X", NbMidiFader);
            sprintf(c_CanalText, "R/L");
            strcat(Cmd, c_Canal);
            strcat(Cmd, "0100");
            SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

            strcpy(Cmd, "12");
            sprintf(c_Canal, "%02X", NbMidiFader);
            strcat(Cmd, c_Canal);
            strcat(Cmd, "0000");
            SendSysExTextOut(midiout, SysExHdr, Cmd, ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].Name);

            //char BarSet[3] = {0xB0, 0x38+NbMidiFader, 0x01};
            //SendMidiOut(midiout, BarSet);

            //printf("Pan Master %i [%f]\n", UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca, ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].PanMidi);

            char Bar[3] = {0xB0, 0x30+NbMidiFader, ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].PanMidi*0x7F};
            SendMidiOut(midiout, Bar);

            strcpy(Cmd, "12");
            sprintf(c_Canal, "%02X", NbMidiFader);
            strcat(Cmd, c_Canal);
            strcat(Cmd, "0200");

            if( ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].PanMidi == 0.5 ){
                sprintf(c_CanalText, "<C>");
            }
            else if( ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].PanMidi < 0.5 ){
                sprintf(c_CanalText, "L%i", (int)(-200*ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].PanMidi+100));
            }
            else if( ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].PanMidi > 0.5 ){
                sprintf(c_CanalText, "R%i", (int)(200*ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].PanMidi-100));
            }
            SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
        }
    }

    for (int Canal = NbMidiFader*AddrMidiTrack; Canal < (NbMidiFader*AddrMidiTrack)+NbMidiFader; Canal++){

        // Mute
        int i_OrMute = (ui[Canal].MaskMute | ui[Canal].Mute) & ( ! (ui[Canal].ForceUnMute));
        char MidiArrayM[3] = {AddrMidiButtonLed, AddrMidiMute+(Canal % NbMidiFader), i_OrMute*0x7F};
        SendMidiOut(midiout, MidiArrayM);

        // Pan BAR LCD
        if ( Lcd == 1 ){
            if( strcmp(ui[Canal].Type,"a") == 0 || strcmp(ui[Canal].Type,"v") == 0 ){

                char BarSet[3] = {0xB0, 0x38+(Canal % NbMidiFader), 0x04};
                SendMidiOut(midiout, BarSet);

                char Cmd[32] = "";
                char c_Canal[32] = "";

                strcpy(Cmd, "12");
                sprintf(c_Canal, "%02X", Canal % NbMidiFader);
                strcat(Cmd, c_Canal);
                strcat(Cmd, "0200");
                SendSysExTextOut(midiout, SysExHdr, Cmd, "");

            }
            else{
                char BarSet[3] = {0xB0, 0x38+(Canal % NbMidiFader), 0x01};
                SendMidiOut(midiout, BarSet);

                char Bar[3] = {0xB0, 0x30+(Canal % NbMidiFader), ui[Canal].PanMidi*0x7F};
                SendMidiOut(midiout, Bar);

                char Cmd[32] = "";
                char c_Canal[32] = "";
                char c_CanalText[32] = "";

                strcpy(Cmd, "12");
                sprintf(c_Canal, "%02X", Canal % NbMidiFader);
                strcat(Cmd, c_Canal);
                strcat(Cmd, "0200");

                if( ui[Canal].PanMidi == 0.5 ){
                    sprintf(c_CanalText, "<C>");
                }
                else if( ui[Canal].PanMidi < 0.5 ){
                    sprintf(c_CanalText, "L%i", (int)(-200*ui[Canal].PanMidi+100));
                }
                else if( ui[Canal].PanMidi > 0.5 ){
                    sprintf(c_CanalText, "R%i", (int)(200*ui[Canal].PanMidi-100));
                }
                SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
            }
        }

        // Solo
        char MidiArrayS[3] = {AddrMidiButtonLed, AddrMidiSolo+(Canal % NbMidiFader), ui[Canal].Solo*0x7F};
        SendMidiOut(midiout, MidiArrayS);

        // Rec
        if(NbRecButton == NbMidiFader){
            char MidiArrayR[3] = {AddrMidiButtonLed, AddrMidiRec+(Canal % NbMidiFader), ui[Canal].Rec*0x7F};
            SendMidiOut(midiout, MidiArrayR);
        }
        else{
            int OrRec = 0;
            for (int j = 0; j < UIAllStrip; j++){
                if( (strcmp(ui[j].Type, "i") == 0 || strcmp(ui[j].Type, "l") == 0) && (j <= 19 || j >= 24) && UIChannel == 24 ){
                    OrRec = OrRec || ui[j].Rec;
                }
            }
            char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec, OrRec*0x7F};
            SendMidiOut(midiout, MidiArray);
        }

        // Mix
        //printf("Informations Type %s Pan %f\n", ui[Canal].Type, ui[Canal].PanMidi );
//        if ( strcmp(ui[Canal].Type,"m") == 0 && ( ui[Canal].PanMidi < 0.5 || ui[Canal].PanMidi > 0.5 ) ){
//        if ( strcmp(ui[Canal].Type,"m") == 0 && ui[Canal].PanMidi != 0.5 ){
            if( strcmp(ui[Canal].Type,"m") == 0 && ui[Canal].PanMidi < 0.5 && ui[Canal].Numb == 1 ){
                int MidiValueR = 0;
                MidiValueR = (127 * (ui[Canal].MixMidi-ui[Canal].MixMidi*(-200*ui[Canal].PanMidi+100)/100));

                char MidiArrayRight[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueR, MidiValueR};
                SendMidiOut(midiout, MidiArrayRight);
            }
            else if( strcmp(ui[Canal].Type,"m") == 0 && ui[Canal].PanMidi > 0.5 && ui[Canal].Numb == 0 ){
                int MidiValueL = 0;
                MidiValueL = (127 * (ui[Canal].MixMidi-ui[Canal].MixMidi*(200*ui[Canal].PanMidi-100)/100));

                char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueL, MidiValueL};
                SendMidiOut(midiout, MidiArrayLeft);
            }
            else{
                int MidiValue = 0;
                MidiValue = (127 * ui[Canal].MixMidi);
                char MidiArray[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                SendMidiOut(midiout, MidiArray);
            }
//        }
//        else{
//            int MidiValue = 0;
//            MidiValue = (127 * ui[Canal].MixMidi);
//            char MidiArray[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
//            SendMidiOut(midiout, MidiArray);
//        }

        // Color to SET button
        if(ui[Canal].Color == 0){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x00};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 1){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x00};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 2){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*139)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 3){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 4){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*165)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 5){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 6){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*86)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*222)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*67)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 7){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*145)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*194)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 8){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*148)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*211)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 9){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*128)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*128)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*128)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 10){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayB);
        }else if(ui[Canal].Color == 11){
            char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
            SendMidiOut(midiout, MidiArrayOn);
            char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
            SendMidiOut(midiout, MidiArrayR);
            char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
            SendMidiOut(midiout, MidiArrayG);
            char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*147)};
            SendMidiOut(midiout, MidiArrayB);
        }

        // Name to LCD

        if ( Lcd == 1 ){
            char Cmd[32] = "";
            char c_Canal[32] = "";
            char c_CanalText[32] = "";

            strcpy(Cmd, "12");
            sprintf(c_Canal, "%02X", Canal-(NbMidiFader*AddrMidiTrack));
            if(strcmp(ui[Canal].Type, "i") == 0){sprintf(c_CanalText, "C%i", ui[Canal].Numb+1);}
            if(strcmp(ui[Canal].Type, "l") == 0 && ui[Canal].Numb == 0){sprintf(c_CanalText, "LL");}
            if(strcmp(ui[Canal].Type, "l") == 0 && ui[Canal].Numb == 1){sprintf(c_CanalText, "LR");}
            if(strcmp(ui[Canal].Type, "p") == 0 && ui[Canal].Numb == 0){sprintf(c_CanalText, "PL");}
            if(strcmp(ui[Canal].Type, "p") == 0 && ui[Canal].Numb == 1){sprintf(c_CanalText, "PR");}
            if(strcmp(ui[Canal].Type, "f") == 0){sprintf(c_CanalText, "F%i", ui[Canal].Numb+1);}
            if(strcmp(ui[Canal].Type, "s") == 0){sprintf(c_CanalText, "S%i", ui[Canal].Numb+1);}
            if(strcmp(ui[Canal].Type, "a") == 0){sprintf(c_CanalText, "A%i", ui[Canal].Numb+1);}
            if(strcmp(ui[Canal].Type, "v") == 0){sprintf(c_CanalText, "V%i", ui[Canal].Numb+1);}
            if(strcmp(ui[Canal].Type, "m") == 0 && Canal == UIAllStrip-2){sprintf(c_CanalText, "ML");}
            if(strcmp(ui[Canal].Type, "m") == 0 && Canal == UIAllStrip-1){sprintf(c_CanalText, "MR");}
            strcat(Cmd, c_Canal);
            if( ui[Canal].Rec == 0 ){
                strcat(Cmd, "0100");
            }
            if( ui[Canal].Rec == 1 ){
                strcat(Cmd, "0104");
            }
            SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

            strcpy(Cmd, "12");
            sprintf(c_Canal, "%02X", Canal-(NbMidiFader*AddrMidiTrack));
            strcat(Cmd, c_Canal);
            strcat(Cmd, "0000");
            SendSysExTextOut(midiout, SysExHdr, Cmd, ui[Canal].Name);

            sprintf(sa_LogMessage,"UI2MCP --> MIDI : Track Update : Canal(%i) : Name[%s] Color[%i] Mix[%f] Solo[%i] Rec[%i] AddrModuloValue 0x%02X (Mute | MaskMute | ! ForceUnMute = %i * 0x7F)\n", Canal, ui[Canal].Name, ui[Canal].Color, ui[Canal].MixMidi, ui[Canal].Solo, ui[Canal].Rec, AddrMidiMute+(Canal % NbMidiFader), i_OrMute);
            LogTrace(hfErr, 1, debug, sa_LogMessage);

            //printf("Canal(%i) STRIP(%s.%i) MIX(%i) MUTE(%i) SOLO(%i) REC(%i) COLOR(%i) %s %s\n", Canal, ui[Canal].Type, ui[Canal].Numb, (int)(127*ui[Canal].MixMidi), (ui[Canal].MaskMute | ui[Canal].Mute) & ( ! (ui[Canal].ForceUnMute)), ui[Canal].Solo, ui[Canal].Rec, ui[Canal].Color, c_CanalText, ui[Canal].Name);
        }
    }
   }
}

	/*  Control the handle signal  CONTROL^C (SIGINT)  */
	//Structure pour l'enregistrement d'une action déclenchée lors de la reception d'un signal.
	struct sigaction action, oldAction;

	action.sa_handler = signalHandler;	//La fonction qui sera appellé est signalHandler

	//On vide la liste des signaux bloqués pendant le traitement
	//C'est à dire que si n'importe quel signal (à part celui qui est traité)
	//est declenché pendant le traitement, ce traitement sera pris en compte.
	sigemptyset(&action.sa_mask);

	//Redémarrage automatique Des appels systêmes lents interrompus par l'arrivée du signal.
	action.sa_flags = SA_RESTART;

	//Mise en place de l'action pour le signal SIGINT, l'ancienne action est sauvegardé dans oldAction
	sigaction(SIGINT, &action, &oldAction);

	// Connection MIDI device
	if ((status = snd_rawmidi_open(&midiin, &midiout, portname, mode)) < 0){
      errormessage("\nProblem opening MIDI input & output: %s\n", snd_strerror(status));
      exit(1);
	}

    /*  Startup message  */
    if ( Lcd == 1 ){
        ConfigLCDTxtMode();

        SendLCDTxt(midiout, SysExHdr, 0, 0, "####################################################");
        usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
        SendLCDTxt(midiout, SysExHdr, 0, 1, "     Read configuration file               [DONE]");
        usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
        SendLCDTxt(midiout, SysExHdr, 0, 2, "     ID Mapping to Midi controler          [DONE]");
        usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
    }


	// Connection socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        errormessage("\n Socket creation error \n");
        exit(1);
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    char UiAddr[15] ;
    sscanf(ControlerConfig.UiAddr, "%s", UiAddr);

    sprintf(sa_LogMessage,"UI2MCP <-- CONFIG : UI Addr : (%s)\n", UiAddr);
    LogTrace(hfErr, 0, debug, sa_LogMessage);

    // Convert IPv4 and IPv6 addresses from text to binary form
//    if(inet_pton(AF_INET, "192.168.0.10", &serv_addr.sin_addr)<=0)
    if(inet_pton(AF_INET, UiAddr, &serv_addr.sin_addr)<=0)
    {
        errormessage("\nInvalid address/ Address not supported \n");
        exit(1);
    }

    if ( Lcd == 1 ){
        SendLCDTxt(midiout, SysExHdr, 0, 3, "     Connecttion to UI'");
        usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        errormessage("\nConnection Failed \n");
        exit(1);
    }

    if ( Lcd == 1 ){
        SendLCDTxt(midiout, SysExHdr, 0, 3, "     Connecttion to UI                     [DONE]");
        usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
    }

    send(sock , ack , strlen(ack) , 0 );
    LogTrace(hfErr, 1, debug, "UI2MCP --> UI : GET send\n" );

	// Init value array
	for (int c = 0; c < UIAllStrip; c++){
        ui[c].MixMidi = 0;
        ui[c].Solo = 0;
        ui[c].Mute = 0;
        ui[c].ForceUnMute = 0;
        ui[c].Rec = 0;
        ui[c].MaskMute = 0;
        ui[c].MaskMuteValue = 0;
        NbPanStep[c] = 0;
	}
	for (int c = 0; c < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup; c++){
		ui[c].PanMidi = .5;			// Pan //MidiTable = Pan potentiometer 0=left 0.5=center 1=right
	}
	for (int mask = 0; mask < 6; mask++){
		GroupMaskMute[mask] = 0;
	}

    /*  Reset Button light on Midi Controler  */
    for (int j = 0; j < 0xFF; j++){
       char MidiArray[3] = {AddrMidiButtonLed, j, 0x00};
       SendMidiOut(midiout, MidiArray);
    }

    if ( Lcd == 1 ){
        SendLCDTxt(midiout, SysExHdr, 0, 4, "     Initialization of the software        [DONE]");
        usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
    }

    if ( Lcd == 0 ){
        // Led Forward On for AddrMidiTrack = 0
        char ForwardLedOn[3] = {AddrMidiButtonLed, IdRewind, 0x7F};
        SendMidiOut(midiout, ForwardLedOn);
    }

    if ( Lcd == 1 ){
        char Version[256] = "";
        strcpy(Version, "     Lauch   Program - Version [");
        strcat(Version, FULLVERSION_STRING);
        strcat(Version, "]");
        SendLCDTxt(midiout, SysExHdr, 0, 5, Version);
        SendLCDTxt(midiout, SysExHdr, 0, 6, "####################################################");
    }

    sleep(2);

    /*  Configuration mode LCD  */
    if ( Lcd == 1 ){
        ConfigLCDMixerMode();
    }

    /*  Initialization of timer variable  */
    current_time_init = currentTimeMillis();
    i_Watchdog_init = currentTimeMillis();
    i_VuMeter_init = currentTimeMillis();

    i_UpdateListTimer = 0;

    /*  -------------------------------------------------------------------------------------------------------------------------  */
    /*  Principal function to receive, put in memories and translate to interface  */
    /*  -------------------------------------------------------------------------------------------------------------------------  */

    do {

        /*  Watchdog  */
        i_Watchdog = currentTimeMillis();
        if (i_Watchdog >= (i_Watchdog_init + 1000)){                                                  /*  Watchdog to send message to UI & Controler  */

            LogTrace(hfErr, 1, debug, "UI2MCP --> UI : ALIVE\n");
            command = "ALIVE\n";
            send(sock , command, strlen(command) , 0 );

            char c_RunningStatus[3] = {0xA0, 0x00, 0x00};
            SendMidiOut(midiout, c_RunningStatus);

            i_UpdateListTimer++;

            if( init >= 1 && i_UpdateListTimer >= 3 ){
                char s_Cmd[32];
                sprintf(s_Cmd,"SNAPSHOTLIST^%s\n", ShowsCurrent);
                send(sock , s_Cmd, strlen(s_Cmd) , 0 );

                sprintf(s_Cmd,"CUELIST^%s\n", ShowsCurrent);
                send(sock , s_Cmd, strlen(s_Cmd) , 0 );

                i_UpdateListTimer = 0;
            }

            i_Watchdog_init = currentTimeMillis();
        }

        /*  test difference for TAP  timer  */
        delay = 60000/UIBpm;
        current_time = currentTimeMillis();

        if (current_time >= (current_time_init + delay)){                                              /*  Tap function  */
            char bpmon[3] = {AddrMidiButtonLed, i_Tap, 0x7F};
            char bpmoff[3] = {AddrMidiButtonLed, i_Tap, 0x00};
            SendMidiOut(midiout, bpmon);
            usleep( 25000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
            SendMidiOut(midiout, bpmoff);
            current_time_init = currentTimeMillis();
        }

        if ((status = snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer))) < 0){
            memset(buffer, 0, sizeof(buffer));
            if(recv(sock, buffer, sizeof(buffer), 0) != SOCKET_ERROR){

//                /*  led blinking for ui & midi activity.  */
//                char UiI[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
//                SendMidiOut(midiout, UiI);
//                usleep( 5 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
//                char UiO[3] = {AddrMidiButtonLed, IdPlay, 0x00};
//                SendMidiOut(midiout, UiO);

                char *ArrayBuffer = NULL;
                char *UIMessage = NULL;
                int AdrSplitCRLF = 0;
                char **SplitArrayCRLF = NULL;;
                int sizeStr = 0;
                int sizeStxBuffer = 0;
                int sizeUIMessage = 0;

                sprintf(sa_LogMessage,"UI2MCP <-- UI : Len websocket (%i) Message websocket\n", strlen(buffer));
                LogTrace(hfErr, 5, debug, sa_LogMessage);

//                LogTrace(hfErr, 4, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
//                sprintf(sa_LogMessage,"%s\n", buffer);
//                LogTrace(hfErr, 4, debug, sa_LogMessage);
//                LogTrace(hfErr, 4, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

                sizeStr=strlen(buffer);

                ArrayBuffer=(char*) malloc( sizeof(char)*(sizeStr+1));
                strcpy(ArrayBuffer, buffer);

                sprintf(sa_LogMessage,"<#|%s|#>\n", ArrayBuffer);
                LogTrace(hfErr, 5, debug, sa_LogMessage);

                SplitArrayCRLF=split(ArrayBuffer,"\n",1);

                for(AdrSplitCRLF=0; SplitArrayCRLF[AdrSplitCRLF]!=NULL; AdrSplitCRLF++){

                    sizeUIMessage=strlen(SplitArrayCRLF[AdrSplitCRLF]);
                    if(strlen(c_StxBuffer) != 0){
                        sizeStxBuffer  =  strlen(c_StxBuffer);

                        UIMessage=(char*) malloc( sizeof(char)*(sizeUIMessage+sizeStxBuffer+2) );

                        // Remove the last caractere of  c_StxBuffer
                        char *c_StripStxBuffer = NULL;
                        c_StripStxBuffer = malloc (sizeof (*c_StripStxBuffer) * (strlen (c_StxBuffer) + 1));
                        int i, j;
                        for (i = 0, j = 0; c_StxBuffer[i]; i++){
                            if (strlen (c_StxBuffer) - 1 == i)
                            {
                                c_StripStxBuffer[j] = '\0';
                                j++;
                            }
                            else
                            {
                                c_StripStxBuffer[j] = c_StxBuffer[i];
                                j++;
                            }
                        }

                        strcpy(UIMessage, c_StripStxBuffer);
                        free(c_StripStxBuffer);

                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : Rest last message => len %i [%s]\n", strlen(UIMessage), UIMessage);
                        LogTrace(hfErr, 5, debug, sa_LogMessage);

//                        sprintf(sa_LogMessage,"%s %s\n", UIMessage, SplitArrayCRLF[AdrSplitCRLF]);
//                        LogTrace(hfErr, 4, debug, sa_LogMessage);

                        strcat(UIMessage, SplitArrayCRLF[AdrSplitCRLF]);
                        memset (c_StxBuffer, 0, sizeof (c_StxBuffer));
                    }
                    else{
                        sizeStxBuffer  =  strlen(c_StxBuffer);

                        UIMessage=(char*) malloc( sizeof(char)*(sizeUIMessage+1) );
                        strcpy(UIMessage,SplitArrayCRLF[AdrSplitCRLF]);
                    }

                    if (strstr(UIMessage, "\x02")){
                        strcpy(c_StxBuffer, UIMessage);
                        sprintf(sa_LogMessage,"UI2MCP <-- UI : Not complit message not used now => len %i [%s]\n", strlen(c_StxBuffer), c_StxBuffer);
                        LogTrace(hfErr, 5, debug, sa_LogMessage);
                        break;
                    }

//                    char UiI[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
//                    SendMidiOut(midiout, UiI);
//                    usleep( 5 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
//                    char UiO[3] = {AddrMidiButtonLed, IdPlay, 0x00};
//                    SendMidiOut(midiout, UiO);

                    sprintf(sa_LogMessage,"UI2MCP <-- UI : WS Mesg - [%s]\n", UIMessage);
                    LogTrace(hfErr, 4, debug, sa_LogMessage);

                    //printf("%s\n", sa_LogMessage);

//                    if( strlen(UIMessage) == 1 || init ==0 ){
                    if( init ==0 ){
//                        if(init == 0){
                            init = 1;

                            UpdateMidiControler();

                            command = "IOSYS^Connexion UI2MCP\n";
                            LogTrace(hfErr, 0, debug, "Send command IOSYS\n");
                            send(sock , command, strlen(command) , 0 );

                            command = "MODEL\n";
                            LogTrace(hfErr, 0, debug, "Send command MODEL\n");
                            send(sock , command, strlen(command) , 0 );

                            command = "VERSION\n";
                            LogTrace(hfErr, 0, debug, "Send command VERSION\n");
                            send(sock , command, strlen(command) , 0 );

                            command = "SHOWLIST\n";
                            LogTrace(hfErr, 0, debug, "Send command SHOWLIST\n");
                            send(sock , command, strlen(command) , 0 );
//                        }
                    }
                    if(strstr(UIMessage, "UPDATE_PLAYLIST")){
                                                // First message of UI.
                        //init = -2;       // Initialize the flag for detection of the last message.
                    }
                    if(strstr(UIMessage, "RTA^")){
                        if(init < 0){init++;}       // Initialize the flag for detection of the last message.
                        // Actually nothing... futur VuMeter message to LCD MIDI Controler
                    }
                    if(strstr(UIMessage, "VU2^")){

                        SplitArrayComa=split(UIMessage,"^",1);
                        free(SplitArrayComa);

                        char *enc = SplitArrayComa[1];

                       unsigned char *dec = b64_decode(enc, strlen(enc));

                       int i_k = 0;
                       //int i_size64 = (3*strlen(enc))/4;

                       UIChannel = (int)dec[0];
                       UIMedia = (int)dec[1];
                       UISubGroup = (int)dec[2];
                       UIFx = (int)dec[3];
                       UIAux = (int)dec[4];
                       UIMaster = (int)dec[5];
                       UILineIn = (int)dec[6];

                        int t = 8;
                        //Inputs
                        for (i_k = 0; i_k < UIChannel; ++i_k) {
                                ui[i_k].vuPre = (int)dec[t];
                                ui[i_k].vuPost = (int)dec[t+1];
                                ui[i_k].vuPostFader = (int)dec[t+2];
                                ui[i_k].vuGateIn = (int)dec[t+3];
                                ui[i_k].vuCompOut = (int)dec[t+4];
                                if((int)dec[t+5] & 0x80){
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+5]^0x80);
                                    ui[i_k].Gate = 0x01;
                                }else{
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+5]);
                                    ui[i_k].Gate = 0x00;
                                }
                                t = t+6;
                        }
                        //Media
                        for (i_k = UIChannel+UILineIn; i_k < UIChannel+UILineIn+UIMedia; ++i_k) {
                                ui[i_k].vuPre = (int)dec[t];
                                ui[i_k].vuPost = (int)dec[t+1];
                                ui[i_k].vuPostFader = (int)dec[t+2];
                                ui[i_k].vuGateIn = (int)dec[t+3];
                                ui[i_k].vuCompOut = (int)dec[t+4];
                                if((int)dec[t+5] & 0x80){
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+5]^0x80);
                                    ui[i_k].Gate = 0x01;
                                }else{
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+5]);
                                    ui[i_k].Gate = 0x00;
                                }
                                t = t+6;
                        }
                        //Sub
                        for (i_k = UIChannel+UILineIn+UIMedia+UIFx; i_k < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup; ++i_k) {
                                ui[i_k].vuPostL = (int)dec[t];
                                ui[i_k].vuPostR = (int)dec[t+1];
                                ui[i_k].vuPostFaderL = (int)dec[t+2];
                                ui[i_k].vuPostFaderR = (int)dec[t+3];
                                ui[i_k].vuGateIn = (int)dec[t+4];
                                ui[i_k].vuCompOut = (int)dec[t+5];
                                if((int)dec[t+6] & 0x80){
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+6]^0x80);
                                    ui[i_k].Gate = 0x01;
                                }else{
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+6]);
                                    ui[i_k].Gate = 0x00;
                                }
                                t = t+7;
                        }
                        //FX
                        for (i_k = UIChannel+UILineIn+UIMedia; i_k < UIChannel+UILineIn+UIMedia+UIFx; ++i_k) {
                                ui[i_k].vuPostL = (int)dec[t];
                                ui[i_k].vuPostR = (int)dec[t+1];
                                ui[i_k].vuPostFaderL = (int)dec[t+2];
                                ui[i_k].vuPostFaderR = (int)dec[t+3];
                                ui[i_k].vuGateIn = (int)dec[t+4];
                                ui[i_k].vuCompOut = (int)dec[t+5];
                                if((int)dec[t+6] & 0x80){
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+6]^0x80);
                                    ui[i_k].Gate = 0x01;
                                }else{
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+6]);
                                    ui[i_k].Gate = 0x00;
                                }
                                t = t+7;
                        }
                        //Aux
                        for (i_k = UIChannel+UILineIn+UIMedia+UIFx+UISubGroup; i_k < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux; ++i_k) {
                                ui[i_k].vuPost = (int)dec[t];
                                ui[i_k].vuPostFader = (int)dec[t+1];
                                ui[i_k].vuGateIn = (int)dec[t+2];
                                ui[i_k].vuCompOut = (int)dec[t+3];
                                if((int)dec[t+4] & 0x80){
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+4]^0x80);
                                    ui[i_k].Gate = 0x01;
                                }else{
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+4]);
                                    ui[i_k].Gate = 0x00;
                                }
                                t = t+5;
                        }
                        //Master
                        for (i_k = UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca; i_k < UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca+UIMaster; ++i_k) {
                                ui[i_k].vuPost = (int)dec[t];
                                ui[i_k].vuPostFader = (int)dec[t+1];
                                ui[i_k].vuGateIn = (int)dec[t+2];
                                ui[i_k].vuCompOut = (int)dec[t+3];
                                if((int)dec[t+4] & 0x80){
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+4]^0x80);
                                    ui[i_k].Gate = 0x01;
                                }else{
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+4]);
                                    ui[i_k].Gate = 0x00;
                                }
                                t = t+5;
                        }
                        //Line
                        for (i_k = UIChannel; i_k < UIChannel+UILineIn; ++i_k) {
                                ui[i_k].vuPre = (int)dec[t];
                                ui[i_k].vuPost = (int)dec[t+1];
                                ui[i_k].vuPostFader = (int)dec[t+2];
                                ui[i_k].vuGateIn = (int)dec[t+3];
                                ui[i_k].vuCompOut = (int)dec[t+4];
                                if((int)dec[t+5] & 0x80){
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+5]^0x80);
                                    ui[i_k].Gate = 0x01;
                                }else{
                                    ui[i_k].vuCompMeter = 0x7F-((int)dec[t+5]);
                                    ui[i_k].Gate = 0x00;
                                }
                                t = t+6;
                        }
                        free(dec);

                        /*  Refresh Vu Meter only 800ms  */
                        i_VuMeter = currentTimeMillis();
                        if (i_VuMeter >= (i_VuMeter_init + 250)){
                            // Channel
                            for (Canal = 0; Canal < UIAllStrip-UIVca-UIMaster; ++Canal) {
                                if(ui[Canal].vuPostFader > 0){
                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        //printf("vuPostFader (%i) %02X\n", Canal, ui[Canal].vuPostFader);
                                        char MidiArray[2] = {0xD0+Canal-(NbMidiFader*AddrMidiTrack), (int)floor((double)127/(double)255*ui[Canal].vuPostFader)};
                                        SendMidiOut(midiout, MidiArray);
                                    }
                                }
                                if(ui[Canal].vuPostFaderL > 0 || ui[Canal].vuPostFaderR > 0){
                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        //printf("vuPostFaderL R (%i) %02X %02X %02X\n", Canal, ui[Canal].vuPostFaderL, ui[Canal].vuPostFaderR, (ui[Canal].vuPostFaderL+ui[Canal].vuPostFaderR)/2);
                                        char MidiArray[2] = {0xD0+Canal-(NbMidiFader*AddrMidiTrack), (int)floor((double)127/(double)255*(ui[Canal].vuPostFaderL+ui[Canal].vuPostFaderR)/2)};
                                        SendMidiOut(midiout, MidiArray);
                                    }
                                }
                            }
                            for (Canal = UIAllStrip-UIMaster; Canal < UIAllStrip; ++Canal) {
                                if(ui[Canal].vuPostFader > 0){
                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        //printf("vuPostFader (%i) %02X\n", Canal, ui[Canal].vuPostFader);
                                        char MidiArray[2] = {0xD0+Canal-(NbMidiFader*AddrMidiTrack), (int)floor((double)127/(double)255*ui[Canal].vuPostFader)};
                                        SendMidiOut(midiout, MidiArray);
                                    }
                                }
                            }
                            if( ModeMasterPressed ==1 ){
                                        char MidiArray[2] = {0xD0+NbMidiFader, (int)floor((double)127/(double)255*ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].vuPostFader)};
                                        SendMidiOut(midiout, MidiArray);
                            }
                            i_VuMeter_init = currentTimeMillis();
                            //printf("\n");
                        }
                    }
                    if(strstr(UIMessage, "currentShow^")){
                        //SETS^var.currentShow^Studio Stephan

                        UICommand = strtok(strstr(UIMessage,"currentShow^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : Current Show : %s\n", UICommand);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        SplitArrayComa=split(UICommand,"^",1);
                        strcat(ShowsCurrent, SplitArrayComa[1]);
                        free(SplitArrayComa);

                        char CmdSnapShot[256];
                        sprintf(CmdSnapShot,"SNAPSHOTLIST^%s\n", ShowsCurrent);

                        sprintf(sa_LogMessage, "UI2MCP  -->  UI : Send command : %s\n", CmdSnapShot);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);
                        send(sock , CmdSnapShot, strlen(CmdSnapShot) , 0 );

                    }
                    if(strstr(UIMessage, "currentSnapshot^")){
                        //3:::SETS^var.currentSnapshot^Snailz

                        UICommand = strtok(strstr(UIMessage,"currentSnapshot^"), "\r\n");
                        //printf("Recu [%s]\n",UICommand);

                        SplitArrayComa=split(UICommand,"^",1);
                        strcat(SnapShotCurrent, SplitArrayComa[1]);
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : Current SnapShot [%s]\n", SnapShotCurrent);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                    }
                    if(strstr(UIMessage, "currentCue^")){
                        //3:::SETS^var.currentCue^Snailz

                        UICommand = strtok(strstr(UIMessage,"currentCue^"), "\r\n");
                        //printf("Recu [%s]\n",UICommand);

                        SplitArrayComa=split(UICommand,"^",1);
                        strcat(CuesCurrent, SplitArrayComa[1]);
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : Current Cue [%s]\n", CuesCurrent);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                    }
                    if(strstr(UIMessage, "PLISTS^")){

                        UICommand = strtok(strstr(UIMessage,"PLISTS^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : DirPlayerList [%s]\n", UICommand);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        memset(UIDirPlayerList, 0, sizeof UIDirPlayerList);

                        SplitArrayComa=split(UICommand,"^",1);
                        //affichage du resultat
                        for(AdrSplitComa=0; SplitArrayComa[AdrSplitComa]!=NULL; AdrSplitComa++){
                            if (AdrSplitComa >= 1){
                                //printf("i=%d : [%s]\n",AdrSplitComa-1,SplitArrayComa[AdrSplitComa]);
                                strcpy(UIDirPlayerList[AdrSplitComa-1],SplitArrayComa[AdrSplitComa]);
                                DirPlayerMax = AdrSplitComa-1;
                            }
                        }
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : DirPlayerIndex [%i]\n", DirPlayerIndex);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        if ( Lcd == 1 && ModeDirPlayerPressed == 1 ){
                            ConfigLCDTxtMode();
                            LcdExplorerUpdate(midiout, SysExHdr, DirPlayerIndex, &UIDirPlayerList);
                        }

                    }
                    if(strstr(UIMessage, "PLIST_TRACKS^")){

                        UICommand = strtok(strstr(UIMessage,"PLIST_TRACKS^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : FilesPlayerList [%s]\n", UICommand);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        memset(UIFilesPlayerList, 0, sizeof UIFilesPlayerList);

                        SplitArrayComa=split(UICommand,"^",1);
                        //affichage du resultat
                        for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
                            if (AdrSplitComa >= 2){
                                //printf("i=%d : [%s]\n",AdrSplitComa-2,SplitArrayComa[AdrSplitComa]);
                                strcpy(UIFilesPlayerList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
                                FilesPlayerMax = AdrSplitComa-2;
                            }
                        }
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : FilesPlayerIndex [%i]\n", FilesPlayerIndex);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        if( ModeFilesPlayerPressed == 1 ){
                            if ( Lcd == 1 ){
                                ConfigLCDTxtMode();
                                LcdExplorerUpdate(midiout, SysExHdr, FilesPlayerIndex, &UIFilesPlayerList);
                            }
                        }
                    }
                    if(strstr(UIMessage, "MTK_GET_SESSIONS^")){

                        UICommand = strtok(strstr(UIMessage,"MTK_GET_SESSIONS^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : MTK Session List [%s]\n", UICommand);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        memset(UIMtkSessionList, 0, sizeof UIMtkSessionList);

                        SplitArrayComa=split(UICommand,"^",1);
                        //affichage du resultat
                        for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
                            if (AdrSplitComa >= 2){
                                //printf("i=%d : [%s]\n",AdrSplitComa-2,SplitArrayComa[AdrSplitComa]);
                                strcpy(UIMtkSessionList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
                                MtkSessionMax = AdrSplitComa-2;
                            }
                        }
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : MTK Session Index [%i]\n", MtkSessionIndex);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        if( ModeMtkSessionsPressed == 1 ){
                            if ( Lcd == 1 ){
                                ConfigLCDTxtMode();
                                LcdExplorerUpdate(midiout, SysExHdr, MtkSessionIndex, &UIMtkSessionList);
                            }
                        }
                    }
                    if(strstr(UIMessage, "SHOWLIST^")){

                        UICommand = strtok(strstr(UIMessage,"SHOWLIST^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : ShowsList [%s]\n", UICommand);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        memset(UIShowsList, 0, sizeof UIShowsList);

                        SplitArrayComa=split(UICommand,"^",1);
                        //affichage du resultat
                        for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
                            if (AdrSplitComa >= 2){
                                //printf("i=%d : [%s] current [%s]\n",AdrSplitComa-2,SplitArrayComa[AdrSplitComa], SnapShotCurrent);
                                strcpy(UIShowsList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
                                ShowsMax = AdrSplitComa-2;

                                if(strcmp(SplitArrayComa[AdrSplitComa], ShowsCurrent) == 0){
                                    ShowsIndex = AdrSplitComa-2;
                                }
                            }
                        }
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : ShowsIndex [%i]\n",ShowsIndex);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                    }
                    if(strstr(UIMessage, "SNAPSHOTLIST^")){

                        UICommand = strtok(strstr(UIMessage,"SNAPSHOTLIST^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : SnapShotList [%s]\n", UICommand);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        memset(UISnapShotList, 0, sizeof UISnapShotList);

                        SplitArrayComa=split(UICommand,"^",1);
                        //affichage du resultat
                        for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
                            if (AdrSplitComa >= 2){
                                //printf("i=%d : [%s] current [%s]\n",AdrSplitComa-2,SplitArrayComa[AdrSplitComa], SnapShotCurrent);
                                strcpy(UISnapShotList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
                                SnapShotMax = AdrSplitComa-2;

                                if(strcmp(SplitArrayComa[AdrSplitComa], SnapShotCurrent) == 0){
                                    if( ModeSnapShotsPressed != 1 ){ SnapShotIndex = AdrSplitComa-2; }
                                }
                            }
                        }
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : SnapShotIndex [%i]\n",SnapShotIndex);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                    }
                    if(strstr(UIMessage, "CUELIST")){

                        UICommand = strtok(strstr(UIMessage,"CUELIST^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : CuesList [%s]\n", UICommand);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        memset(UICuesList, 0, sizeof UICuesList);

                        SplitArrayComa=split(UICommand,"^",1);
                        //affichage du resultat
                        for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
                            if (AdrSplitComa >= 2){
                                //printf("i=%d : [%s] current [%s]\n",AdrSplitComa-2,SplitArrayComa[AdrSplitComa], CuesCurrent);
                                strcpy(UICuesList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
                                CuesMax = AdrSplitComa-2;

                                if(strcmp(SplitArrayComa[AdrSplitComa], CuesCurrent) == 0){
                                    CuesIndex = AdrSplitComa-2;
                                    if( ModeCuesPressed != 1 ){ CuesIndex = AdrSplitComa-2; }
                                }
                            }
                        }
                        free(SplitArrayComa);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : CuesIndex [%i]\n",CuesIndex);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                    }
                    if(strstr(UIMessage, "MODEL^")){
                        UIModel = strtok(strstr(UIMessage,"MODEL^"), "\r\n");

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : Soundcraft Model [%s]\n",UIModel);
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        if(strstr(UIModel,"ui12")){
                            UIChannel = 12;
                        }else if (strstr(UIModel,"ui16")){
                            UIChannel = 16;
                        }else if (strstr(UIModel,"ui24")){
                            UIChannel = 24;
                        }else{
                            printf("Sorry not supported.\n");
                        }
                    }
                    if(strstr(UIMessage, "VERSION^")){
                        UICommand = strtok(strstr(UIMessage,"VERSION^"), "\r\n");

                        SplitArrayComa=split(UICommand,"^",1);
                        strcat(UIFirmware, SplitArrayComa[1]);
                        free(SplitArrayComa);

                        if(strstr(UIFirmware,"2.0.7548-ui24")){
                            sprintf(sa_LogMessage, "UI2MCP  <--  UI : Soundcraft Firmware [%s]\n",UIFirmware);
                            LogTrace(hfErr, 1, debug, sa_LogMessage);
                        }else if (strstr(UIFirmware,"2.0.7852-ui24")){
                            sprintf(sa_LogMessage, "UI2MCP  <--  UI : Soundcraft Firmware [%s]\n",UIFirmware);
                            LogTrace(hfErr, 1, debug, sa_LogMessage);
                        }
                    }
                    if(strstr(UIMessage, "SETD^") || strstr(UIMessage, "SETS^")){

                        /* split UI message by '^' and '.' the websocket message  */

                        memset(UIchan,0,strlen(UIchan));
                        memset(UIfunc,0,strlen(UIfunc));
                        memset(UIsfunc,0,strlen(UIsfunc));
                        memset(UIval,0,strlen(UIval));

                        SplitArrayComa=split(UIMessage,"^",1);
                        for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
                            if(AdrSplitComa==1){
                                SplitArrayDot=split(SplitArrayComa[AdrSplitComa],".",1);
                                for(ArraySplitDot=0;SplitArrayDot[ArraySplitDot]!=NULL;ArraySplitDot++){
                                    if (ArraySplitDot == 0){ sprintf(UIio,"%s",SplitArrayDot[ArraySplitDot]); }
                                    if (ArraySplitDot == 1){ sprintf(UIchan,"%s",SplitArrayDot[ArraySplitDot]);}
                                    if (ArraySplitDot == 2){ sprintf(UIfunc,"%s",SplitArrayDot[ArraySplitDot]);}
                                    if (ArraySplitDot == 3){ sprintf(UIsfunc,"%s",SplitArrayDot[ArraySplitDot]);}
                                    free(SplitArrayDot[ArraySplitDot]);
                                }
                            }else if(AdrSplitComa!=1){
                                if (AdrSplitComa == 0){ sprintf(UImsg,"%s",SplitArrayComa[AdrSplitComa]);}
                                if (AdrSplitComa == 2){ sprintf(UIval,"%s",SplitArrayComa[AdrSplitComa]);}
                                free(SplitArrayComa[AdrSplitComa]);
                            }
                        }
                        free(SplitArrayComa);
                        free(SplitArrayDot);

                        if( strcmp(UIio,"var") == 0 ){

//                            if( strcmp(UIchan,"currentShow") == 0 ){
//                                strcpy(ShowsCurrent, UIval);
//                                printf("currentShow %s\n", ShowsCurrent);
//                            }
//
//                            if( strcmp(UIchan,"currentSnapshot") == 0 ){
//                                strcpy(SnapShotCurrent, UIval);
//                                printf("currentSnapshot %s\n", SnapShotCurrent);
//                            }
//
                            if( strcmp(UIchan,"currentCue") == 0 ){
                                strcpy(CuesCurrent, UIval);
                                printf("currentCue %s\n", CuesCurrent);
                            }

                            if( strcmp(UIchan,"isRecording") == 0 ){
                                int Recording = atoi(UIval);

                                printf("State Media isRecording %i\n", MediaPlayerCurrentState);
                                if ( Recording == 1 && ModePlayer == 0 ){
                                    MediaRecCurrentState = Recording;

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x7F};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"currentState") == 0 ){
                                MediaPlayerCurrentState = atoi(UIval);

                                printf("State Media %i\n", MediaPlayerCurrentState);
//                                if ( MediaPlayerCurrentState == 0 && ModePlayer == 0 ){
                                if ( MediaPlayerCurrentState == 0 && MediaRecCurrentState ==0 ){
                                    ModePlayer = 0;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
//                                else if ( MediaPlayerCurrentState == 1 && ModePlayer == 0 ){
                                else if ( MediaPlayerCurrentState == 1 && MediaRecCurrentState ==0 ){
                                    //MediaRecCurrentState = MediaPlayerCurrentState;

                                    //char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    //SendMidiOut(midiout, MidiArrayStop);
                                    //char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    //SendMidiOut(midiout, MidiArrayPlay);
                                    //char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x7F};
                                    //SendMidiOut(midiout, MidiArrayRec);
                                }
//                                else if ( MediaPlayerCurrentState == 2 && ModePlayer == 0 ){
                                else if ( MediaPlayerCurrentState == 2 && MediaRecCurrentState ==0 ){
                                    ModePlayer = 0;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
//                                else if ( MediaPlayerCurrentState == 3 && ModePlayer == 0 ){
                                else if ( MediaPlayerCurrentState == 3 && MediaRecCurrentState ==0 ){
                                    ModePlayer = 0;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x01};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"recBusy") == 0 ){
                                MediaRecCurrentState = atoi(UIval);

                                printf("Debug recBusy %i %i\n", MediaRecCurrentState, ModePlayer);

                                if ( MediaRecCurrentState == 1 ){
                                    ModePlayer = 0;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x7F};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                                else if ( MediaRecCurrentState == 0 ){
                                    ModePlayer = 0;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"usbfill") == 0 ){
                                float UsbFill;
                                UsbFill = atof(UIval);

                                printf("Debug UsbFill %f %i %i\n", UsbFill, MediaPlayerCurrentState, ModePlayer);

                                if ( UsbFill > 0 ){
                                    ModePlayer = 0;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x7F};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"currentTrackPos") == 0 ){
                                float TrackPos;
                                TrackPos = atof(UIval);

                                printf("Debug TrackPos %f %i %i\n", TrackPos, MediaPlayerCurrentState, ModePlayer);

                                if ( TrackPos > 0 ){
                                    ModePlayer = 0;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"mtk") == 0 && strcmp(UIfunc,"currentState") == 0 ){
                                MtkCurrentState = atoi(UIval);

                                printf("Debug currentState %i %i\n", MtkCurrentState, ModePlayer);

                                if ( MtkCurrentState == 0  && ModePlayer == 1 ){
                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                }
                                else if ( MtkCurrentState == 1&& ModePlayer == 1 ){

                                    printf("Debug Pause currentState %i %i\n", MtkCurrentState, ModePlayer);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x01};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                }
                                else if ( MtkCurrentState == 2 && ModePlayer == 1 ){
                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                }
//                            }
//
//                                MtkRecCurrentState = atoi(UIval);
//                                if ( MtkRecCurrentState == 0 && ModePlayer == 1 ){
//                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x7F};
//                                    SendMidiOut(midiout, MidiArrayStop);
//                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
//                                    SendMidiOut(midiout, MidiArrayPlay);
//                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
//                                    SendMidiOut(midiout, MidiArrayRec);
//                                }
//                                else if ( MtkRecCurrentState == 1 && ModePlayer == 1 ){
//                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
//                                    SendMidiOut(midiout, MidiArrayStop);
//                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x01};
//                                    SendMidiOut(midiout, MidiArrayPlay);
//                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
//                                    SendMidiOut(midiout, MidiArrayRec);
//                                }
//                                else if( MtkRecCurrentState == 2 && ModePlayer == 0 ){
//                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
//                                    SendMidiOut(midiout, MidiArrayStop);
//                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
//                                    SendMidiOut(midiout, MidiArrayPlay);
//                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
//                                    SendMidiOut(midiout, MidiArrayRec);
//                                }
                            }

                            if( strcmp(UIchan,"mtk") == 0 && strcmp(UIfunc,"rec") == 0 && strcmp(UIsfunc,"busy") == 0 ){

                                MtkRecCurrentState = atoi(UIval);

                                if ( MtkRecCurrentState == 0 ){
                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"mtk") == 0 && strcmp(UIfunc,"bufferfill") == 0 ){
                                float MTKBufferFill;
                                MTKBufferFill = atof(UIval);

                                printf("Debug BufferFill %f %i %i\n", MTKBufferFill, MtkCurrentState, ModePlayer);

                                if ( MTKBufferFill > 0 ){
                                    ModePlayer = 1;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x7F};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"mtk") == 0 && strcmp(UIfunc,"currentTrackPos") == 0 ){
                                float MTKTrackPos;
                                MTKTrackPos = atof(UIval);

                                printf("Debug TrackPos %f %i %i\n", MTKTrackPos, MtkCurrentState, ModePlayer);

                                if ( MTKTrackPos > 0 && MtkCurrentState == 2 ){
                                    ModePlayer = 1;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                                else if ( MTKTrackPos == 0 ){
                                    ModePlayer = 1;

                                    sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);

                                    char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayPlayer);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayB);

                                    char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                                    SendMidiOut(midiout, MidiArrayStop);
                                    char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x00};
                                    SendMidiOut(midiout, MidiArrayPlay);
                                    char MidiArrayRec[3] = {AddrMidiButtonLed, IdRec, 0x00};
                                    SendMidiOut(midiout, MidiArrayRec);
                                }
                            }

                            if( strcmp(UIchan,"mtk") == 0 && strcmp(UIfunc,"soundcheck") == 0  ){
                                SoundCheck = atoi(UIval);
                                if ( SoundCheck == 0 ){
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrSoundCheck, 0x00};
                                    SendMidiOut(midiout, MidiArray);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrSoundCheck, 0x00};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrSoundCheck, 0x7F};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrSoundCheck, 0x00};
                                    SendMidiOut(midiout, MidiArrayB);

                                    ModePlayer = 0;

                                    char MidiArrayM[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayM);
                                    char MidiArrayMR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayMR);
                                    char MidiArrayMG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                                    SendMidiOut(midiout, MidiArrayMG);
                                    char MidiArrayMB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                                    SendMidiOut(midiout, MidiArrayMB);

                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : Sound Check Desactived\n");
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);
                                }
                                else if ( SoundCheck == 1 ){
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrSoundCheck, 0x7F};
                                    SendMidiOut(midiout, MidiArray);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrSoundCheck, 0x00};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrSoundCheck, 0x7F};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrSoundCheck, 0x00};
                                    SendMidiOut(midiout, MidiArrayB);

                                    ModePlayer = 1;

                                    char MidiArrayM[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayM);
                                    char MidiArrayMR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayMR);
                                    char MidiArrayMG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x7F};
                                    SendMidiOut(midiout, MidiArrayMG);
                                    char MidiArrayMB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x00};
                                    SendMidiOut(midiout, MidiArrayMB);

                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : Sound Check Actived\n");
                                    LogTrace(hfErr, 1, debug, sa_LogMessage);
                                }
                            }

                            sprintf(sa_LogMessage, "UI2MCP <-> Variable = %s %s %s %s\n", UIchan, UIfunc, UIsfunc, UIval);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                        }
                        else if( strcmp(UIio,"settings") == 0 ){
                            sprintf(sa_LogMessage, "UI2MCP <-> Setting = %s %s %s %s\n", UIchan, UIfunc, UIsfunc, UIval);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                            if( strcmp(UIchan,"multiplesolo") == 0 ){
                                SoloMode  = atoi(UIval);

                                sprintf(sa_LogMessage, "UI2MCP  <--  UI : Setting SoloMode = %i\n",  SoloMode);
                                LogTrace(hfErr, 2, debug, sa_LogMessage);
                            }
                        }
                        else if( strcmp(UIfunc,"bpm") == 0 ){
                            UIBpm = atof(UIval);

                            sprintf(sa_LogMessage, "UI2MCP  <--  UI : UIbpm = %f\n", atof(UIval));
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                        }
                        else if( strcmp(UIio,"m") == 0 && strcmp(UIchan,"dim") == 0 ){

                            int v = 0;
                            v = atoi(UIval);
                            if (v == 0){
                                DimMaster = 0;

                                char MidiArray[3] = {AddrMidiButtonLed, i_Dim, 0x00};
                                SendMidiOut(midiout, MidiArray);

                                char MidiArrayR[3] = {AddrMidiButtonLed+1, i_Dim, (int)floor((double)127/(double)255*0)};
                                SendMidiOut(midiout, MidiArrayR);
                                char MidiArrayG[3] = {AddrMidiButtonLed+2, i_Dim, (int)floor((double)127/(double)255*145)};
                                SendMidiOut(midiout, MidiArrayG);
                                char MidiArrayB[3] = {AddrMidiButtonLed+3, i_Dim, (int)floor((double)127/(double)255*194)};
                                SendMidiOut(midiout, MidiArrayB);
                            }
                            else if (v == 1){
                                DimMaster = 1;

                                char MidiArray[3] = {AddrMidiButtonLed, i_Dim, 0x7F};
                                SendMidiOut(midiout, MidiArray);

                                char MidiArrayR[3] = {AddrMidiButtonLed+1, i_Dim, (int)floor((double)127/(double)255*0)};
                                SendMidiOut(midiout, MidiArrayR);
                                char MidiArrayG[3] = {AddrMidiButtonLed+2, i_Dim, (int)floor((double)127/(double)255*145)};
                                SendMidiOut(midiout, MidiArrayG);
                                char MidiArrayB[3] = {AddrMidiButtonLed+3, i_Dim, (int)floor((double)127/(double)255*194)};
                                SendMidiOut(midiout, MidiArrayB);
                            }
                        }
                        else if( strcmp(UIio,"m") == 0 && strcmp(UIchan,"mix") == 0 ){

                            Canal = UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca;

                            ui[Canal].MixMidi = atof(UIval);
                            ui[Canal+1].MixMidi = atof(UIval);

                            int MidiValue = 0;
                            //float unit = 0.0078740157480315;
                            //MidiValue = (127 * MixMidi[Canal]);
                            MidiValue = (127 * ui[Canal].MixMidi);

                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : Fader %i: %f %i\n", Canal, ui[Canal].MixMidi, MidiValue);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1 ){
                                    char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                    SendMidiOut(midiout, MidiArrayLeft);
                                    char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                    SendMidiOut(midiout, MidiArrayRight);
                            }
                            if( ModeMasterPressed == 1 ){
                                char MidiArray[3] = {AddrMidiMix+NbMidiFader, MidiValue, MidiValue};
                                SendMidiOut(midiout, MidiArray);
                            }
                        }
                        else if( strcmp(UIio,"m") == 0 && strcmp(UIchan,"pan") == 0 ){

                            Canal = UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca;

                            ui[Canal].PanMidi = atof(UIval);
                            ui[Canal+1].PanMidi = atof(UIval);

                            if( ModeMasterPressed == 0 ){
                                if( atof(UIval) == 0.5 ){

                                    int MidiValue = 0;
//                                    if( ui[Canal].MixMidi > ui[Canal+1].MixMidi ){
//                                        ui[Canal+1].MixMidi=ui[Canal].MixMidi;
//                                    }
//                                    else if( ui[Canal].MixMidi < ui[Canal+1].MixMidi ){
//                                        ui[Canal].MixMidi=ui[Canal+1].MixMidi;
//                                    }

                                    MidiValue = (127 * ui[Canal].MixMidi);

                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                        SendMidiOut(midiout, MidiArrayLeft);
                                        char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                        SendMidiOut(midiout, MidiArrayRight);
                                    }
                                }
                                else if( atof(UIval) < 0.5 ){
                                    int MidiValueR = 0;
                                    MidiValueR = (127 * (ui[Canal].MixMidi-ui[Canal].MixMidi*(-200*ui[Canal+1].PanMidi+100)/100));
                                    //ui[Canal+1].MixMidi = (ui[Canal].MixMidi-ui[Canal].MixMidi*(-200*ui[Canal+1].PanMidi+100)/100);
                                    //MidiValueR = (127 * ui[Canal+1].MixMidi);

                                    int MidiValueL = 0;
                                    MidiValueL = (127 * ui[Canal].MixMidi);

                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueL, MidiValueL};
                                        SendMidiOut(midiout, MidiArrayLeft);

                                        char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValueR, MidiValueR};
                                        SendMidiOut(midiout, MidiArrayRight);
                                    }
                                }
                                else if( atof(UIval) > 0.5 ){
                                    int MidiValueL = 0;
                                    MidiValueL = (127 * (ui[Canal+1].MixMidi-ui[Canal+1].MixMidi*(200*ui[Canal].PanMidi-100)/100));
                                    //ui[Canal].MixMidi = (ui[Canal+1].MixMidi-ui[Canal+1].MixMidi*(200*ui[Canal].PanMidi-100)/100);
                                    //MidiValueL = (127 * ui[Canal].MixMidi);

                                    int MidiValueR = 0;
                                    MidiValueR = (127 * ui[Canal+1].MixMidi);

                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueL, MidiValueL};
                                        SendMidiOut(midiout, MidiArrayLeft);

                                        char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValueR, MidiValueR};
                                        SendMidiOut(midiout, MidiArrayRight);
                                    }
                                }

                                if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){

                                    if ( Lcd == 1 ){
                                        char BarL[3] = {0xB0, 0x30+Canal-(NbMidiFader*AddrMidiTrack), ui[Canal].PanMidi*0x7F};
                                        SendMidiOut(midiout, BarL);
                                        char BarR[3] = {0xB0, 0x30+Canal+1-(NbMidiFader*AddrMidiTrack), ui[Canal+1].PanMidi*0x7F};
                                        SendMidiOut(midiout, BarR);

                                        char CmdL[32] = "";
                                        char c_CanalL[32] = "";
                                        char CmdR[32] = "";
                                        char c_CanalR[32] = "";
                                        char c_CanalText[32] = "";

                                        strcpy(CmdL, "12");
                                        sprintf(c_CanalL, "%02X", Canal-(NbMidiFader*AddrMidiTrack));
                                        strcat(CmdL, c_CanalL);
                                        strcat(CmdL, "0200");

                                        strcpy(CmdR, "12");
                                        sprintf(c_CanalR, "%02X", Canal-(NbMidiFader*AddrMidiTrack)+1);
                                        strcat(CmdR, c_CanalR);
                                        strcat(CmdR, "0200");

                                        if( ui[Canal].PanMidi == 0.5 ){
                                            sprintf(c_CanalText, "<C>");
                                        }
                                        else if( ui[Canal].PanMidi < 0.5 ){
                                            sprintf(c_CanalText, "L%i", (int)(-200*ui[Canal].PanMidi+100));
                                        }
                                        else if( ui[Canal].PanMidi > 0.5 ){
                                            sprintf(c_CanalText, "R%i", (int)(200*ui[Canal].PanMidi-100));
                                        }

                                        SendSysExTextOut(midiout, SysExHdr, CmdL, c_CanalText);
                                        SendSysExTextOut(midiout, SysExHdr, CmdR, c_CanalText);
                                    }
                                }

                                sprintf(sa_LogMessage, "UI2MCP <-> MEM : Pan %i: %f\n", Canal, ui[Canal].PanMidi);
                                LogTrace(hfErr, 2, debug, sa_LogMessage);
                            }
                            else if( ModeMasterPressed == 1 ){

                                if ( Lcd == 1 ){
                                    char Bar[3] = {0xB0, 0x30+NbMidiFader, ui[Canal].PanMidi*0x7F};
                                    SendMidiOut(midiout, Bar);

                                    char Cmd[32] = "";
                                    char c_Canal[32] = "";
                                    char c_CanalText[32] = "";

                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", NbMidiFader);
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0200");


                                    if( ui[Canal].PanMidi == 0.5 ){
                                        sprintf(c_CanalText, "<C>");
                                    }
                                    else if( ui[Canal].PanMidi < 0.5 ){
                                        sprintf(c_CanalText, "L%i", (int)(-200*ui[Canal].PanMidi+100));
                                    }
                                    else if( ui[Canal].PanMidi > 0.5 ){
                                        sprintf(c_CanalText, "R%i", (int)(200*ui[Canal].PanMidi-100));
                                    }
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                                }

                                if( atof(UIval) == 0.5 ){

                                    int MidiValue = 0;
//                                    if( ui[Canal].MixMidi > ui[Canal+1].MixMidi ){
//                                        ui[Canal+1].MixMidi=ui[Canal].MixMidi;
//                                    }
//                                    else if( ui[Canal].MixMidi < ui[Canal+1].MixMidi ){
//                                        ui[Canal].MixMidi=ui[Canal+1].MixMidi;
//                                    }

                                    MidiValue = (127 * ui[Canal].MixMidi);

                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                        SendMidiOut(midiout, MidiArrayLeft);
                                        char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                        SendMidiOut(midiout, MidiArrayRight);
                                    }
                                }
                                else if( atof(UIval) < 0.5 ){
                                    int MidiValueR = 0;
                                    MidiValueR = (127 * (ui[Canal].MixMidi-ui[Canal].MixMidi*(-200*ui[Canal+1].PanMidi+100)/100));
                                    //ui[Canal+1].MixMidi = (ui[Canal].MixMidi-ui[Canal].MixMidi*(-200*ui[Canal+1].PanMidi+100)/100);
                                    //MidiValueR = (127 * ui[Canal+1].MixMidi);

                                    int MidiValueL = 0;
                                    MidiValueL = (127 * ui[Canal].MixMidi);

                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueL, MidiValueL};
                                        SendMidiOut(midiout, MidiArrayLeft);

                                        char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValueR, MidiValueR};
                                        SendMidiOut(midiout, MidiArrayRight);
                                    }
                                }
                                else if( atof(UIval) > 0.5 ){
                                    int MidiValueL = 0;
                                    MidiValueL = (127 * (ui[Canal+1].MixMidi-ui[Canal+1].MixMidi*(200*ui[Canal].PanMidi-100)/100));
                                    //ui[Canal].MixMidi = (ui[Canal+1].MixMidi-ui[Canal+1].MixMidi*(200*ui[Canal].PanMidi-100)/100);
                                    //MidiValueL = (127 * ui[Canal].MixMidi);

                                    int MidiValueR = 0;
                                    MidiValueR = (127 * ui[Canal+1].MixMidi);

                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueL, MidiValueL};
                                        SendMidiOut(midiout, MidiArrayLeft);

                                        char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValueR, MidiValueR};
                                        SendMidiOut(midiout, MidiArrayRight);
                                    }
                                }

                                if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){

                                    if ( Lcd == 1 ){
                                        char BarL[3] = {0xB0, 0x30+Canal-(NbMidiFader*AddrMidiTrack), ui[Canal].PanMidi*0x7F};
                                        SendMidiOut(midiout, BarL);
                                        char BarR[3] = {0xB0, 0x30+Canal+1-(NbMidiFader*AddrMidiTrack), ui[Canal+1].PanMidi*0x7F};
                                        SendMidiOut(midiout, BarR);

                                        char CmdL[32] = "";
                                        char c_CanalL[32] = "";
                                        char CmdR[32] = "";
                                        char c_CanalR[32] = "";
                                        char c_CanalText[32] = "";

                                        strcpy(CmdL, "12");
                                        sprintf(c_CanalL, "%02X", Canal-(NbMidiFader*AddrMidiTrack));
                                        strcat(CmdL, c_CanalL);
                                        strcat(CmdL, "0200");

                                        strcpy(CmdR, "12");
                                        sprintf(c_CanalR, "%02X", Canal-(NbMidiFader*AddrMidiTrack)+1);
                                        strcat(CmdR, c_CanalR);
                                        strcat(CmdR, "0200");

                                        if( ui[Canal].PanMidi == 0.5 ){
                                            sprintf(c_CanalText, "<C>");
                                        }
                                        else if( ui[Canal].PanMidi < 0.5 ){
                                            sprintf(c_CanalText, "L%i", (int)(-200*ui[Canal].PanMidi+100));
                                        }
                                        else if( ui[Canal].PanMidi > 0.5 ){
                                            sprintf(c_CanalText, "R%i", (int)(200*ui[Canal].PanMidi-100));
                                        }

                                        SendSysExTextOut(midiout, SysExHdr, CmdL, c_CanalText);
                                        SendSysExTextOut(midiout, SysExHdr, CmdR, c_CanalText);
                                    }
                                }

                                sprintf(sa_LogMessage, "UI2MCP <-> MEM : Pan %i: %f\n", Canal, ui[Canal].PanMidi);
                                LogTrace(hfErr, 0, debug, sa_LogMessage);
                            }
                        }
                        else if( strcmp(UIio,"mgmask") == 0 || strcmp(UIfunc,"mgmask") == 0){

                            if( strcmp(UIio,"mgmask") == 0 )
                            {

                                *UIMuteMask = '\0';
                                strcat(UIMuteMask, UIval);

                                sprintf(sa_LogMessage,"UI2MCP <-- UI : %s : Mute Mask : %i\n", UIMessage, atoi(UIMuteMask));
                                LogTrace(hfErr, 2, debug, sa_LogMessage);

                                unsigned bit;
                                for (bit = 0; bit < 6; bit++)
                                {
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : bit %i : %i\n", bit, atoi(UIMuteMask) & (1u << bit));
                                    LogTrace(hfErr, 2, debug, sa_LogMessage);
                                    GroupMaskMute[bit]= atoi(UIMuteMask) & (1u << bit);
                                }

                                GroupMaskMuteAll = atoi(UIMuteMask) & (1u << 23);
                                GroupMaskMuteFx = atoi(UIMuteMask) & (1u << 22);

                                for (Canal = 0; Canal < UIAllStrip; Canal++){
                                    if( GroupMaskMuteAll ){
                                        ui[Canal].MaskMute = 1;
                                    }
                                    else if( (GroupMaskMute[0] == (ui[Canal].MaskMuteValue & (1u << 0)) && (ui[Canal].MaskMuteValue & (1u << 0)) != 0) ||
                                        (GroupMaskMute[1] == (ui[Canal].MaskMuteValue & (1u << 1)) && (ui[Canal].MaskMuteValue & (1u << 1)) != 0) ||
                                        (GroupMaskMute[2] == (ui[Canal].MaskMuteValue & (1u << 2)) && (ui[Canal].MaskMuteValue & (1u << 2)) != 0) ||
                                        (GroupMaskMute[3] == (ui[Canal].MaskMuteValue & (1u << 3)) && (ui[Canal].MaskMuteValue & (1u << 3)) != 0) ||
                                        (GroupMaskMute[4] == (ui[Canal].MaskMuteValue & (1u << 4)) && (ui[Canal].MaskMuteValue & (1u << 4)) != 0) ||
                                        (GroupMaskMute[5] == (ui[Canal].MaskMuteValue & (1u << 5)) && (ui[Canal].MaskMuteValue & (1u << 5)) != 0)){

                                        ui[Canal].MaskMute = 1;
                                    }
                                    else{
                                        ui[Canal].MaskMute = 0;
                                    }

                                    if( GroupMaskMuteFx ){
                                        if(strcmp(ui[Canal].Type, "f") == 0){
                                            ui[Canal].MaskMute = 1;
                                        }
                                    }

                                    int d = 0x7F;
                                    int i_OrMute = (ui[Canal].MaskMute | ui[Canal].Mute) & ( ! (ui[Canal].ForceUnMute));
                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        sprintf(sa_LogMessage,"UI2MCP <-- UI : mgmask : Update Light : Canal(%i) : Mute | MaskMute | ! ForceUnMute = %i * 0x7F\n", Canal, i_OrMute);
                                        LogTrace(hfErr, 2, debug, sa_LogMessage);
                                        char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMute*d};
                                        SendMidiOut(midiout, MidiArray);
                                    }
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : mgmask : Canal(%i) [Mute=%i][ForceUnMute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, ui[Canal].Mute, ui[Canal].ForceUnMute, ui[Canal].MaskMute, ui[Canal].MaskMuteValue);
                                    LogTrace(hfErr, 2, debug, sa_LogMessage);
                                }
                            }
                            if(((strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"f") == 0 || strcmp(UIio,"s") == 0 || strcmp(UIio,"a") == 0 || strcmp(UIio,"v") == 0)
                                        && strcmp(UIfunc,"mgmask") == 0)){

                                sprintf(sa_LogMessage,"UI2MCP <-- UI : %s\n", UIMessage);
                                LogTrace(hfErr, 2, debug, sa_LogMessage);

                                Canal = atoi(UIchan);
                                if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                                if( strcmp(UIio,"l") == 0){ Canal = Canal+UIChannel;}
                                if( strcmp(UIio,"p") == 0){ Canal = Canal+UIChannel+UILineIn;}
                                if( strcmp(UIio,"f") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia;}
                                if( strcmp(UIio,"s") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx;}
                                if( strcmp(UIio,"a") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup;}
                                if( strcmp(UIio,"v") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux;}

                                sprintf(sa_LogMessage,"UI2MCP <-- UI : Mgmask : Type IO: %s Channel: %i Function: %s Value: %i\n", UIio, atoi(UIchan),UIfunc,atoi(UIval));
                                LogTrace(hfErr, 2, debug, sa_LogMessage);

//                                if( debug == 2){
//                                    printf("(GroupMaskMute[0]=%i,Value=%i)\n", GroupMaskMute[0], atoi(UIval) & (1u << 0));
//                                    printf("(GroupMaskMute[1]=%i,Value=%i)\n", GroupMaskMute[1], atoi(UIval) & (1u << 1));
//                                    printf("(GroupMaskMute[2]=%i,Value=%i)\n", GroupMaskMute[2], atoi(UIval) & (1u << 2));
//                                    printf("(GroupMaskMute[3]=%i,Value=%i)\n", GroupMaskMute[3], atoi(UIval) & (1u << 3));
//                                    printf("(GroupMaskMute[4]=%i,Value=%i)\n", GroupMaskMute[4], atoi(UIval) & (1u << 4));
//                                    printf("(GroupMaskMute[5]=%i,Value=%i)\n", GroupMaskMute[5], atoi(UIval) & (1u << 5));
//                                }

                                if( (GroupMaskMute[0] == (atoi(UIval) & (1u << 0)) && (atoi(UIval) & (1u << 0)) != 0) ||
                                    (GroupMaskMute[1] == (atoi(UIval) & (1u << 1)) && (atoi(UIval) & (1u << 1)) != 0) ||
                                    (GroupMaskMute[2] == (atoi(UIval) & (1u << 2)) && (atoi(UIval) & (1u << 2)) != 0) ||
                                    (GroupMaskMute[3] == (atoi(UIval) & (1u << 3)) && (atoi(UIval) & (1u << 3)) != 0) ||
                                    (GroupMaskMute[4] == (atoi(UIval) & (1u << 4)) && (atoi(UIval) & (1u << 4)) != 0) ||
                                    (GroupMaskMute[5] == (atoi(UIval) & (1u << 5)) && (atoi(UIval) & (1u << 5)) != 0)){

                                    ui[Canal].MaskMute = 1;
                                    ui[Canal].MaskMuteValue = atoi(UIval);
                                }
                                else{
                                    ui[Canal].MaskMute = 0;
                                    ui[Canal].MaskMuteValue = atoi(UIval);
                                }

                                int d = 0x7F;
                                //int i_OrMute = (MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) & ( ! (MidiTable [Canal][ForceUnMute]));
                                int i_OrMute = (ui[Canal].MaskMute | ui[Canal].Mute) & ( ! (ui[Canal].ForceUnMute));
                                if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mgmask : Update Light : Canal(%i) : Mute | MaskMute | ! ForceUnMute =%i * 0x7F\n", Canal, i_OrMute);
                                    LogTrace(hfErr, 2, debug, sa_LogMessage);
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMute*d};
                                    SendMidiOut(midiout, MidiArray);
                                }
                                //sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mgmask : Canal(%i) [Mute=%i][ForceUnMute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, MidiTable [Canal][Mute], MidiTable [Canal][ForceUnMute], MidiTable [Canal][MaskMute], MidiTable [Canal][MaskMuteValue]);
                                sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mgmask : Canal(%i) [Mute=%i][ForceUnMute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, ui[Canal].Mute, ui[Canal].ForceUnMute, ui[Canal].MaskMute, ui[Canal].MaskMuteValue);
                                LogTrace(hfErr, 2, debug, sa_LogMessage);
                                //printf( "Canal %i AddrMute %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiMute, NbMidiFader, AddrMidiTrack, v);
                            }
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"f") == 0 || strcmp(UIio,"s") == 0 || strcmp(UIio,"a") == 0 || strcmp(UIio,"v") == 0)
                                        && (strcmp(UIfunc,"mute") == 0 || strcmp(UIfunc,"forceunmute") == 0)){

                            int d,v;

                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal+UIChannel;}
                            if( strcmp(UIio,"p") == 0){ Canal = Canal+UIChannel+UILineIn;}
                            if( strcmp(UIio,"f") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia;}
                            if( strcmp(UIio,"s") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx;}
                            if( strcmp(UIio,"a") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup;}
                            if( strcmp(UIio,"v") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux;}

                            v = atoi(UIval);

                            int i_OrMuteForceunmute = (ui[Canal].Mute | ui[Canal].MaskMute | ( ! (ui[Canal].ForceUnMute)));

                            //sprintf(sa_LogMessage,"UI2MCP <-- UI : %s\n", UIMessage);
                            //LogTrace(hfErr, 2, debug, sa_LogMessage);

                            if (strcmp(UIfunc,"forceunmute") == 0){
                                if (ui[Canal].MaskMute == 1){
                                    if (v == 1){
                                        d = 0x00;
                                        ui[Canal].ForceUnMute = 1;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.forceunmute : forceunmute to 1 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                    else if (v == 0){
                                        d = 0x7F;
                                        ui[Canal].ForceUnMute = 0;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.forceunmute :  forceunmute to 0 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.forceunmute : io.forecunmute : Canal(%i) : Mute | MaskMute | ! ForceUnMute =%i * 0x7F\n", Canal, i_OrMuteForceunmute);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                        char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMuteForceunmute*d};
                                        SendMidiOut(midiout, MidiArray);
                                    }
                                }else if (ui[Canal].MaskMute == 0){
                                    if (v == 1){
                                        ui[Canal].ForceUnMute = 1;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.forceunmute : update only forceunmute to 1 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                    else if (v == 0){
                                        ui[Canal].ForceUnMute = 0;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.forceunmute : update only forceunmute to 0 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                }
                            }
                            else if(strcmp(UIfunc,"mute") == 0){
                                if (ui[Canal].MaskMute == 0){
                                    if (v == 0){
                                        d = 0x00;
                                        ui[Canal].Mute = 0;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.mute : mute to 0 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                    else if (v == 1){
                                        d = 0x7F;
                                        ui[Canal].Mute = 1;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.mute : mute to 1 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                    if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.mute : io.mute : Canal(%i) : Mute | MaskMute | ! ForceUnMute =%i * 0x7F\n", Canal, i_OrMuteForceunmute);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                        char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMuteForceunmute*d};
                                        SendMidiOut(midiout, MidiArray);
                                    }
                                }else if (ui[Canal].MaskMute == 1){
                                    if (v == 0){
                                        ui[Canal].Mute = 0;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.mute : update only mute to 0 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                    else if (v == 1){
                                        ui[Canal].Mute = 1;
                                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.mute : update only mute to 1 on Canal(%i)\n", Canal);
                                        LogTrace(hfErr, 3, debug, sa_LogMessage);
                                    }
                                }
                            }
                            sprintf(sa_LogMessage,"UI2MCP <-> MEM : io.mute/forceunmute : Canal(%i) [Mute=%i][ForceUnMute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, ui[Canal].Mute, ui[Canal].ForceUnMute, ui[Canal].MaskMute, ui[Canal].MaskMuteValue);
                            LogTrace(hfErr, 3, debug, sa_LogMessage);
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"a") == 0 )
                                        && strcmp(UIfunc,"stereoIndex") == 0 ){

                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal + UIChannel;}
                            if( strcmp(UIio,"p") == 0){ Canal = Canal + UIChannel+UILineIn;}
                            if( strcmp(UIio,"a") == 0){ Canal = Canal + UIChannel + UILineIn + UIMedia + UIFx + UISubGroup;}

                            ui[Canal].StereoIndex = atoi(UIval);

                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : StereoIndex %i: %i\n", Canal, ui[Canal].StereoIndex);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                            if( ui[Canal].StereoIndex == 0 || ui[Canal].StereoIndex == 1 ){
                                    UpdateMidiControler();
                            }
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0) && strcmp(UIfunc,"mtkrec") == 0 ){

                            int v;
                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal + UIChannel;}

                            v = atoi(UIval);
                            if (v == 0){
                                ui[Canal].Rec = 0;
                            }
                            else if (v == 1 && (Canal <= 19 || Canal >= 24)){ // Channel 21 to 23 for input to 1 !!!!
                                ui[Canal].Rec = 1;
                            }

                            if( ui[Canal].Position >= NbMidiFader*AddrMidiTrack && ui[Canal].Position <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1 && NbRecButton == NbMidiFader ){
                                char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+ui[Canal].Position-(NbMidiFader*AddrMidiTrack) , ui[Canal].Rec*0x7F};
                                SendMidiOut(midiout, MidiArray);
                            }
                            else if( NbRecButton == 1 ){
//                                int OrRec = 0;
//                                for (int j = 0; j < UIAllStrip; j++){
//                                    if( (strcmp(ui[j].Type, "i") == 0 || strcmp(ui[j].Type, "l") == 0) && (j <= 19 || j >= 24) && UIChannel == 24 ){
//                                        OrRec = OrRec || ui[j].Rec;
//                                    }
//                                }
                                //char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec, OrRec*0x7F};
                                //SendMidiOut(midiout, MidiArray);
                                UpdateMidiControler();
                            }
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"f") == 0 || strcmp(UIio,"s") == 0 || strcmp(UIio,"a") == 0 || strcmp(UIio,"v") == 0)
                                        && strcmp(UIfunc,"name") == 0 ){

                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal+UIChannel;}
                            if( strcmp(UIio,"p") == 0){ Canal = Canal+UIChannel+UILineIn;}
                            if( strcmp(UIio,"f") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia;}
                            if( strcmp(UIio,"s") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx;}
                            if( strcmp(UIio,"a") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup;}
                            if( strcmp(UIio,"v") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux;}

                            strcpy(ui[Canal].Name, UIval);

                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : Name(%i): %s\n", Canal, ui[Canal].Name);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                            /*  Send Name to MIDI LCD  */
                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){

                                if ( Lcd == 1 ){
                                    char Cmd[32] = "";
                                    char c_Canal[32] = "";
                                    char c_CanalText[32] = "";

                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", Canal);
                                    if(strcmp(ui[Canal].Type, "i") == 0){sprintf(c_CanalText, "C%i", ui[Canal].Numb+1);}
                                    if(strcmp(ui[Canal].Type, "l") == 0 && ui[Canal].Numb == 0){sprintf(c_CanalText, "LL");}
                                    if(strcmp(ui[Canal].Type, "l") == 0 && ui[Canal].Numb == 1){sprintf(c_CanalText, "LR");}
                                    if(strcmp(ui[Canal].Type, "p") == 0 && ui[Canal].Numb == 0){sprintf(c_CanalText, "PL");}
                                    if(strcmp(ui[Canal].Type, "p") == 0 && ui[Canal].Numb == 1){sprintf(c_CanalText, "PR");}
                                    if(strcmp(ui[Canal].Type, "f") == 0){sprintf(c_CanalText, "F%i", ui[Canal].Numb+1);}
                                    if(strcmp(ui[Canal].Type, "s") == 0){sprintf(c_CanalText, "S%i", ui[Canal].Numb+1);}
                                    if(strcmp(ui[Canal].Type, "a") == 0){sprintf(c_CanalText, "A%i", ui[Canal].Numb+1);}
                                    if(strcmp(ui[Canal].Type, "v") == 0){sprintf(c_CanalText, "V%i", ui[Canal].Numb+1);}
                                    if(strcmp(ui[Canal].Type, "m") == 0 && Canal == UIAllStrip-2){sprintf(c_CanalText, "ML");}
                                    if(strcmp(ui[Canal].Type, "m") == 0 && Canal == UIAllStrip-1){sprintf(c_CanalText, "MR");}
                                    //sprintf(c_CanalText, "%i", Canal+1);
                                    strcat(Cmd, c_Canal);
                                    if( ui[Canal].Rec == 0 ){
                                        strcat(Cmd, "0100");
                                    }
                                    if( ui[Canal].Rec == 1 ){
                                        strcat(Cmd, "0104");
                                    }
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", Canal);
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0000");
                                    //SendSysExTextOut(midiout, SysExHdr, Cmd, NameChannel[Canal]);
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, ui[Canal].Name);
                                }
                            }
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"f") == 0 || strcmp(UIio,"s") == 0 || strcmp(UIio,"a") == 0 || strcmp(UIio,"v") == 0)
                                        && strcmp(UIfunc,"color") == 0 ){

                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal+UIChannel;}
                            if( strcmp(UIio,"p") == 0){ Canal = Canal+UIChannel+UILineIn;}
                            if( strcmp(UIio,"f") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia;}
                            if( strcmp(UIio,"s") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx;}
                            if( strcmp(UIio,"a") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup;}
                            if( strcmp(UIio,"v") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux;}

                            ui[Canal].Color = atoi(UIval);

                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : Color(%i): %i\n", Canal, ui[Canal].Color);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                            /*  Light On SELECT button with color to MIDI LCD  */
                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                if(ui[Canal].Color == 0){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x00};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 1){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x00};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 2){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*139)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 3){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 4){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*165)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 5){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 6){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*86)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*222)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*67)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 7){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*145)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*194)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 8){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*148)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*0)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*211)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 9){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*128)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*128)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*128)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 10){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }else if(ui[Canal].Color == 11){
                                    char MidiArrayOn[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , 0x7F};
                                    SendMidiOut(midiout, MidiArrayOn);
                                    char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*255)};
                                    SendMidiOut(midiout, MidiArrayR);
                                    char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*20)};
                                    SendMidiOut(midiout, MidiArrayG);
                                    char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack) , (int)floor((double)127/(double)255*147)};
                                    SendMidiOut(midiout, MidiArrayB);
                                }
                            }
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"f") == 0 || strcmp(UIio,"s") == 0 || strcmp(UIio,"a") == 0 || strcmp(UIio,"v") == 0)
                                        && strcmp(UIfunc,"solo") == 0 ){

                            int v;
                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal+UIChannel;}
                            if( strcmp(UIio,"p") == 0){ Canal = Canal+UIChannel+UILineIn;}
                            if( strcmp(UIio,"f") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia;}
                            if( strcmp(UIio,"s") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx;}
                            if( strcmp(UIio,"a") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup;}
                            if( strcmp(UIio,"v") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux;}

                            v = atoi(UIval);
                            if (v == 0){
                                ui[Canal].Solo = 0;
                            }
                            else if (v == 1){
                                ui[Canal].Solo = 1;
                            }
                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+Canal-(NbMidiFader*AddrMidiTrack) , ui[Canal].Solo*0x7F};
                                SendMidiOut(midiout, MidiArray);
                            }
                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : Solo(%i): %i\n", Canal, ui[Canal].Solo);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"f") == 0 || strcmp(UIio,"s") == 0)
                                        && strcmp(UIfunc,"pan") == 0 ){

                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal+UIChannel;}
                            if( strcmp(UIio,"p") == 0){ Canal = Canal+UIChannel+UILineIn;}
                            if( strcmp(UIio,"f") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia;}
                            if( strcmp(UIio,"s") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx;}

                            ui[Canal].PanMidi = atof(UIval);

                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){

                                if ( Lcd == 1 ){
                                    char Bar[3] = {0xB0, 0x30+Canal-(NbMidiFader*AddrMidiTrack), ui[Canal].PanMidi*0x7F};
                                    SendMidiOut(midiout, Bar);

                                    char Cmd[32] = "";
                                    char c_Canal[32] = "";
                                    char c_CanalText[32] = "";

                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", Canal-(NbMidiFader*AddrMidiTrack));
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0200");

                                    if( ui[Canal].PanMidi == 0.5 ){
                                        sprintf(c_CanalText, "<C>");
                                    }
                                    else if( ui[Canal].PanMidi < 0.5 ){
                                        sprintf(c_CanalText, "L%i", (int)(-200*ui[Canal].PanMidi+100));
                                    }
                                    else if( ui[Canal].PanMidi > 0.5 ){
                                        sprintf(c_CanalText, "R%i", (int)(200*ui[Canal].PanMidi-100));
                                    }

                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                                }
                            }

                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : Fader %i: %f\n", Canal, ui[Canal].PanMidi);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);
                        }
                        else if( (strcmp(UIio,"i") == 0 || strcmp(UIio,"l") == 0 || strcmp(UIio,"p") == 0 || strcmp(UIio,"f") == 0 || strcmp(UIio,"s") == 0 || strcmp(UIio,"a") == 0 || strcmp(UIio,"v") == 0)
                                        && strcmp(UIfunc,"mix") == 0 ){

                            Canal = atoi(UIchan);
                            if( strcmp(UIio,"i") == 0){ /*  Nothing  */}
                            if( strcmp(UIio,"l") == 0){ Canal = Canal+UIChannel;}
                            if( strcmp(UIio,"p") == 0){ Canal = Canal+UIChannel+UILineIn;}
                            if( strcmp(UIio,"f") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia;}
                            if( strcmp(UIio,"s") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx;}
                            if( strcmp(UIio,"a") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup;}
                            if( strcmp(UIio,"v") == 0){ Canal = Canal+UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux;}

                            ui[Canal].MixMidi = atof(UIval);

                            int MidiValue = 0;
                            //float unit = 0.0078740157480315;
                            //MidiValue = (127 * MixMidi[Canal]);
                            MidiValue = (127 * ui[Canal].MixMidi);

                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : Fader %i: %f %i\n", Canal, ui[Canal].MixMidi, MidiValue);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                            sprintf(sa_LogMessage, "UI2MCP <-> MEM : ADD %i NB %i Ste %i, Fader %i: %f %i\n", AddrMidiTrack, NbMidiFader, ui[Canal].StereoIndex, Canal, ui[Canal].MixMidi, MidiValue);
                            LogTrace(hfErr, 2, debug, sa_LogMessage);

                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                if( ui[Canal].StereoIndex == -1 || strcmp(ui[Canal].Type,"v") == 0 ){
//                                if( ui[Canal].StereoIndex == -1 || ( strcmp(ui[Canal].Type,"v") == 0 && ui[Canal].StereoIndex == 0 )){ // Possible bug on UI --> stereoindex for VCA = 0 !!!
                                    char MidiArray[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                    SendMidiOut(midiout, MidiArray);
                                }
                                else if( ui[Canal].StereoIndex == 0 && ui[Canal+1].StereoIndex == 1 ){
                                    char MidiArrayL[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                    SendMidiOut(midiout, MidiArrayL);
                                    char MidiArrayR[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)+1 , MidiValue, MidiValue};
                                    SendMidiOut(midiout, MidiArrayR);
                                }
                            }
                        }
                    free(UIMessage);
                    }
                }
                free(ArrayBuffer);
                free(SplitArrayCRLF);
            }
        }
        else{
            LogTrace(hfErr, 1, debug, "UI2MCP <-- MIDI : Midi read\n");

            InMidi = (int)Midibuffer[0];
            MidiCC = (int)Midibuffer[1];
            MidiValue = (int)Midibuffer[2];

            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Midi IN: 0x%02X 0x%02X 0x%02X\n",InMidi, MidiCC, MidiValue);
            LogTrace(hfErr, 2, debug, sa_LogMessage);

            if ( InMidi >= AddrMidiMix && InMidi <= AddrMidiMix + NbMidiFader - 1 + ModeMasterPressed ){                                            /*  Midi command Fader  */
                Canal = InMidi % AddrMidiMix +(NbMidiFader*AddrMidiTrack);

                int fs;
                float db;
                float unit = 0.0078740157480315;
                for (fs = 0; fs < status; fs+=3){
                    status = snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer));
                    if ( InMidi >= AddrMidiMix && InMidi <= AddrMidiMix + NbMidiFader - 1 + ModeMasterPressed ){
                        InMidi = (int)Midibuffer[fs];
                        MidiCC = (int)Midibuffer[fs+1];
                        MidiValue = (int)Midibuffer[fs+2];
                        if ( InMidi != AddrMidiButtonLed ){

//                            else{
                            if ( ModeMasterPressed == 1 && (InMidi % AddrMidiMix) == NbMidiFader ){
                                db = MidiValue * unit;
                                ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].MixMidi = db;
                                ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca+1].MixMidi = db;

                                char sendui[256];
                                sprintf(sendui,"SETD^m.mix^%.10f\n", db);
                                send(sock , sendui, strlen(sendui) , 0 );
                            }
                            else{
                                if( strcmp(ui[Canal].Type, "m") !=0 ){
                                    db = MidiValue * unit;
                                    ui[Canal].MixMidi = db;

                                    char sendui[256];
                                    sprintf(sendui,"SETD^%s.%d.mix^%.10f\n", ui[Canal].Type, ui[Canal].Numb, db);
                                    send(sock , sendui, strlen(sendui) , 0 );

                                    if( ui[Canal].StereoIndex == 0 && ui[Canal+1].StereoIndex == 1 ){
                                        char sendui[256];
                                        ui[Canal+1].MixMidi = db;
                                        sprintf(sendui,"SETD^%s.%d.mix^%.10f\n", ui[Canal+1].Type, ui[Canal+1].Numb, db);
                                        send(sock , sendui, strlen(sendui) , 0 );
                                    }
                                    else if( ui[Canal-1].StereoIndex == 0 && ui[Canal].StereoIndex == 1 ){
                                        char sendui[256];
                                        ui[Canal-1].MixMidi = db;
                                        sprintf(sendui,"SETD^%s.%d.mix^%.10f\n", ui[Canal-1].Type, ui[Canal-1].Numb, db);
                                        send(sock , sendui, strlen(sendui) , 0 );
                                    }
                                }
                                else{
                                    db = MidiValue * unit;
                                    if( Canal == UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca ){
                                        ui[Canal].MixMidi = db;
                                        ui[Canal+1].MixMidi = db;

                                        //char MidiArrayR[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)+1 , MidiValue, MidiValue};
                                        //SendMidiOut(midiout, MidiArrayR);
                                        char MidiArrayL[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)+1 , MidiValue, MidiValue};
                                        SendMidiOut(midiout, MidiArrayL);

                                    }
                                    else if( Canal == UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca+1 ){
                                        ui[Canal].MixMidi = db;
                                        ui[Canal-1].MixMidi = db;

                                        char MidiArrayR[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)-1, MidiValue, MidiValue};
                                        SendMidiOut(midiout, MidiArrayR);
                                        //char MidiArrayL[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)+1 , MidiValue, MidiValue};
                                        //SendMidiOut(midiout, MidiArrayL);
                                    }

                                    char sendui[256];
                                    sprintf(sendui,"SETD^%s.mix^%.10f\n", ui[Canal].Type, db);
                                    send(sock , sendui, strlen(sendui) , 0 );
                                }
                            }

                            printf("Status %i Potentiometer %i - %i: %02X %02X, %.10f\n", status, Canal, ui[Canal].Numb, MidiCC, MidiValue, db);
                            // Write on LCD the value of the db for the channel
                            if ( Lcd == 1 ){
                                    char Cmd[32] = "";
                                    char c_Canal[32] = "";
                                    char c_CanalText[32] = "";

                                    strcpy(Cmd, "12");
                                    if ( ModeMasterPressed == 0 ){
                                        sprintf(c_Canal, "%02X", Canal-(NbMidiFader*AddrMidiTrack));
                                        sprintf(c_CanalText, "%.1f", round(linear_to_db(ui[Canal].MixMidi)));
                                    }
                                    else if( ModeMasterPressed == 1 ){
                                        printf("InMidi %i AddrMidiMix %i MOD %i NbMidiFader %i\n", InMidi, AddrMidiMix, InMidi % AddrMidiMix, NbMidiFader);
                                        if ( (InMidi % AddrMidiMix) == NbMidiFader ){
                                            printf("ICICIC\n");
                                            sprintf(c_Canal, "%02X", NbMidiFader);
                                            sprintf(c_CanalText, "%.1f", round(linear_to_db(ui[UIChannel+UILineIn+UIMedia+UIFx+UISubGroup+UIAux+UIVca].MixMidi)));
                                        }
                                        else{
                                            sprintf(c_Canal, "%02X", Canal-(NbMidiFader*AddrMidiTrack));
                                            sprintf(c_CanalText, "%.1f", round(linear_to_db(ui[Canal].MixMidi)));
                                        }
                                    }
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0200");
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                            }

                            if( ui[Canal].StereoIndex == 0 && ui[Canal+1].StereoIndex == 1 ){
                                char MidiArrayR[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)+1 , MidiValue, MidiValue};
                                SendMidiOut(midiout, MidiArrayR);
                            }
                            else if( ui[Canal-1].StereoIndex == 0 && ui[Canal].StereoIndex == 1 ){
                                MidiValue = (127 * ui[Canal].MixMidi);
                                char MidiArrayL[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)-1 , MidiValue, MidiValue};
                                SendMidiOut(midiout, MidiArrayL);
                            }
                        }
                        else if (InMidi == AddrMidiButtonLed && MidiCC >= AddrMidiTouch && MidiCC <= AddrMidiTouch + NbMidiFader - 1){                     /*  After Touch  */

                            Canal = MidiCC % AddrMidiTouch +(NbMidiFader*AddrMidiTrack);

                            if(MidiValue == 0x00){
                                if( ui[Canal].StereoIndex == 0 && ui[Canal+1].StereoIndex == 1 ){
                                    MidiValue = (127 * ui[Canal].MixMidi);
                                    char MidiArrayR[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)+1 , MidiValue, MidiValue};
                                    SendMidiOut(midiout, MidiArrayR);
                                }
                                else if( ui[Canal-1].StereoIndex == 0 && ui[Canal].StereoIndex == 1 ){
                                    MidiValue = (127 * ui[Canal].MixMidi);
                                    char MidiArrayL[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack)-1 , MidiValue, MidiValue};
                                    SendMidiOut(midiout, MidiArrayL);
                                }
                                usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                                UpdateMidiControler();
                            }
                        }
                    }
                }
            }
            else if (InMidi == AddrMidiEncoder){                                                                                                   /*  Midi command Encoder  */
                if (MidiCC >= AddrMidiEncoderPan && MidiCC <= AddrMidiEncoderPan + NbMidiFader - 1 && NbPanButton == NbMidiFader && PanPressed == 1 ){

                    Canal = MidiCC % AddrMidiEncoderPan +(NbMidiFader*AddrMidiTrack);

                    /*  Give information to UI for channel selected  */
                    char sendui[256];
                    sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                    send(sock , sendui, strlen(sendui) , 0 );

                    if ( strcmp(ControlerConfig.TypePan,"POT") == 0 ){
                        if(MidiValue == 0x41 && ui[Canal].PanMidi > 0){
                            ui[Canal].PanMidi =  fabs (ui[Canal].PanMidi-.0003*NbPanStep[Canal]);
                        }
                        else if(MidiValue == 0x01 && ui[Canal].PanMidi < 1){
                            ui[Canal].PanMidi = fabs (ui[Canal].PanMidi+.0003*NbPanStep[Canal]);
                        }
                        else if(MidiValue == 0x3F){
                            NbPanStep[Canal] = 0;
                            ui[Canal].PanMidi =  1;
                        }
                        else if(MidiValue == 0x7F){
                            NbPanStep[Canal] = 0;
                            ui[Canal].PanMidi =  0;
                        }
                        NbPanStep[Canal]++;
                    }
                    else if ( strcmp(ControlerConfig.TypePan,"ENC") == 0 ){
                        if(( MidiValue >= 0x41 && MidiValue <= 0x45 ) && ui[SelectButtonPressed].PanMidi > 0 && ui[SelectButtonPressed].PanMidi != 0 ){
                            ui[SelectButtonPressed].PanMidi =  ui[SelectButtonPressed].PanMidi - (.01*(MidiValue % 0x40));
                        }
                        else if(( MidiValue >= 0x01 && MidiValue <= 0x05 ) && ui[SelectButtonPressed].PanMidi < 1 && ui[SelectButtonPressed].PanMidi != 1 ){
                            ui[SelectButtonPressed].PanMidi = ui[SelectButtonPressed].PanMidi + (.01*MidiValue);
                        }
                    }
                    printf("Pan (step[%i])%i: %f\n", NbPanStep[Canal], Canal, ui[Canal].PanMidi);

                    /*  Write LCD controler with PAN value  */
                    if ( Lcd == 1 ){
                        /*  Update LCD BAR controler with PAN value  */
                        char Bar[3] = {0xB0, 0x30+Canal-(NbMidiFader*AddrMidiTrack), ui[Canal].PanMidi*0x7F};
                        SendMidiOut(midiout, Bar);

                        char Cmd[32] = "";
                        char c_Canal[32] = "";
                        char c_CanalText[32] = "";

                        /*  Compute string with PAN value  */
                        strcpy(Cmd, "12");
                        sprintf(c_Canal, "%02X", Canal);
                        strcat(Cmd, c_Canal);
                        strcat(Cmd, "0200");

                        /*  Translate float value to percent L & R  */
                        if( ui[Canal].PanMidi == 0.5 ){
                            sprintf(c_CanalText, "<C>");
                        }
                        else if( ui[Canal].PanMidi < 0.5 ){
                            int i_Left = (int)(-200*ui[Canal].PanMidi+100);
                            if( i_Left > 100 ){ i_Left = 100; }
                            sprintf(c_CanalText, "L%i", i_Left);
                        }
                        else if( ui[Canal].PanMidi > 0.5 ){
                            int i_Right = (int)(200*ui[Canal].PanMidi-100);
                            if( i_Right > 100 ){ i_Right = 100; }
                            sprintf(c_CanalText, "R%i", i_Right);
                        }
                        SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                    }

                    char Bar[3] = {0xB0, 0x30+MidiCC, ui[Canal].PanMidi*0x7F};
                    SendMidiOut(midiout, Bar);

                    sprintf(sendui,"SETD^%s.%d.pan^%.10f\n", ui[Canal].Type, ui[Canal].Numb, ui[Canal].PanMidi);
                    send(sock , sendui, strlen(sendui) , 0 );
                }
                else if (MidiCC == AddrMidiEncoderPan && NbPanButton == 1 && PanPressed == 1 && SelectButtonPressed != -1){

                    /*  Give information to UI for channel selected  */
                    char sendui[256];
                    sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, SelectButtonPressed);
                    send(sock , sendui, strlen(sendui) , 0 );

                    if( strcmp( ui[SelectButtonPressed].Type, "m" ) != 0 ){
                        if ( strcmp(ControlerConfig.TypePan,"POT") == 0 ){
                            if(MidiValue == 0x41 && ui[SelectButtonPressed].PanMidi >= 0){
                                ui[SelectButtonPressed].PanMidi =  fabs (ui[SelectButtonPressed].PanMidi+.0003*NbPanStep[AddrMidiEncoderPan]);
                            }
                            else if(MidiValue == 0x01 && ui[SelectButtonPressed].PanMidi <= 1){
                                ui[SelectButtonPressed].PanMidi = fabs (ui[SelectButtonPressed].PanMidi-.0003*NbPanStep[AddrMidiEncoderPan]);
                            }
                            else if(MidiValue == 0x3F){
                                NbPanStep[AddrMidiEncoderPan] = 0;
                                ui[SelectButtonPressed].PanMidi =  1;
                            }
                            else if(MidiValue == 0x7F){
                                NbPanStep[AddrMidiEncoderPan] = 0;
                                ui[SelectButtonPressed].PanMidi =  0;
                            }
                            NbPanStep[AddrMidiEncoderPan]++;
                        }
                        else if ( strcmp(ControlerConfig.TypePan,"ENC") == 0 ){
                            if(( MidiValue >= 0x41 && MidiValue <= 0x45 ) && ui[SelectButtonPressed].PanMidi > 0 && ui[SelectButtonPressed].PanMidi != 0 ){
                                ui[SelectButtonPressed].PanMidi =  ui[SelectButtonPressed].PanMidi - (.01*(MidiValue % 0x40));
                            }
                            else if(( MidiValue >= 0x01 && MidiValue <= 0x05 ) && ui[SelectButtonPressed].PanMidi < 1 && ui[SelectButtonPressed].PanMidi != 1 ){
                                ui[SelectButtonPressed].PanMidi = ui[SelectButtonPressed].PanMidi + (.01*MidiValue);
                            }
                        }
                        printf("Pan (step[%i])%i: %f\n", NbPanStep[AddrMidiEncoderPan], SelectButtonPressed, ui[SelectButtonPressed].PanMidi);

                        /*  Write LCD controler with PAN value  */
                        if ( Lcd == 1 ){
                            /*  Update LCD BAR controler with PAN value  */
                            char Bar[3] = {0xB0, 0x30+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), ui[SelectButtonPressed].PanMidi*0x7F};
                            SendMidiOut(midiout, Bar);

                            char Cmd[32] = "";
                            char c_Canal[32] = "";
                            char c_CanalText[32] = "";

                            /*  Compute string with PAN value  */
                            strcpy(Cmd, "12");
                            sprintf(c_Canal, "%02X", SelectButtonPressed-(NbMidiFader*AddrMidiTrack));
                            strcat(Cmd, c_Canal);
                            strcat(Cmd, "0200");

                            /*  Translate float value to percent L & R  */
                            if( ui[SelectButtonPressed].PanMidi == 0.5 ){
                                sprintf(c_CanalText, "<C>");
                            }
                            else if( ui[SelectButtonPressed].PanMidi < 0.5 ){
                                int i_Left = (int)(-200*ui[SelectButtonPressed].PanMidi+100);
                                if( i_Left > 100 ){ i_Left = 100; }
                                sprintf(c_CanalText, "L%i", i_Left);

                            }
                            else if( ui[SelectButtonPressed].PanMidi > 0.5 ){
                                int i_Right = (int)(200*ui[SelectButtonPressed].PanMidi-100);
                                if( i_Right > 100 ){ i_Right = 100; }
                                sprintf(c_CanalText, "R%i", i_Right);
                            }
                            SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                        }

                        if(SelectButtonPressed >= NbMidiFader*AddrMidiTrack && SelectButtonPressed <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                            char Bar[3] = {0xB0, 0x30+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), ui[SelectButtonPressed].PanMidi*0x7F};
                            SendMidiOut(midiout, Bar);
                        }

                        sprintf(sendui,"SETD^%s.%d.pan^%.10f\n", ui[SelectButtonPressed].Type, ui[SelectButtonPressed].Numb, ui[SelectButtonPressed].PanMidi);
                        send(sock , sendui, strlen(sendui) , 0 );
                    }
                    else if( strcmp( ui[SelectButtonPressed].Type, "m" ) == 0 ){
                        if ( strcmp(ControlerConfig.TypePan,"POT") == 0 ){
                            if(MidiValue == 0x41 && ui[SelectButtonPressed].PanMidi >= 0){
                                ui[SelectButtonPressed].PanMidi =  fabs (ui[SelectButtonPressed].PanMidi+.0003*NbPanStep[AddrMidiEncoderPan]);
                            }
                            else if(MidiValue == 0x01 && ui[SelectButtonPressed].PanMidi <= 1){
                                ui[SelectButtonPressed].PanMidi = fabs (ui[SelectButtonPressed].PanMidi-.0003*NbPanStep[AddrMidiEncoderPan]);
                            }
                            else if(MidiValue == 0x3F){
                                NbPanStep[AddrMidiEncoderPan] = 0;
                                ui[SelectButtonPressed].PanMidi =  1;
                            }
                            else if(MidiValue == 0x7F){
                                NbPanStep[AddrMidiEncoderPan] = 0;
                                ui[SelectButtonPressed].PanMidi =  0;
                            }
                            NbPanStep[AddrMidiEncoderPan]++;
                        }
                        else if ( strcmp(ControlerConfig.TypePan,"ENC") == 0 ){
                            if(( MidiValue >= 0x41 && MidiValue <= 0x45 ) && ui[SelectButtonPressed].PanMidi > 0 && ui[SelectButtonPressed].PanMidi != 0 ){
                                ui[SelectButtonPressed].PanMidi =  ui[SelectButtonPressed].PanMidi - (.005*(MidiValue % 0x40));
                            }
                            else if(( MidiValue >= 0x01 && MidiValue <= 0x05 ) && ui[SelectButtonPressed].PanMidi < 1 && ui[SelectButtonPressed].PanMidi != 1 ){
                                ui[SelectButtonPressed].PanMidi = ui[SelectButtonPressed].PanMidi + (.005*MidiValue);
                            }
                        }
                        printf("Pan (step[%i])%i: %f\n", NbPanStep[AddrMidiEncoderPan], SelectButtonPressed, ui[SelectButtonPressed].PanMidi);

                        int Canal = 0;
                        if( ui[SelectButtonPressed].Numb == 0 ){
                            ui[SelectButtonPressed+1].PanMidi = ui[SelectButtonPressed].PanMidi;
                            Canal = SelectButtonPressed;
                        }
                        if( ui[SelectButtonPressed].Numb == 1 ){
                            ui[SelectButtonPressed-1].PanMidi = ui[SelectButtonPressed].PanMidi;
                            Canal = SelectButtonPressed-1;
                        }

                        if( ui[SelectButtonPressed].PanMidi == 0.5 ){
                            int MidiValue = 0;
                            MidiValue = (127 * ui[Canal].MixMidi);

                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                SendMidiOut(midiout, MidiArrayLeft);

                                char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                                SendMidiOut(midiout, MidiArrayRight);
                            }
                        }
                        else if( ui[SelectButtonPressed].PanMidi < 0.5 ){
                            int MidiValueR = 0;
                            MidiValueR = (127 * (ui[Canal].MixMidi-ui[Canal].MixMidi*(-200*ui[Canal+1].PanMidi+100)/100));

                            int MidiValueL = 0;
                            MidiValueL = (127 * ui[Canal].MixMidi);

                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueL, MidiValueL};
                                SendMidiOut(midiout, MidiArrayLeft);

                                char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValueR, MidiValueR};
                                SendMidiOut(midiout, MidiArrayRight);
                            }
                        }
                        else if( ui[SelectButtonPressed].PanMidi > 0.5 ){
                            int MidiValueL = 0;
                            MidiValueL = (127 * (ui[Canal+1].MixMidi-ui[Canal+1].MixMidi*(200*ui[Canal].PanMidi-100)/100));

                            int MidiValueR = 0;
                            MidiValueR = (127 * ui[Canal+1].MixMidi);

                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                char MidiArrayLeft[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValueL, MidiValueL};
                                SendMidiOut(midiout, MidiArrayLeft);

                                char MidiArrayRight[3] = {AddrMidiMix+Canal+1-(NbMidiFader*AddrMidiTrack) , MidiValueR, MidiValueR};
                                SendMidiOut(midiout, MidiArrayRight);
                            }
                        }

                        /*  Write LCD controler with PAN value  */
                        if ( Lcd == 1 ){
                            /*  Update LCD BAR controler with PAN value  */
                            char Bar[3] = {0xB0, 0x30+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), ui[SelectButtonPressed].PanMidi*0x7F};
                            SendMidiOut(midiout, Bar);

                            char Cmd[32] = "";
                            char c_Canal[32] = "";
                            char c_CanalText[32] = "";

                            /*  Translate float value to percent L & R  */
                            if( ui[SelectButtonPressed].PanMidi == 0.5 ){
                                sprintf(c_CanalText, "<C>");
                            }
                            else if( ui[SelectButtonPressed].PanMidi < 0.5 ){
                                int i_Left = (int)(-200*ui[SelectButtonPressed].PanMidi+100);
                                if( i_Left > 100 ){ i_Left = 100; }
                                sprintf(c_CanalText, "L%i", i_Left);
                            }
                            else if( ui[SelectButtonPressed].PanMidi > 0.5 ){
                                int i_Right = (int)(200*ui[SelectButtonPressed].PanMidi-100);
                                if( i_Right > 100 ){ i_Right = 100; }
                                sprintf(c_CanalText, "R%i", i_Right);
                            }

                            /*  Compute string with PAN value  */
                            if( ModeMasterPressed == 1 ){
                                strcpy(Cmd, "12");
                                sprintf(c_Canal, "%02X", SelectButtonPressed-(NbMidiFader*AddrMidiTrack));
                                strcat(Cmd, c_Canal);
                                strcat(Cmd, "0200");
                                SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                            }
                            if( ModeMasterPressed == 0 ){
                                if( ui[SelectButtonPressed].Numb == 0 ){
                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", SelectButtonPressed-(NbMidiFader*AddrMidiTrack));
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0200");
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", SelectButtonPressed+1-(NbMidiFader*AddrMidiTrack));
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0200");
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                                }
                                if( ui[SelectButtonPressed].Numb == 1 ){
                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", SelectButtonPressed-1-(NbMidiFader*AddrMidiTrack));
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0200");
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

                                    strcpy(Cmd, "12");
                                    sprintf(c_Canal, "%02X", SelectButtonPressed-(NbMidiFader*AddrMidiTrack));
                                    strcat(Cmd, c_Canal);
                                    strcat(Cmd, "0200");
                                    SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);
                                }
                            }
                        }

                        if(SelectButtonPressed >= NbMidiFader*AddrMidiTrack && SelectButtonPressed <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                            if( ModeMasterPressed == 0 ){
                                char BarL[3] = {0xB0, 0x30+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), ui[SelectButtonPressed].PanMidi*0x7F};
                                SendMidiOut(midiout, BarL);
                                char BarR[3] = {0xB0, 0x30+SelectButtonPressed+1-(NbMidiFader*AddrMidiTrack), ui[SelectButtonPressed].PanMidi*0x7F};
                                SendMidiOut(midiout, BarR);
                            }
                            if( ModeMasterPressed == 1 ){
                                char Bar[3] = {0xB0, 0x30+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), ui[SelectButtonPressed].PanMidi*0x7F};
                                SendMidiOut(midiout, Bar);
                            }
                        }
                        sprintf(sendui,"SETD^m.pan^%.10f\n",  ui[SelectButtonPressed].PanMidi);
                        send(sock , sendui, strlen(sendui) , 0 );
                    }
                }
                else if ( MidiCC == AddrMidiEncoderSession ){

//                    printf("Test Index=%i Max=%i\n", SnapShotIndex, SnapShotMax);
//
//                    for (int c = 0; c <= SnapShotMax; c++){
//                            printf("SnapValue %i [%s]\n",  c, UISnapShotList[c]);
//                    }

                    if( ModeSnapShotsPressed == 1){
                        // Function for move in the current mode
                        if( MidiValue >= 0x41 && MidiValue <= 0x45 && SnapShotIndex > 0 ){
                            SnapShotIndex--;
                        }
                        else if( MidiValue >= 0x01 && MidiValue <= 0x05 && SnapShotIndex < SnapShotMax ){
                            SnapShotIndex++;
                        }
                        ConfigLCDTxtMode();
                        LcdExplorerUpdate(midiout, SysExHdr, SnapShotIndex, &UISnapShotList);
                    }
                    else if( ModeCuesPressed == 1){
                        // Function for move in the current mode
                        if( MidiValue >= 0x41 && MidiValue <= 0x45 && CuesIndex > 0 ){
                            CuesIndex--;
                        }
                        else if( MidiValue >= 0x01 && MidiValue <= 0x05 && CuesIndex < CuesMax ){
                            CuesIndex++;
                        }
                        ConfigLCDTxtMode();
                        LcdExplorerUpdate(midiout, SysExHdr, CuesIndex, &UICuesList);
                    }
                    else if( ModeShowsPressed == 1){
                        // Function for move in the current mode
                        if( MidiValue >= 0x41 && MidiValue <= 0x45 && ShowsIndex > 0 ){
                            ShowsIndex--;
                        }
                        else if( MidiValue >= 0x01 && MidiValue <= 0x05 && ShowsIndex < ShowsMax ){
                            ShowsIndex++;
                        }
                        ConfigLCDTxtMode();
                        LcdExplorerUpdate(midiout, SysExHdr, ShowsIndex, &UIShowsList);
                    }
                    else if( ModeDirPlayerPressed == 1){
                        // Function for move in the current mode
                        if( MidiValue >= 0x41 && MidiValue <= 0x45 && DirPlayerIndex > 0 ){
                            DirPlayerIndex--;
                        }
                        else if( MidiValue >= 0x01 && MidiValue <= 0x05 && DirPlayerIndex < DirPlayerMax ){
                            DirPlayerIndex++;
                        }
                        ConfigLCDTxtMode();
                        LcdExplorerUpdate(midiout, SysExHdr, DirPlayerIndex, &UIDirPlayerList);
                    }
                    else if( ModeFilesPlayerPressed == 1){
                        // Function for move in the current mode
                        if( MidiValue >= 0x41 && MidiValue <= 0x45 && FilesPlayerIndex > 0 ){
                            FilesPlayerIndex--;
                        }
                        else if( MidiValue >= 0x01 && MidiValue <= 0x05 && FilesPlayerIndex < FilesPlayerMax ){
                            FilesPlayerIndex++;
                        }
                        ConfigLCDTxtMode();
                        LcdExplorerUpdate(midiout, SysExHdr, FilesPlayerIndex, &UIFilesPlayerList);
                    }
                    else if( ModeMtkSessionsPressed == 1){
                        // Function for move in the current mode
                        if( MidiValue >= 0x41 && MidiValue <= 0x45 && MtkSessionIndex > 0 ){
                            MtkSessionIndex--;
                        }
                        else if( MidiValue >= 0x01 && MidiValue <= 0x05 && MtkSessionIndex < MtkSessionMax ){
                            MtkSessionIndex++;
                        }
                        ConfigLCDTxtMode();
                        LcdExplorerUpdate(midiout, SysExHdr, MtkSessionIndex, &UIMtkSessionList);
                    }
                }
            }
            else if (InMidi == AddrMidiButtonLed){                                                                                                                            /*  Midi command Button & Led  */
                if ( (MidiCC >= AddrMidiRec && MidiCC <= AddrMidiRec + NbMidiFader - 1) && NbRecButton == NbMidiFader ){                                    /*  Record  */

                    Canal = MidiCC +(NbMidiFader*AddrMidiTrack);

                    if( (strcmp(ui[Canal].Type, "i") == 0 || strcmp(ui[Canal].Type, "l") == 0) && UIChannel == 24){

                        if(MidiValue == 0x7F && ui[Canal].Rec == 0 && (Canal <= 19 || Canal >= 24)){
                            ui[Canal].Rec = 1;
                            char sendui[256];
                            sprintf(sendui,"SETD^%s.%d.mtkrec^1\n", ui[Canal].Type, ui[Canal].Numb);
                            send(sock , sendui, strlen(sendui) , 0 );

                            char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                            SendMidiOut(midiout, MidiArray);
                        }
                        else if(MidiValue == 0x7F && ui[Canal].Rec == 1 && (Canal <= 19 || Canal >= 24)){
                            ui[Canal].Rec = 0;
                            char sendui[256];
                            sprintf(sendui,"SETD^%s.%d.mtkrec^0\n", ui[Canal].Type, ui[Canal].Numb);
                            send(sock , sendui, strlen(sendui) , 0 );

                            char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                            SendMidiOut(midiout, MidiArray);
                        }
                        usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                    }
                }
                else if (MidiCC >= AddrMidiSolo && MidiCC <= AddrMidiSolo + NbMidiFader -1){                             /*  Solo  */

                    Canal = MidiCC % AddrMidiSolo +(NbMidiFader*AddrMidiTrack);

                    if(MidiValue == 0x7F){
                        char sendui[256];
                        sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                        send(sock , sendui, strlen(sendui) , 0 );
                    }

                    if( SoloMode == 0 ){
                        //Init value array
                        for (int j = 0; j < UIAllStrip; j++){
                            if(j != Canal){
                                ui[j].Solo = 0;
                                char sendui[256];
                                sprintf(sendui,"SETD^%s.%d.solo^0\n", ui[j].Type, ui[j].Numb);
                                send(sock , sendui, strlen(sendui) , 0 );
                            }
                        }
                    }

                    if(MidiValue == 0x7F && ui[Canal].Solo == 0){
                        ui[Canal].Solo = 1;
                        char sendui[256];
                        sprintf(sendui,"SETD^%s.%d.solo^1\n", ui[Canal].Type, ui[Canal].Numb);
                        send(sock , sendui, strlen(sendui) , 0 );

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                        for (int j = 0; j < UIAllStrip; j++){
                            if(ui[j].Solo == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
                                char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x00};
                                SendMidiOut(midiout, MidiArray);
                            }
                        }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                    // Clear Midi Buffer for toggle function
                    snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer));

                    }
                    else if(MidiValue == 0x00 && ui[Canal].Solo == 1){
                        ui[Canal].Solo = 0;
                        char sendui[256];
                        sprintf(sendui,"SETD^%s.%d.solo^0\n", ui[Canal].Type, ui[Canal].Numb);
                        send(sock , sendui, strlen(sendui) , 0 );

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);

                        for (int j = 0; j < UIAllStrip; j++){
                            if(ui[j].Solo == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
                                char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x00};
                                SendMidiOut(midiout, MidiArray);
                            }
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC >= AddrMidiMute && MidiCC <= AddrMidiMute + NbMidiFader - 1){                        /*  Mute  */

                    Canal = MidiCC % AddrMidiMute +(NbMidiFader*AddrMidiTrack);

                    if( MidiValue == 0x7F ){
                        char sendui[256];

                        sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                        send(sock , sendui, strlen(sendui) , 0 );

                        if( ui[Canal].MaskMute == 0){
                            if( ui[Canal].Mute == 0){
                               ui[Canal].Mute = 1;

                                sprintf(sendui,"SETD^%s.%d.mute^1\n", ui[Canal].Type, ui[Canal].Numb);
                                printf(sendui,"SETD^%s.%d.mute^1\n", ui[Canal].Numb);

                                char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                                SendMidiOut(midiout, MidiArray);
                            }
                            else{
                               ui[Canal].Mute = 0;

                                sprintf(sendui,"SETD^%s.%d.mute^0\n", ui[Canal].Type, ui[Canal].Numb);
                                printf(sendui,"SETD^%s.%d.mute^0\n", ui[Canal].Numb);

                                char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                                SendMidiOut(midiout, MidiArray);
                            }
                        }
                        else if( ui[Canal].MaskMute == 1 ){
                            if( ui[Canal].ForceUnMute == 0 ){
                                ui[Canal].ForceUnMute = 1;

                                sprintf(sendui,"SETD^%s.%d.forceunmute^1\n", ui[Canal].Type, ui[Canal].Numb);
                                printf(sendui,"SETD^%s.%d.forceunmute^1\n", ui[Canal].Numb);

                                char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                                SendMidiOut(midiout, MidiArray);
                            }
                            else if( ui[Canal].ForceUnMute == 1 ){
                                ui[Canal].ForceUnMute = 0;

                                sprintf(sendui,"SETD^%s.%d.forceunmute^0\n", ui[Canal].Type, ui[Canal].Numb);
                                printf(sendui,"SETD^%s.%d.forceunmute^0\n", ui[Canal].Numb);

                                char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                                SendMidiOut(midiout, MidiArray);
                            }
                        }
                        send(sock , sendui, strlen(sendui) , 0 );
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC >= AddrMidiTouch && MidiCC <= AddrMidiTouch + NbMidiFader - 1){                     /*  After Touch  */

                    Canal = MidiCC % AddrMidiTouch +(NbMidiFader*AddrMidiTrack);

                    if(MidiValue == 0x7F){
                        char sendui[256];
                        sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                        send(sock , sendui, strlen(sendui) , 0 );
                    }
                }
                else if (MidiCC == i_Tap){                                                                                                         /*  Button for Track Next */

                    if(MidiValue == 0x7F){
                        i_NbTap++;

                        tap[i_NbTap] = currentTimeMillis();

                        int SumTap = 0;
                        int Compt = 0;
                        double MoyenneTap = 0;

                        for (int t = 1; t < i_NbTap; t+=2){
                            //printf("Tap : %i\n",  60000/(tap[t+1]-tap[t] ));
                            SumTap+=(60000/(tap[t+1]-tap[t]));
                            Compt = t;
                        }
                        //printf("Div Tap : %i %i\n", Compt, (Compt/2)+1);
                        //printf("SumTap : %i\n", SumTap);
                        MoyenneTap = SumTap / ((Compt/2)+1);
                        //printf("Moyenne Tap : %i\n", (int)MoyenneTap);

                        if ( (Compt/2)+1 > 1 ){
                            UIBpm = (int)MoyenneTap;
                            printf("SETD^f.1.bpm^%i\n", UIBpm);

                            char sendui[256];
                            sprintf(sendui,"SETD^f.1.bpm^%i\n", UIBpm);
                            send(sock , sendui, strlen(sendui) , 0 );
                        }
                    }
                }
                else if (MidiCC == IdTrackNext){                                                                                                         /*  Button for Track Next */

                    int i_FlagNext = 0;
                    NbMidiTrack = UIAllStrip/NbMidiFader;

                    if(MidiValue == 0x7F && AddrMidiTrack == 0){
                        AddrMidiTrack++;
                        i_FlagNext = 1;

                        if( ShiftRightPressed == 1){
                            AddrMidiTrack = NbMidiTrack-1;

                            ShiftRightPressed = 0;

                            char ShiftRightLedOn[3] = {AddrMidiButtonLed, AddrShiftRight, 0x00};
                            SendMidiOut(midiout, ShiftRightLedOn);
                        }

                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : Address Track : %i\n", AddrMidiTrack);
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        if ( Lcd == 0 ){
                            char RewindLedOn[3] = {AddrMidiButtonLed, IdForward, 0x7F};
                            SendMidiOut(midiout, RewindLedOn);
                        }
                    }
                    else if(MidiValue == 0x7F && AddrMidiTrack < NbMidiTrack-1){
                        AddrMidiTrack++;
                        i_FlagNext = 1;

                        if( ShiftRightPressed == 1){
                            AddrMidiTrack = NbMidiTrack-1;

                            ShiftRightPressed = 0;

                            char ShiftRightLedOn[3] = {AddrMidiButtonLed, AddrShiftRight, 0x00};
                            SendMidiOut(midiout, ShiftRightLedOn);
                        }

                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : Address Track : %i\n", AddrMidiTrack);
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        if ( Lcd == 0 ){
                            char ForwardLedOn[3] = {AddrMidiButtonLed, IdRewind, 0x00};
                            SendMidiOut(midiout, ForwardLedOn);
                            char RewindLedOn[3] = {AddrMidiButtonLed, IdForward, 0x7F};
                            SendMidiOut(midiout, RewindLedOn);
                        }
                    }
                    //printf("Track Left: %i\n", AddrMidiTrack);
    //				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                    if(i_FlagNext == 1){
                        UpdateMidiControler();
                    }
                }
                else if (MidiCC == IdTrackPrev){                                                                                                         /*  Button for Track Prev  */

                    int i_FlagNext = 0;
                    NbMidiTrack = UIAllStrip/NbMidiFader;

                    if(MidiValue == 0x7F && AddrMidiTrack == 0){

                        if ( Lcd == 0 ){
                            char ForwardLedOn[3] = {AddrMidiButtonLed, IdRewind, 0x7F};
                            SendMidiOut(midiout, ForwardLedOn);
                            char RewindLedOn[3] = {AddrMidiButtonLed, IdForward, 0x00};
                            SendMidiOut(midiout, RewindLedOn);
                        }
                    }
                    else if(MidiValue == 0x7F && AddrMidiTrack > 0){
                        AddrMidiTrack--;
                        i_FlagNext = 1;

                        if( ShiftRightPressed == 1){
                            AddrMidiTrack = 0;

                            ShiftRightPressed = 0;

                            char ShiftRightLedOn[3] = {AddrMidiButtonLed, AddrShiftRight, 0x00};
                            SendMidiOut(midiout, ShiftRightLedOn);
                        }

                        sprintf(sa_LogMessage,"UI2MCP <-> MEM : Address Track : %i\n", AddrMidiTrack);
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        if ( Lcd == 0 ){
                            char ForwardLedOn[3] = {AddrMidiButtonLed, IdRewind, 0x7F};
                            SendMidiOut(midiout, ForwardLedOn);
                        }

                        if(AddrMidiTrack == 0){
                            if ( Lcd == 0 ){
                                char RewindLedOn[3] = {AddrMidiButtonLed, IdForward, 0x00};
                                SendMidiOut(midiout, RewindLedOn);
                            }
                        }
                    }
                    //printf("Track Left: %i\n", AddrMidiTrack);
    //				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                    if(i_FlagNext == 1){
                        UpdateMidiControler();
                    }
                }
                else if (MidiCC == AddrMidiParamButton){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if(MidiValue == 0x7F){

                        if (NbPanButton == 1 && SelectButtonPressed != -1){
                            if( strcmp(ui[SelectButtonPressed].Type, "i") == 0 || strcmp(ui[SelectButtonPressed].Type, "l") == 0 || strcmp(ui[SelectButtonPressed].Type, "p") == 0 || strcmp(ui[SelectButtonPressed].Type, "f") == 0 || strcmp(ui[SelectButtonPressed].Type, "s") == 0 ){
                                if( ui[SelectButtonPressed].StereoIndex == -1 ){
                                    char sendui[256];
                                    sprintf(sendui,"SETD^%s.%d.pan^0.5\n", ui[SelectButtonPressed].Type, ui[SelectButtonPressed].Numb);
                                    send(sock , sendui, strlen(sendui) , 0 );
                                    ui[SelectButtonPressed].PanMidi = 0.5;
                                }
                                else if( ui[SelectButtonPressed].StereoIndex == 0 ){
                                    char sendui[256];
                                    sprintf(sendui,"SETD^%s.%d.pan^0\n", ui[SelectButtonPressed].Type, ui[SelectButtonPressed].Numb);
                                    send(sock , sendui, strlen(sendui) , 0 );
                                    ui[SelectButtonPressed].PanMidi = 0;
                                }
                                else if( ui[SelectButtonPressed].StereoIndex == 1 ){
                                    char sendui[256];
                                    sprintf(sendui,"SETD^%s.%d.pan^1\n", ui[SelectButtonPressed].Type, ui[SelectButtonPressed].Numb);
                                    send(sock , sendui, strlen(sendui) , 0 );
                                    ui[SelectButtonPressed].PanMidi = 1;
                                }
                            }
                            else if( strcmp( ui[SelectButtonPressed].Type, "m" ) == 0 ){
                                char sendui[256];
                                sprintf(sendui,"SETD^m.pan^0.5\n");
                                send(sock , sendui, strlen(sendui) , 0 );
                                ui[SelectButtonPressed].PanMidi = 0.5;
                                if( ui[SelectButtonPressed].Numb == 0 ){ ui[SelectButtonPressed+1].PanMidi = ui[SelectButtonPressed].PanMidi; }
                                if( ui[SelectButtonPressed].Numb == 1 ){ ui[SelectButtonPressed-1].PanMidi = ui[SelectButtonPressed].PanMidi; }
                            }
                        }
                        UpdateMidiControler();
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrMuteClear){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if(MidiValue == 0x7F){

                        char sendui[256];
                        for (int j = 0; j < UIAllStrip; j++){
                            ui[j].MaskMute = 0;

                            ui[j].Mute = 0;
                            sprintf(sendui,"SETD^%s.%d.mute^0\n", ui[j].Type, ui[j].Numb);
                            send(sock , sendui, strlen(sendui) , 0 );

                            ui[j].ForceUnMute = 0;
                            sprintf(sendui,"SETD^%s.%d.forceunmute^0\n", ui[j].Type, ui[j].Numb);
                            send(sock , sendui, strlen(sendui) , 0 );
                        }

                       sprintf(sendui,"SETD^mgmask^0\n");
                       send(sock , sendui, strlen(sendui) , 0 );

                        char MidiArray[3] = {AddrMidiButtonLed, AddrMuteClear, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                        UpdateMidiControler();
                    }
                    else if(MidiValue == 0x00){
                        char MidiArray[3] = {AddrMidiButtonLed, AddrMuteClear, 0x00};
                        SendMidiOut(midiout, MidiArray);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrMuteSolo){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if(MidiValue == 0x7F){

                        char sendui[256];
                        for (int j = 0; j < UIAllStrip; j++){
                            ui[j].Solo = 0;
                            sprintf(sendui,"SETD^%s.%d.solo^0\n", ui[j].Type, ui[j].Numb);
                            send(sock , sendui, strlen(sendui) , 0 );
                        }
                        char MidiArray[3] = {AddrMidiButtonLed, AddrMuteSolo, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                        UpdateMidiControler();
                    }
                    else if(MidiValue == 0x00){
                        char MidiArray[3] = {AddrMidiButtonLed, AddrMuteSolo, 0x00};
                        SendMidiOut(midiout, MidiArray);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrMidiRec && NbRecButton == 1 && ShiftLeftPressed == 1 ){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if(MidiValue == 0x7F){

                        char ShiftLeftLedOn[3] = {AddrMidiButtonLed, AddrShiftLeft, 0x00};
                        SendMidiOut(midiout, ShiftLeftLedOn);

                        char RecLedOn[3] = {AddrMidiButtonLed, AddrMidiRec, 0x01};
                        SendMidiOut(midiout, RecLedOn);

                        ShiftLeftPressed = 0;

                        if( ArmRecAll == 0 ){
                            ArmRecAll = 1;
                        }
                        else if( ArmRecAll == 1 ){
                            ArmRecAll = 0;
                        }

                        for (int j = 0; j < UIAllStrip; j++){
                            if( (strcmp(ui[j].Type, "i") == 0 || strcmp(ui[j].Type, "l") == 0) && (j <= 19 || j >= 24) && UIChannel == 24 ){
                                ui[j].Rec = ArmRecAll;

                                char sendui[256];
                                sprintf(sendui,"SETD^%s.%d.mtkrec^%i\n", ui[j].Type, ui[j].Numb, ArmRecAll);
                                send(sock , sendui, strlen(sendui) , 0 );

                                //char MidiArray[3] = {AddrMidiButtonLed, MidiCC, ArmRecAll*0x7F};
                                //SendMidiOut(midiout, MidiArray);
                            }
                        }
                        UpdateMidiControler();
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrMidiRec && NbRecButton == 1 && ShiftLeftPressed == 0 && SelectButtonPressed != -1 ){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( (strcmp(ui[SelectButtonPressed].Type, "i") == 0 || strcmp(ui[SelectButtonPressed].Type, "l") == 0) && UIChannel == 24){

                        if(MidiValue == 0x7F && ui[SelectButtonPressed].Rec == 0 && (SelectButtonPressed <= 19 || SelectButtonPressed >= 24)){
                            ui[SelectButtonPressed].Rec = 1;
                            char sendui[256];
                            sprintf(sendui,"SETD^%s.%d.mtkrec^1\n", ui[SelectButtonPressed].Type, ui[SelectButtonPressed].Numb);
                            send(sock , sendui, strlen(sendui) , 0 );
                        }
                        else if(MidiValue == 0x7F && ui[SelectButtonPressed].Rec == 1 && (SelectButtonPressed <= 19 || SelectButtonPressed >= 24)){
                            ui[SelectButtonPressed].Rec = 0;
                            char sendui[256];
                            sprintf(sendui,"SETD^%s.%d.mtkrec^0\n", ui[SelectButtonPressed].Type, ui[SelectButtonPressed].Numb);
                            send(sock , sendui, strlen(sendui) , 0 );
                        }
                        UpdateMidiControler();
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrShiftLeft){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ShiftLeftPressed == 0){
                        ShiftLeftPressed = 1;

                        char ShiftLeftLedOn[3] = {AddrMidiButtonLed, AddrShiftLeft, 0x7F};
                        SendMidiOut(midiout, ShiftLeftLedOn);
                    }
                    else if( MidiValue == 0x7F && ShiftLeftPressed == 1){
                        ShiftLeftPressed = 0;

                        char ShiftLeftLedOn[3] = {AddrMidiButtonLed, AddrShiftLeft, 0x00};
                        SendMidiOut(midiout, ShiftLeftLedOn);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrShiftRight){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ShiftRightPressed == 0){
                        ShiftRightPressed = 1;

                        char ShiftRightLedOn[3] = {AddrMidiButtonLed, AddrShiftRight, 0x7F};
                        SendMidiOut(midiout, ShiftRightLedOn);
                    }
                    else if( MidiValue == 0x7F && ShiftRightPressed == 1){
                        ShiftRightPressed = 0;

                        char ShiftRightLedOn[3] = {AddrMidiButtonLed, AddrShiftRight, 0x00};
                        SendMidiOut(midiout, ShiftRightLedOn);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC >= AddrMidiSelect && MidiCC <= AddrMidiSelect + NbMidiFader - 1){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    Canal = MidiCC % AddrMidiSelect +(NbMidiFader*AddrMidiTrack);

                    if(MidiValue == 0x7F){

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Actual selected channel button %i\n",SelectButtonPressed);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);

                        char sendui[256];
                        sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                        send(sock , sendui, strlen(sendui) , 0 );

                        if( SelectButtonPressed == -1 || SelectButtonPressed != Canal ){
                            for (int j = NbMidiFader*AddrMidiTrack; j < (NbMidiFader*AddrMidiTrack)+NbMidiFader; j++){
                                if( ui[j].Color == 0 ){
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSelect+j-(NbMidiFader*AddrMidiTrack), 0x00};
                                    SendMidiOut(midiout, MidiArray);
                                }
                                else{
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSelect+j-(NbMidiFader*AddrMidiTrack), 0x7F};
                                    SendMidiOut(midiout, MidiArray);
                                }
                            }

                            SelectButtonPressed = Canal;

                            char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSelect+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), 0x01};
                            SendMidiOut(midiout, MidiArray);

                            if( ui[SelectButtonPressed].Color == 0 || ui[SelectButtonPressed].Color == 1 ){
                                char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrMidiSelect+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), 0x7F};
                                SendMidiOut(midiout, MidiArrayR);
                                char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrMidiSelect+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), 0x7F};
                                SendMidiOut(midiout, MidiArrayG);
                                char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrMidiSelect+SelectButtonPressed-(NbMidiFader*AddrMidiTrack), 0x7F};
                                SendMidiOut(midiout, MidiArrayB);
                            }
                        }
                        else if( SelectButtonPressed > -1 ){
                            SelectButtonPressed = -1;
                            if( ui[Canal].Color == 0 ){
                                char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack), 0x00};
                                SendMidiOut(midiout, MidiArray);
                            }
                            else{
                                char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSelect+Canal-(NbMidiFader*AddrMidiTrack), 0x7F};
                                SendMidiOut(midiout, MidiArray);
                            }
                        }
                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Now Selected channel button %i\n",SelectButtonPressed);
                        LogTrace(hfErr, 2, debug, sa_LogMessage);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == i_StopUI2Mcp){                                                                                                                     /*  TRANSPORT Rewind & Forward to Stop program  */

                    if(MidiValue == 0x7F){

                            char MidiArray[3] = {AddrMidiButtonLed, 0x5D, 0x7F};
                            SendMidiOut(midiout, MidiArray);
                            usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                            char MidiArray1[3] = {AddrMidiButtonLed, 0x5E, 0x7F};
                            SendMidiOut(midiout, MidiArray1);
                            usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                            char MidiArray2[3] = {AddrMidiButtonLed, 0x5F, 0x7F};
                            SendMidiOut(midiout, MidiArray2);
                            usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                            status = snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer));
                            MidiCC = (int)Midibuffer[1];
                            MidiValue = (int)Midibuffer[2];

                            if (MidiCC == i_Validation && MidiValue == 0x7F){
                                printf("Stop the Software\n");

                                char bpmon[3]  = {AddrMidiButtonLed, 0x56, 0x7F};
                                SendMidiOut(midiout, bpmon);

                                char MidiArray[3] = {AddrMidiButtonLed, 0x5D, 0x00};
                                SendMidiOut(midiout, MidiArray);

                                char MidiArray1[3] = {AddrMidiButtonLed, 0x5E, 0x00};
                                SendMidiOut(midiout, MidiArray1);

                                char MidiArray2[3] = {AddrMidiButtonLed, 0x5F, 0x00};
                                SendMidiOut(midiout, MidiArray2);

                                sleep(2);

                                break;
                            }
                            else{
                                char MidiArray[3] = {AddrMidiButtonLed, 0x5D, 0x00};
                                SendMidiOut(midiout, MidiArray);
                                usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                                char MidiArray1[3] = {AddrMidiButtonLed, 0x5E, 0x00};
                                SendMidiOut(midiout, MidiArray1);
                                usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

                                char MidiArray2[3] = {AddrMidiButtonLed, 0x5F, 0x00};
                                SendMidiOut(midiout, MidiArray2);
                                usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                                }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == IdStop){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if(MidiValue == 0x7F && MtkRecCurrentState == 0 && ModePlayer == 1 ){
                            MtkCurrentState = 0;

                            char sendui[256];
                            sprintf(sendui,"MTK_STOP\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Stop\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArray[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                            SendMidiOut(midiout, MidiArray);
                    }
                    else if(MidiValue == 0x7F && MediaRecCurrentState == 0 && ModePlayer == 0 ){
                            MediaPlayerCurrentState = 0;

                            char sendui[256];
                            sprintf(sendui,"MEDIA_STOP\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Media Stop\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArray[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                            SendMidiOut(midiout, MidiArray);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == IdPlay){                                                                                                                     /*  TRANSPORT PLAY button for Track view with Led  */

                    if( MidiValue == 0x7F && MtkRecCurrentState == 0 && ModePlayer == 1 ){
                        if(MtkCurrentState == 0){
                            MtkCurrentState = 2;

                            char sendui[256];
                            sprintf(sendui,"MTK_PLAY\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Play\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                            SendMidiOut(midiout, MidiArrayStop);
                            char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                            SendMidiOut(midiout, MidiArrayPlay);
                        }
                        else if(MtkCurrentState == 1){
                            MtkCurrentState = 2;

                            char sendui[256];
                            sprintf(sendui,"MTK_PLAY\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Play\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                            SendMidiOut(midiout, MidiArrayStop);
                            char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                            SendMidiOut(midiout, MidiArrayPlay);
                        }
                        else if(MtkCurrentState == 2){
                            MtkCurrentState = 1;

                            char sendui[256];
                            sprintf(sendui,"MTK_PAUSE\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Pause\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                            SendMidiOut(midiout, MidiArrayStop);
                            char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x01};
                            SendMidiOut(midiout, MidiArrayPlay);
                        }
                    }
                    else if( MidiValue == 0x7F && MediaRecCurrentState == 0 && ModePlayer == 0 ){
                        if(MediaPlayerCurrentState == 0 ){
                            MediaPlayerCurrentState = 2;

                            char sendui[256];
                            sprintf(sendui,"MEDIA_PLAY\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MEDIA Player Play\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                            SendMidiOut(midiout, MidiArrayStop);
                            char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                            SendMidiOut(midiout, MidiArrayPlay);
                        }
                        else if(MediaPlayerCurrentState == 3){
                            MediaPlayerCurrentState = 2;

                            char sendui[256];
                            sprintf(sendui,"MEDIA_PLAY\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MEDIA Player Play\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                            SendMidiOut(midiout, MidiArrayStop);
                            char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x7F};
                            SendMidiOut(midiout, MidiArrayPlay);
                        }
                        else if(MediaPlayerCurrentState == 2){
                            MediaPlayerCurrentState = 3;

                            char sendui[256];
                            sprintf(sendui,"MEDIA_PAUSE\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MEDIA Player Pause\n");
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                            SendMidiOut(midiout, MidiArrayStop);
                            char MidiArrayPlay[3] = {AddrMidiButtonLed, IdPlay, 0x01};
                            SendMidiOut(midiout, MidiArrayPlay);
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == IdRec){                                                                                                                     /*  TRANSPORT REC button for Track view with Led  */

                    printf("Debug %i %i %i\n", MediaRecCurrentState, MtkRecCurrentState, ModePlayer);

//                    if( MidiValue == 0x7F && MtkRecCurrentState == 0 && MtkCurrentState == 0 && ModePlayer == 1 ){
                    if( MidiValue == 0x7F && MtkRecCurrentState == 0 && ModePlayer == 1 ){
                        MtkRecCurrentState = 1;
                        char sendui[256];
                        sprintf(sendui,"MTK_REC_TOGGLE\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Record ON\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                    }
                    else if( MidiValue == 0x7F && MtkRecCurrentState == 1 && ModePlayer == 1 ){
                        MtkRecCurrentState = 0;
                        char sendui[256];
                        sprintf(sendui,"MTK_REC_TOGGLE\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Record OFF\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);
                    }

//                    if( MidiValue == 0x7F && MediaRecCurrentState == 0 && MediaPlayerCurrentState == 0 && ModePlayer == 0 ){
                    if( MidiValue == 0x7F && MediaRecCurrentState == 0 && ModePlayer == 0 ){
                        MediaRecCurrentState = 1;
                        char sendui[256];
                        sprintf(sendui,"RECTOGGLE\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MEDIA Record ON\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x00};
                        SendMidiOut(midiout, MidiArrayStop);
                        char MidiArrayRec[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArrayRec);

                    }
                    else if( MidiValue == 0x7F && MediaRecCurrentState == 1 && ModePlayer == 0 ){
                        MediaRecCurrentState = 0;
                        char sendui[256];
                        sprintf(sendui,"RECTOGGLE\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MEDIA Record OFF\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArrayStop[3] = {AddrMidiButtonLed, IdStop, 0x7F};
                        SendMidiOut(midiout, MidiArrayStop);
                        char MidiArrayRec[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArrayRec);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == i_Dim){                                                                                                                     /*  DIM Master  */

                    if(MidiValue == 0x7F && DimMaster ==0){
                        DimMaster = 1;
                        char sendui[256];
                        sprintf(sendui,"SETD^m.dim^1\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        LogTrace(hfErr, 2, debug, "UI2MCP --> UI : DIM On\n");

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                        char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, (int)floor((double)127/(double)255*0)};
                        SendMidiOut(midiout, MidiArrayR);
                        char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, (int)floor((double)127/(double)255*145)};
                        SendMidiOut(midiout, MidiArrayG);
                        char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, (int)floor((double)127/(double)255*194)};
                        SendMidiOut(midiout, MidiArrayB);
                    }
                    else if(MidiValue == 0x7F && DimMaster ==1){
                        DimMaster = 0;
                        char sendui[256];
                        sprintf(sendui,"SETD^m.dim^0\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        LogTrace(hfErr, 2, debug, "UI2MCP --> UI : DIM Off\n");

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == i_SnapShotNavUp){                                                                                                                     /*  Snapshot Navigation up  */

//                    if(MidiValue == 0x7F){
//                        printf("Marker Left (SnapShot Nagigation)\n");
//
//                        if(SnapShotIndex > 0){
//                            SnapShotIndex--;
//                        }
//                        printf("SnapShot(%i/%i) = [%s]\n", SnapShotIndex, SnapShotMax, UISnapShotList[SnapShotIndex]);
//
//                        char sendui[256];
//                        sprintf(sendui,"LOADSNAPSHOT^Studio Stephan^%s\n", UISnapShotList[SnapShotIndex]);
//                        send(sock , sendui, strlen(sendui) , 0 );
//
//                        sprintf(sendui,"MSG^$SNAPLOAD^%s\n", UISnapShotList[SnapShotIndex]);
//                        send(sock , sendui, strlen(sendui) , 0 );
//
//                    }
//                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == i_SnapShotNavDown){                                                                                                                     /*  Snapshot navigation down  */

//                    if(MidiValue == 0x7F){
//                        printf("Marker Right (SnapShot Nagigation)\n");
//
//                        if(SnapShotIndex < SnapShotMax){
//                            SnapShotIndex++;
//                        }
//                        printf("SnapShot(%i/%i) = [%s]\n", SnapShotIndex, SnapShotMax, UISnapShotList[SnapShotIndex]);
//
//                        char sendui[256];
//                        sprintf(sendui,"LOADSNAPSHOT^Studio Stephan^%s\n", UISnapShotList[SnapShotIndex]);
//                        send(sock , sendui, strlen(sendui) , 0 );
//
//                        sprintf(sendui,"MSG^$SNAPLOAD^%s\n", UISnapShotList[SnapShotIndex]);
//                        send(sock , sendui, strlen(sendui) , 0 );
//
//                    }
//                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrMidiSessionButton){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F ){

                        if( ModeSnapShotsPressed == 1){
                            char sendui[256];
                            sprintf(sendui,"LOADSNAPSHOT^%s^%s\n", ShowsCurrent, UISnapShotList[SnapShotIndex]);
                            send(sock , sendui, strlen(sendui) , 0 );

                            //sprintf(sendui,"MSG^$SNAPLOAD^%s\n", UISnapShotList[SnapShotIndex]);
                            //send(sock , sendui, strlen(sendui) , 0 );

                            if ( Lcd == 1 ){
                                ConfigLCDMixerMode();

                                UpdateMidiControler();
                            }

                            char MidiArray[3] = {AddrMidiButtonLed, AddrSnapShotsSelect, 0x00};
                            SendMidiOut(midiout, MidiArray);

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Snapshot load : %s\n", UISnapShotList[SnapShotIndex]);
                            LogTrace(hfErr, 1, debug, sa_LogMessage);
                        }
                        else if( ModeShowsPressed == 1){

                            strcpy(ShowsCurrent, UIShowsList[ShowsIndex]);
                            printf("Current Selected Shows : %s\n", ShowsCurrent);

                            char sendui[256];
                            sprintf(sendui,"SNAPSHOTLIST^%s\n", ShowsCurrent);
                            send(sock , sendui, strlen(sendui) , 0 );

                            if ( Lcd == 1 ){
                                ConfigLCDMixerMode();

                                UpdateMidiControler();
                            }

                            char MidiArray[3] = {AddrMidiButtonLed, AddrShowsSelect, 0x00};
                            SendMidiOut(midiout, MidiArray);

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Shows load : %s\n", UIShowsList[ShowsIndex]);
                            LogTrace(hfErr, 1, debug, sa_LogMessage);
                        }
                        else if( ModeDirPlayerPressed == 1){

                            strcpy(DirPlayerCurrent, UIDirPlayerList[DirPlayerIndex]);
                            printf("Current Selected Directory : %s\n", DirPlayerCurrent);

                            char sendui[256];
                            sprintf(sendui,"MEDIA_GET_PLIST_TRACKS^%s\n", DirPlayerCurrent);
                            send(sock , sendui, strlen(sendui) , 0 );

                            ModeDirPlayerPressed = 0;

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Dir Player load : %s\n", UIDirPlayerList[DirPlayerIndex]);
                            LogTrace(hfErr, 1, debug, sa_LogMessage);

                            sprintf(sendui,"PLIST_TRACKS^%s\n", FilesPlayerCurrent);
                            send(sock , sendui, strlen(sendui) , 0 );

                            ModeFilesPlayerPressed = 1;

                        }
                        else if( ModeFilesPlayerPressed == 1){

                            strcpy(FilesPlayerCurrent, UIFilesPlayerList[FilesPlayerIndex]);
                            printf("Current Selected File : %s\n", FilesPlayerCurrent);

                            char sendui[256];
                            sprintf(sendui,"MEDIA_SWITCH_TRACK^%s^%s\n", DirPlayerCurrent, FilesPlayerCurrent);
                            send(sock , sendui, strlen(sendui) , 0 );
                            sprintf(sendui,"MEDIA_STOP\n");
                            send(sock , sendui, strlen(sendui) , 0 );

                            ModeFilesPlayerPressed  = 0;

                            if ( Lcd == 1 ){
                                ConfigLCDMixerMode();

                                UpdateMidiControler();
                            }

                            char MidiArray[3] = {AddrMidiButtonLed, AddrMediaSelect, 0x00};
                            SendMidiOut(midiout, MidiArray);

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : File Player load : %s\n", UIFilesPlayerList[FilesPlayerIndex]);
                            LogTrace(hfErr, 1, debug, sa_LogMessage);
                        }
                        else if( ModeMtkSessionsPressed == 1){

                            strcpy(MtkSessionCurrent, UIMtkSessionList[MtkSessionIndex]);
                            printf("Current Selected MTK Session : %s\n", MtkSessionCurrent);

                            char sendui[256];
                            sprintf(sendui,"MTK_SELECT^%s\n", MtkSessionCurrent);
                            send(sock , sendui, strlen(sendui) , 0 );

                            ModeMtkSessionsPressed  = 0;

                            if ( Lcd == 1 ){
                                ConfigLCDMixerMode();

                                UpdateMidiControler();
                            }

                            char MidiArray[3] = {AddrMidiButtonLed, AddrSessionSelect, 0x00};
                            SendMidiOut(midiout, MidiArray);

                            sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Session load : %s\n", UIMtkSessionList[MtkSessionIndex]);
                            LogTrace(hfErr, 1, debug, sa_LogMessage);
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrSessionSelect){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ModeMtkSessionsPressed == 0 ){
                        ModeMtkSessionsPressed = 1;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Session Mode Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, AddrSessionSelect, 0x7F};
                        SendMidiOut(midiout, MidiArray);
                        char sendui[256];
                        sprintf(sendui,"MTK_GET_SESSIONS\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        if ( Lcd == 1 ){
                            ConfigLCDTxtMode();
                            LcdExplorerUpdate(midiout, SysExHdr, MtkSessionIndex, &UIMtkSessionList);
                        }
                    }
                    else if(MidiValue == 0x7F && ModeMtkSessionsPressed == 1){
                        ModeMtkSessionsPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : MTK Session Mode Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, AddrSessionSelect, 0x00};
                        SendMidiOut(midiout, MidiArray);
                        if ( Lcd == 1 ){
                            ConfigLCDMixerMode();

                            UpdateMidiControler();
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrMediaSelect){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ModeDirPlayerPressed == 0 && ModeFilesPlayerPressed == 0){
                        ModeDirPlayerPressed = 1;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Dir Mode Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrMediaSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayPlayer);
                        // char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayR);
                        // char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        // SendMidiOut(midiout, MidiArrayG);
                        // char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayB);
                        char sendui[256];
                        sprintf(sendui,"MEDIA_GET_PLISTS\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        if ( Lcd == 1 ){
                            ConfigLCDTxtMode();
                            LcdExplorerUpdate(midiout, SysExHdr, DirPlayerIndex, &UIDirPlayerList);
                        }
                    }
                    else if(MidiValue == 0x7F && ModeDirPlayerPressed == 1 && ModeFilesPlayerPressed == 0){
                        ModeDirPlayerPressed = 0;
                        ModeFilesPlayerPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Dir Mode Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, AddrMediaSelect, 0x00};
                        SendMidiOut(midiout, MidiArray);
                        // char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayR);
                        // char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        // SendMidiOut(midiout, MidiArrayG);
                        // char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayB);
                        if ( Lcd == 1 ){
                            ConfigLCDMixerMode();

                            UpdateMidiControler();
                        }
                    }
                    else if(MidiValue == 0x7F && ModeDirPlayerPressed == 0 && ModeFilesPlayerPressed == 1){
                        ModeDirPlayerPressed = 0;
                        ModeFilesPlayerPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Dir Mode Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, AddrMediaSelect, 0x00};
                        SendMidiOut(midiout, MidiArray);
                        // char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayR);
                        // char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        // SendMidiOut(midiout, MidiArrayG);
                        // char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayB);
                        if ( Lcd == 1 ){
                            ConfigLCDMixerMode();

                            UpdateMidiControler();
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrTransportModeSelect){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ModePlayer == 0 ){
                        ModePlayer = 1;

                        printf("Debug Transport %i %i %i\n", MediaRecCurrentState, MtkRecCurrentState, ModePlayer);

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArrayPlayer[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayPlayer);
                        char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayR);
                        char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayG);
                        char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayB);

                    }
                    else if(MidiValue == 0x7F && ModePlayer == 1){
                        ModePlayer = 0;

                        printf("Debug Transport %i %i %i\n", MediaRecCurrentState, MtkRecCurrentState, ModePlayer);

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Player Mode %i\n", ModePlayer);
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArray);
                        char MidiArrayR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayR);
                        char MidiArrayG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                        SendMidiOut(midiout, MidiArrayG);
                        char MidiArrayB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                        SendMidiOut(midiout, MidiArrayB);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == IdRewind){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F ){

                        char MidiArray[3] = {AddrMidiButtonLed, IdRewind, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                        command = "MEDIA_PREV\n";
                        send(sock , command, strlen(command) , 0 );
                        command = "MEDIA_STOP\n";
                        send(sock , command, strlen(command) , 0 );
                    }
                    else if( MidiValue == 0x00 ){

                        char MidiArray[3] = {AddrMidiButtonLed, IdRewind, 0x00};
                        SendMidiOut(midiout, MidiArray);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == IdForward){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F ){

                        char MidiArray[3] = {AddrMidiButtonLed, IdForward, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                        command = "MEDIA_NEXT\n";
                        send(sock , command, strlen(command) , 0 );
                        command = "MEDIA_STOP\n";
                        send(sock , command, strlen(command) , 0 );
                    }
                    else if( MidiValue == 0x00 ){

                        char MidiArray[3] = {AddrMidiButtonLed, IdForward, 0x00};
                        SendMidiOut(midiout, MidiArray);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrShowsSelect){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ModeShowsPressed == 0 ){
                        ModeShowsPressed = 1;
                        ModeSnapShotsPressed = 0;
                        ModeCuesPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Shows Mode Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArrayShows[3] = {AddrMidiButtonLed, AddrShowsSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayShows);
                        char MidiArraySnaps[3] = {AddrMidiButtonLed, AddrSnapShotsSelect, 0x00};
                        SendMidiOut(midiout, MidiArraySnaps);
                        char MidiArrayCues[3] = {AddrMidiButtonLed, AddrCuesSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayCues);
                        // char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayR);
                        // char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        // SendMidiOut(midiout, MidiArrayG);
                        // char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayB);
                        if ( Lcd == 1 ){
                            ConfigLCDTxtMode();
                            LcdExplorerUpdate(midiout, SysExHdr, ShowsIndex, &UIShowsList);
                        }
                    }
                    else if(MidiValue == 0x7F && ModeShowsPressed ==1){
                        ModeShowsPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Shows Mode Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);
                        // char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayR);
                        // char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        // SendMidiOut(midiout, MidiArrayG);
                        // char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        // SendMidiOut(midiout, MidiArrayB);
                        if ( Lcd == 1 ){
                            ConfigLCDMixerMode();

                            UpdateMidiControler();
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrSnapShotsSelect){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ModeSnapShotsPressed == 0 ){
                        ModeSnapShotsPressed = 1;
                        ModeShowsPressed = 0;
                        ModeCuesPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : SnapShots Mode Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArrayShows[3] = {AddrMidiButtonLed, AddrShowsSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayShows);
                        char MidiArraySnaps[3] = {AddrMidiButtonLed, AddrSnapShotsSelect, 0x7F};
                        SendMidiOut(midiout, MidiArraySnaps);
                        char MidiArrayCues[3] = {AddrMidiButtonLed, AddrCuesSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayCues);
                        //char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        //SendMidiOut(midiout, MidiArrayR);
                        //char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        //SendMidiOut(midiout, MidiArrayG);
                        //char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        //SendMidiOut(midiout, MidiArrayB);
                        if ( Lcd == 1 ){
                            ConfigLCDTxtMode();
                            LcdExplorerUpdate(midiout, SysExHdr, SnapShotIndex, &UISnapShotList);
                        }
                    }
                    else if(MidiValue == 0x7F && ModeSnapShotsPressed ==1){
                        ModeSnapShotsPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : SnapShots Mode Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);
                        //char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        //SendMidiOut(midiout, MidiArrayR);
                        //char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        //SendMidiOut(midiout, MidiArrayG);
                        //char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        //SendMidiOut(midiout, MidiArrayB);

                        if ( Lcd == 1 ){
                            ConfigLCDMixerMode();

                            UpdateMidiControler();
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrCuesSelect){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ModeCuesPressed == 0 ){
                        ModeCuesPressed = 1;
                        ModeShowsPressed = 0;
                        ModeSnapShotsPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Cues Mode Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArrayShows[3] = {AddrMidiButtonLed, AddrShowsSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayShows);
                        char MidiArraySnaps[3] = {AddrMidiButtonLed, AddrSnapShotsSelect, 0x00};
                        SendMidiOut(midiout, MidiArraySnaps);
                        char MidiArrayCues[3] = {AddrMidiButtonLed, AddrCuesSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayCues);
//                        char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayR);
//                        char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
//                        SendMidiOut(midiout, MidiArrayG);
//                        char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayB);
                        if ( Lcd == 1 ){
                            ConfigLCDTxtMode();
                            LcdExplorerUpdate(midiout, SysExHdr, CuesIndex, &UICuesList);
                        }
                    }
                    else if(MidiValue == 0x7F && ModeCuesPressed ==1){
                        ModeCuesPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Cues Mode Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);
//                        char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayR);
//                        char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
//                        SendMidiOut(midiout, MidiArrayG);
//                        char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayB);

                        if ( Lcd == 1 ){
                            ConfigLCDMixerMode();

                            UpdateMidiControler();
                        }
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrPanSelect){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && PanPressed == 0 ){
                        PanPressed = 1;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Pan Mode Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);
//                        char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayR);
//                        char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
//                        SendMidiOut(midiout, MidiArrayG);
//                        char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayB);
                    }
                    else if(MidiValue == 0x7F && PanPressed ==1){
                        PanPressed = 0;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Pan Mode Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);
//                        char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayR);
//                        char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
//                        SendMidiOut(midiout, MidiArrayG);
//                        char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
//                        SendMidiOut(midiout, MidiArrayB);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrMidiMaster){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

                    if( MidiValue == 0x7F && ModeMasterPressed == 0 ){
                        ModeMasterPressed = 1;
                        NbMidiFader--;

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Mode Master Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);

                        UpdateMidiControler();
                    }
                    else if(MidiValue == 0x7F && ModeMasterPressed ==1){
                        ModeMasterPressed = 0;
                        NbMidiFader = atoi(ControlerConfig.NbMidiFader);

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Mode Master Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);

                        if( AddrMidiTrack*NbMidiFader+1 > UIAllStrip ){
                            AddrMidiTrack--;
                            NbMidiTrack = UIAllStrip/NbMidiFader;
                        }

                        UpdateMidiControler();
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else if (MidiCC == AddrSoundCheck){                                                                                                                     /*  DIM Master  */

                    if( MidiValue == 0x7F && SoundCheck == 0 ){
                        SoundCheck = 1;

                        char sendui[256];
                        sprintf(sendui,"SETD^var.mtk.soundcheck^1\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Sound Check Actived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);
                        char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArrayR);
                        char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArrayG);
                        char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArrayB);

                        ModePlayer = 1;

                        char MidiArrayM[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayM);
                        char MidiArrayMR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayMR);
                        char MidiArrayMG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayMG);
                        char MidiArrayMB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x00};
                        SendMidiOut(midiout, MidiArrayMB);

                    }
                    else if(MidiValue == 0x7F && SoundCheck ==1){
                        SoundCheck = 0;

                        char sendui[256];
                        sprintf(sendui,"SETD^var.mtk.soundcheck^0\n");
                        send(sock , sendui, strlen(sendui) , 0 );

                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Sound Check Desactived\n");
                        LogTrace(hfErr, 1, debug, sa_LogMessage);

                        char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArray);
                        char MidiArrayR[3] = {AddrMidiButtonLed+1, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArrayR);
                        char MidiArrayG[3] = {AddrMidiButtonLed+2, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArrayG);
                        char MidiArrayB[3] = {AddrMidiButtonLed+3, MidiCC, 0x00};
                        SendMidiOut(midiout, MidiArrayB);

                        ModePlayer = 0;

                        char MidiArrayM[3] = {AddrMidiButtonLed, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayM);
                        char MidiArrayMR[3] = {AddrMidiButtonLed+1, AddrTransportModeSelect, 0x7F};
                        SendMidiOut(midiout, MidiArrayMR);
                        char MidiArrayMG[3] = {AddrMidiButtonLed+2, AddrTransportModeSelect, 0x0A};
                        SendMidiOut(midiout, MidiArrayMG);
                        char MidiArrayMB[3] = {AddrMidiButtonLed+3, AddrTransportModeSelect, 0x49};
                        SendMidiOut(midiout, MidiArrayMB);
                    }
                    usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
                }
                else{
                }
            }
            else{
            }
        }
     }while(!stop);

    /* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
    /* End of the program */
    /* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */

     /*  Reset Button light on Midi Controler  */
     for (int j = 0; j < 0xFF; j++){
        char MidiArray[3] = {AddrMidiButtonLed, j, 0x00};
        SendMidiOut(midiout, MidiArray);
     }

     /*  Reset Mix button on Midi Controler  */
     for (int j = 0; j < NbMidiFader; j++){
        int MidiValue = 0;
        char MidiArray[3] = {AddrMidiMix+j, MidiValue, MidiValue};
        SendMidiOut(midiout, MidiArray);
     }

     /*  Reset LCD on Midi Controler  */
     if ( Lcd == 1 ){
            for (int j = 0; j < NbMidiFader; j++){
            char Cmd[32] = "";
            char c_Canal[32] = "";

            strcpy(Cmd, "13");
            sprintf(c_Canal, "%02X", j);
            strcat(Cmd, c_Canal);
            strcat(Cmd, "10");
            SendSysExTextOut(midiout, SysExHdr, Cmd, "");

            char BarSet[3] = {0xB0, 0x38+j, 0x04};
            SendMidiOut(midiout, BarSet);

         }
     }

     LogTrace(hfErr, 0, debug, "UI2MCP --> UI : IOSYS^Disconnexion to UI\n");
     command = "IOSYS^Disconnexion UI2MCP\n";
     send(sock , command, strlen(command) , 0 );

     LogTrace(hfErr, 0, debug, "UI2MCP <-> Close MIDI socket\n");
     snd_rawmidi_close(midiout);
     snd_rawmidi_close(midiin);
     midiout = NULL;   // snd_rawmidi_close() does not clear invalid pointer,
     midiin = NULL;     // snd_rawmidi_close() does not clear invalid pointer,

     char sendui[256];
     sprintf(sendui,"QUIT\n");
     send(sock , sendui, strlen(sendui) , 0 );
     LogTrace(hfErr, 0, debug, "UI2MCP --> Ui : Quit\n");

     LogTrace(hfErr, 0, debug, "UI2MCP <-> Close HTTP socket\n");
     close(sock);

     /*  End of the program  */
     LogTrace(hfErr, 0, debug, "UI2MCP <-> End\n");
     return EXIT_SUCCESS;

}
