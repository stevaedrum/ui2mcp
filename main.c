//
// Programmer:    Stephan Allene <stephan.allene]gmail.com>
// Filename:      alsarawmidiout.c
// Syntax:        C; ALSA 1.0
// $Smake:      gcc -o %b %f -lasound
//

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
#define FILENAME "config.conf"
#define MAXBUF 1024
#define DELIM "="

unsigned short stop = 0;                                                        // Flag of program stop, default value is 0.
FILE *hfErr;                                                                            // Declaration for log file.
char sa_LogMessage[262145] = "";                                        // Variable for log message.

static char *send_hex;                                                            // Variable for SysEx message in hexa.
static char *send_data;                                                           // Variable for SysEx message.
static int send_data_length;                                                    // Variable for len of SysEx message.

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

void LogTrace(FILE *p_File, int debug, char *p_Trace, ...){
//void LogTrace(FILE *p_File, int debug, char *p_Trace){
    //char sa_Buffer[80];
    //va_list arg;
    //va_start(arg,pszTrace);
    //vsprintf(szBuffer,pszTrace,arg);
    //sprintf(sa_Buffer,p_Trace);
    if(debug == 0){
        //printf("%s",p_Trace);
    }
    else if (debug ==1){
        fprintf(p_File,"%s", p_Trace);
    }
    //va_end(arg);
    return ;
}

/*  give ime in miliseconds.  */

long currentTimeMillis() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

/*  split char with demiliter like Perl function.  */

char** split(char* chaine,const char* delim,int vide){

    char** tab=NULL;                    //tableau de chaine, tableau resultat
    char *ptr;                     //pointeur sur une partie de
    int sizeStr;                   //taille de la chaine à recupérer
    int sizeTab=0;                 //taille du tableau de chaine
    char* largestring;             //chaine à traiter

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

/*  Memory allocation.  */

static void *my_malloc(size_t size)
{
	void *p = malloc(size);
	if (!p) {
		errormessage("out of memory");
		exit(EXIT_FAILURE);
	}
	return p;
}

/*  Send SysEx function.  */

static int send_midi_interleaved(void)
{
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
				usleep((buffer_size - snd_rawmidi_status_get_avail(st)));
				//usleep((buffer_size - snd_rawmidi_status_get_avail(st)) * 320);
				snd_rawmidi_status(midiout, st);
			} while(snd_rawmidi_status_get_avail(st) < buffer_size);
			//usleep(320);
			//usleep(1000);
			//usleep(sysex_interval * 1000);
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

/*  Convert char to hexa value.  */

static int hex_value(char c)
{
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

static void parse_data(void)
{
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

static void add_send_hex_data(const char *str)
{
	int length;
	char *s;

	length = (send_hex ? strlen(send_hex) + 1 : 0) + strlen(str) + 1;
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
	char Buff[255];

    unsigned BuffH[200];
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
	if ((status = send_midi_interleaved()) < 0) {
		errormessage("cannot send data: %s", snd_strerror(status));
		exit(1);
	}

}

/*  Send midi out message.  */

void SendMidiOut(snd_rawmidi_t *hwMidi, char *MidiOut){
	int status;
	if ((status = snd_rawmidi_write(hwMidi, MidiOut, sizeof(MidiOut))) < 0){
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
/* main code */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {

    // Main variable
	//char *version = "1.0.0";
	int i,j = 0;
	int status = 0;
	int current_time_init;
	int current_time;

	int i_Watchdog_init;
	int i_Watchdog;

	int i_VuMeter_init;
	int i_VuMeter;

	//int i_StxBuffer = 0;
    char  c_StxBuffer[1024] = "";

	int init = 0;
	int debug = 0;

	// Midi connexion  variable
	const char* portname = "hw:1,0,0";                                      // Default portname
	int mode = SND_RAWMIDI_NONBLOCK;                                // SND_RAWMIDI_SYNC
	//snd_rawmidi_t* midiout = NULL;
	//snd_rawmidi_t* midiin = NULL;
	unsigned char Midibuffer[2048] = "";                                   // Storage for input buffer received
	int InMidi, MidiValue, MidiCC;

    // Network websocket
	struct sockaddr_in address;
    int sock = 0;
    struct sockaddr_in serv_addr;
	char *ack = "GET /raw HTTP1.1\n\n";
	char *command = NULL;
    char buffer[1024] = "";
    //char buffer[262144] = "";

 	int AdrSplitComa, ArraySplitDot;
	char** SplitArrayComa = NULL;
	char** SplitArrayDot = NULL;

	// UI device variable
	//char  UIAddress = "192.168.0.10";
	int UIChannel = 24;
	int UIMedia = 2;
	int UISubGroup = 6;
	int UIFx = 4;
	int UIAux = 10;
	int UIMaster = 2;
	int UILineIn = 2;

	// student structure variable
	struct UiI ui_i[UIChannel];
	struct UiI ui_media[UIMedia];
	struct UiSubFx ui_s[UISubGroup];
	struct UiSubFx ui_f[UIFx];
	struct UiAux ui_a[UIAux];
	struct UiMaster ui_master[UIMaster];
	struct UiI ui_l[UILineIn];


    for(int c = 0; c < UIChannel; c++){ui_i[c].Color = 0;}
    for(int c = 0; c < UILineIn; c++){ui_l[c].Color = 3;}
    for(int c = 0; c < UIMedia; c++){ui_media[c].Color = 11;}
    for(int c = 0; c < UISubGroup; c++){ui_s[c].Color  = 8;}
    for(int c = 0; c < UIFx; c++){ui_f[c].Color = 7;}
    for(int c = 0; c < UIAux; c++){ui_a[c].Color = 5;}
    for(int c = 0; c < UIMaster; c++){ui_master[c].Color = 2;}

	char UImsg[64] = "";
	char UIio[64] = "";
	char UIfunc[64] = "";
	char UIval[64] = "";
	char UIchan[24] = "";

	//float zeroDbPos = .7647058823529421;   //   0 db
	//float maxDb = 1;					                    // +10 db

	char c_SyncId[256] = "Stevae";
	char *UIModel = NULL;
	char *UICommand = NULL;
	//char *uisnapshot = "";

	//UI MuteMask
	char UIMuteMask[16] = "";
	unsigned GroupMaskMute[6];

	//UI SnapShot
	char UIShow[256] = "";                              //"Studio Stephan";
	char UISnapShotList [200][256];
	int SnapShotIndex = 0;
	int SnapShotMax = 0;
	char SnapShotCurrent[256] = "";

	//int UiChannelsolo[24];
	//int UiChannelmute[24];
	//int UiChannelpan[24];
	//int UiChannelmix[24];
	int UIBpm = 120;
	int delay = 60000/UIBpm;

	// Common variable
	int AddrMidiTrack = 0;	                                             // value of translation fader
	int Canal;
	int MidiTable [UIChannel][7];	            	// Array of Mix, Solo, Mute, Rec, MaskMute, MaskMuteValue
	//int Mix = 1; 					                        	// Fader MIX //MidiTable [Fader][1] = Mix Potentiometer
	int Solo = 2;			                        			// Solo button //MidiTable [Fader][2] = Solo Button
	int Mute = 3; 		                        				// Mute button //MidiTable [Fader][3] = Mute Button
	int ForceUnMute = 4; 		                        		// ForceMute button //MidiTable [Fader][4] = Force Mute Button
	int Rec = 5;					                            	// Rec button //MidiTable [Fader][5] = Rec Button
	int MaskMute = 6;			                    		// Mute button //MidiTable [Fader][6] = Mute Button with UI Mask
	int MaskMuteValue = 7;		                	// Mute button //MidiTable [Fader][7] = Mute Button with UI Mask

	//int MtkStop;						                        // Value for Mtk STOP
	int MtkPlay;					                        	// Value for Mtk PLAY
	int MtkRec;					                            	// Value for Mtk REC
	int DimMaster;				                    		// Value for Dim Master
	double PanMidi[UIChannel];		            	// Pan //MidiTable = Pan potentiometer 0=left 0.5=center 1=right
	double MixMidi[UIChannel];		            	// Mix //MidiTable = Fader potentiometer 0 to 1
	char NameChannel[UIChannel][256];                    // Name of the channel on UIx for Midi LCD pannel.
    //int i_OrMuteMaskMute[UIChannel];

	// Parameter of MIDI device
    //int MidiValueOn = 0x7F;
	//int MidiValueOff = 0x00;

    struct config ControlerConfig;
    ControlerConfig = get_config(FILENAME);

	// Parameter of MIDI device
	char *ControlerName = ControlerConfig.ControlerName;
	char *ControlerMode = ControlerConfig.ControlerMode;
	int Lcd = atoi(ControlerConfig.Lcd);
	int NbMidiFader = atoi(ControlerConfig.NbMidiFader);
	int NbMidiTrack = UIChannel/NbMidiFader;   // Number by modulo of number of Midi channel
	//mapping midi controler to UI for fader 0

	// Fader = Ex ll hh
	int AddrMidiMix = 0;
    sscanf(ControlerConfig.AddrMidiMix, "%x", &AddrMidiMix);

	// Encoder = B0 10 xx
	int AddrMidiEncoderPan = 0; //0xB0;
    sscanf(ControlerConfig.AddrMidiEncoderPan, "%x", &AddrMidiEncoderPan);
        int AddrMidiPan = 0; //0x10;
        sscanf(ControlerConfig.AddrMidiPan, "%x", &AddrMidiPan);

	// Button/Led 90 ID CC
	int AddrMidiButtonLed = 0; //0x90;
    sscanf(ControlerConfig.AddrMidiButtonLed, "%x", &AddrMidiButtonLed);
        int AddrMidiRec = 0; //0x00;
        sscanf(ControlerConfig.AddrMidiRec, "%x", &AddrMidiRec);
        int AddrMidiMute = 0; //0x10;
        sscanf(ControlerConfig.AddrMidiMute, "%x", &AddrMidiMute);
        int AddrMidiSolo = 0; //0x08;
        sscanf(ControlerConfig.AddrMidiSolo, "%x", &AddrMidiSolo);

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
    int i_ConfirmStopUI2Mcp = 0; //IdForward;
    sscanf(ControlerConfig.i_ConfirmStopUI2Mcp, "%x", &i_ConfirmStopUI2Mcp);

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

	if ((argc > 1) && (strncmp("hw:", argv[1], 3) == 0)){
      portname = argv[1];
	}

     //ouverture du fichier de log ou creation
     if ((hfErr = fopen("ui2mcp.log","w" )) == (FILE *)NULL){
          errormessage("\nError opening log file\n");
          exit(1);
     }

    LogTrace(hfErr, debug, "Starting to MCP Midi Controler...\n" );
    printf("Starting to MCP Midi Controler...\n");
    sprintf(sa_LogMessage,"Controler Midi : %s[%s]\n", ControlerName, ControlerMode);
    LogTrace(hfErr, debug, sa_LogMessage);
    printf("%s\n", sa_LogMessage);


	// Connection MIDI device
	if ((status = snd_rawmidi_open(&midiin, &midiout, portname, mode)) < 0){
      errormessage("\nProblem opening MIDI input & output: %s\n", snd_strerror(status));
      exit(1);
	}

	// Connection socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        errormessage("\n Socket creation error \n");
        exit(1);
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    //serv_addr.sin_addr.s_addr = "192.168.0.10";
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "192.168.0.10", &serv_addr.sin_addr)<=0)
    {
        errormessage("\nInvalid address/ Address not supported \n");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        errormessage("\nConnection Failed \n");
        exit(1);
    }

    send(sock , ack , strlen(ack) , 0 );
    LogTrace(hfErr, debug, "UI2MCP --> UI : MESSAGE GET send\n" );
    //printf("MESSAGE GET send\n");

	// Init value array
	for (j = 0; j < UIChannel; j++){
		for (i = 0; i < 8; i++){
			MidiTable [j][i] = 0;
		}
		PanMidi[j] = .5;			// Pan //MidiTable = Pan potentiometer 0=left 0.5=center 1=right
	}
	for (j = 0; j < 6; j++){
		GroupMaskMute[j] = 0;
	}

	// Reset Button light
	for (j = 0; j < NbMidiFader; j++){
		char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+j, 0x00};
        SendMidiOut(midiout, MidiArray);
	}

	for (j = 0; j < NbMidiFader; j++){
		char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+j, 0x00};
        SendMidiOut(midiout, MidiArray);
	}

	for (j = 0; j < NbMidiFader; j++){
		char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+j, 0x00};
        SendMidiOut(midiout, MidiArray);
	}

	// Led Forward On for AddrMidiTrack = 0
    char ForwardLedOn[3] = {AddrMidiButtonLed, 0x5B, 0x7F};
    SendMidiOut(midiout, ForwardLedOn);

/*  Configuration mode LCD  */
 for (j = 0; j < NbMidiFader; j++){
    char Cmd[32] = "";
    char c_Canal[32] = "";

    strcpy(Cmd, "13");
    sprintf(c_Canal, "%02X", j);
    strcat(Cmd, c_Canal);
    strcat(Cmd, "18");
    SendSysExTextOut(midiout, SysExHdr, Cmd, "");
 }

 /*  Initialization of timer variable  */
 current_time_init = currentTimeMillis();
 i_Watchdog_init = currentTimeMillis();
 i_VuMeter_init = currentTimeMillis();

do {

 	/*  Watchdog  */
	i_Watchdog = currentTimeMillis();
	if (i_Watchdog >= (i_Watchdog_init + 1000)){                                                  /*  Watchdog to send message to UI & Controler  */

        //LogTrace(hfErr, debug, "UI2MCP --> UI : ALIVE\n");
		command = "ALIVE\n";
		send(sock , command, strlen(command) , 0 );

		char c_RunningStatus[3] = {0xA0, 0x00, 0x00};
        SendMidiOut(midiout, c_RunningStatus);

		i_Watchdog_init = currentTimeMillis();
	}

 	// Test de timer pour TAP
	delay = 60000/UIBpm;
	current_time = currentTimeMillis();

	if (current_time >= (current_time_init + delay)){                                              /*  Tap function  */
		char bpmon[3]  = {AddrMidiButtonLed, i_Tap, 0x7F};
		char bpmoff[3] = {AddrMidiButtonLed, i_Tap, 0x00};
        SendMidiOut(midiout, bpmon);
		usleep( 25000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
        SendMidiOut(midiout, bpmoff);
		current_time_init = currentTimeMillis();
	}

	if ((status = snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer))) < 0)
	{
        memset(buffer, 0, sizeof(buffer));
		if(recv(sock, buffer, sizeof(buffer), 0) != SOCKET_ERROR)
		{

            //i_StxBuffer = 0;

			char *ArrayBuffer = NULL;  // changement ** to *
			char *UIMessage = NULL;  // changement ** to *
			int AdrSplitCRLF = 0;
			char **SplitArrayCRLF = NULL;;
			int sizeStr = 0;
			int sizeStxBuffer = 0;
			int sizeUIMessage = 0;

			//free(ArrayBuffer);
			//free(UIMessage);

            sprintf(sa_LogMessage,"\nUI2MCP <-- UI : Len %i\n", strlen(buffer));
            LogTrace(hfErr, debug, sa_LogMessage);

//            char c_LastBuffeString = buffer[strlen(buffer)-1];
//            if ( ! (strstr(&c_LastBuffeString, "\n"))){
//            	i_StxBuffer = 1;
//                LogTrace(hfErr, debug, "Buffer is not complit !!!!!!!\n");
//            }
//            if ( strstr(&c_LastBuffeString, "\x02")){
//            	i_StxBuffer = 1;
//                LogTrace(hfErr, debug, "Start of text\n");
//            }

//            sprintf(sa_LogMessage,"UI2MCP <-- UI : The last string of the chaine [%c]\n", buffer[strlen(buffer)-1]);
//            LogTrace(hfErr, debug, sa_LogMessage);
            LogTrace(hfErr, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
            sprintf(sa_LogMessage,"%s\n", buffer);
            LogTrace(hfErr, debug, sa_LogMessage);
            LogTrace(hfErr, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

			//sizeStr=strlen(buffer) + strlen(c_StxBuffer);
			sizeStr=strlen(buffer);
//			if(strlen(c_StxBuffer) != 0){
//                sizeStxBuffer  =  strlen(c_StxBuffer);
//                sprintf(sa_LogMessage,"REST PRECEDANT => len %i [%s]\n", strlen(c_StxBuffer), c_StxBuffer);
//                LogTrace(hfErr, debug, sa_LogMessage);
//			}
            ArrayBuffer=(char*) malloc( sizeof(char)*(sizeStr+1));
            //memset(ArrayBuffer, 0, sizeof(ArrayBuffer));
			//strncpy(ArrayBuffer,buffer,sizeStr);
//			if(c_StxBuffer != 0){
//                sizeStxBuffer  =  strlen(c_StxBuffer);
//			}
//			if(strlen(c_StxBuffer) == 0){
//                strcpy(ArrayBuffer, buffer);
//			}
//            else {
//                //sprintf(ArrayBuffer,"%s%s", c_StxBuffer, buffer);
//                //strcat(ArrayBuffer, c_StxBuffer);
//                strcpy(ArrayBuffer, c_StxBuffer);
//                //strncat(ArrayBuffer, buffer, sizeStr+sizeStxBuffer) ;
//            }
            strcpy(ArrayBuffer, buffer);

//            LogTrace(hfErr, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
//            sprintf(sa_LogMessage,"%s\n", ArrayBuffer);
//            LogTrace(hfErr, debug, sa_LogMessage);
//            LogTrace(hfErr, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

			SplitArrayCRLF=split(ArrayBuffer,"\n",1);

			//AdrSplitCRLF = 0;
			//while( 1 == 1){
			for(AdrSplitCRLF=0; SplitArrayCRLF[AdrSplitCRLF]!=NULL; AdrSplitCRLF++){

                //AdrSplitCRLF++;


				sizeUIMessage=strlen(SplitArrayCRLF[AdrSplitCRLF]);
				//UIMessage=(char*) malloc( sizeof(char)*(sizeUIMessage+1) );
                //memset(UIMessage, 0, sizeof(UIMessage));
				//strncpy(UIMessage,SplitArrayCRLF[AdrSplitCRLF],sizeUIMessage);
                if(strlen(c_StxBuffer) != 0){
                    sizeStxBuffer  =  strlen(c_StxBuffer);
                    //sprintf(sa_LogMessage,"1 REST PRECEDANT => len %i [%s]\n", strlen(c_StxBuffer), c_StxBuffer);
                    //LogTrace(hfErr, debug, sa_LogMessage);

                    UIMessage=(char*) malloc( sizeof(char)*(sizeUIMessage+sizeStxBuffer+2) );

                    // Remove the last caractere of  c_StxBuffer
                    char *c_StripStxBuffer = NULL;
                    c_StripStxBuffer = malloc (sizeof (*c_StripStxBuffer) * (strlen (c_StxBuffer) + 1));
                    int i, j;
                    for (i = 0, j = 0; c_StxBuffer[i]; i++)
                    {
                        //if (c_StxBuffer[i] == '\x02')
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
                    //c_StripStxBuffer[j] = '\0';

                    strcpy(UIMessage, c_StripStxBuffer);
                    free(c_StripStxBuffer);

                    sprintf(sa_LogMessage,"Rest last message => len %i [%s]\n", strlen(UIMessage), UIMessage);
                    LogTrace(hfErr, debug, sa_LogMessage);
                    LogTrace(hfErr, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

                    //sprintf(sa_LogMessage,"%s%s\n", UIMessage, SplitArrayCRLF[AdrSplitCRLF]);
                    //LogTrace(hfErr, debug, sa_LogMessage);

                    //strncat(UIMessage, sa_LogMessage, strlen(sa_LogMessage));
                    //strcpy(UIMessage,SplitArrayCRLF[AdrSplitCRLF]);
                    strcat(UIMessage, SplitArrayCRLF[AdrSplitCRLF]);
                    memset (c_StxBuffer, 0, sizeof (c_StxBuffer));
                }
				else{
                    sizeStxBuffer  =  strlen(c_StxBuffer);
                    //sprintf(sa_LogMessage,"1 REST PRECEDANT => len %i [%s]\n", strlen(c_StxBuffer), c_StxBuffer);
                    //LogTrace(hfErr, debug, sa_LogMessage);

                    UIMessage=(char*) malloc( sizeof(char)*(sizeUIMessage+1) );
                    strcpy(UIMessage,SplitArrayCRLF[AdrSplitCRLF]);
				}
//                strcpy(UIMessage,SplitArrayCRLF[AdrSplitCRLF]);

                sprintf(sa_LogMessage,"SplitArrayCRLF - [%s]\n", UIMessage);
                LogTrace(hfErr, debug, sa_LogMessage);

                if (strstr(UIMessage, "\x02")){
//                        int y;
//                        for(y = 0; UIMessage[y] != "\x02";y++){
//                            strncpy(UIMessage,SplitArrayCRLF[AdrSplitCRLF],sizeUIMessage);
//                        }
                    strcpy(c_StxBuffer, UIMessage);
                    //sprintf(c_StxBuffer,"%s", UIMessage);
                    sprintf(sa_LogMessage,"Not complit message not used now => len %i [%s]\n", strlen(c_StxBuffer), c_StxBuffer);
                    //sprintf(sa_LogMessage,"START OF TEXT => len \n");
                    LogTrace(hfErr, debug, sa_LogMessage);
                    LogTrace(hfErr, debug, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
                    break;
                }

//                if (SplitArrayCRLF[AdrSplitCRLF] == NULL){
//                    //c_StxBuffer=(char*) malloc( sizeof(char)*(strlen(SplitArrayCRLF[AdrSplitCRLF])+1) );
//                    //strncpy(c_StxBuffer, SplitArrayCRLF[AdrSplitCRLF], strlen(SplitArrayCRLF[AdrSplitCRLF]));
//                    LogTrace(hfErr, debug, c_StxBuffer);
//                    LogTrace(hfErr, debug, "dernier text\n");
//                    break;
//                }

				if(strlen(UIMessage) == 1){
					//printf("Len %i Recu : [%s]\n", strlen(buffer), buffer);

					if(init == 0){
						init = 1;

						command = "IOSYS^Connexion UI2MCP\n";
						printf("Send command IOSYS\n");
						send(sock , command, strlen(command) , 0 );

						command = "MODEL\n";
						printf("Send command MODEL\n");
						send(sock , command, strlen(command) , 0 );
					}
				}
				if(strstr(UIMessage, "RTA^")){
					// Actually nothing... futur VuMeter message to LCD MIDI Controler
				}
				if(strstr(UIMessage, "VU2^")){

                    SplitArrayComa=split(UIMessage,"^",1);
					free(SplitArrayComa);

                    char *enc = SplitArrayComa[1];

                   unsigned char *dec = b64_decode(enc, strlen(enc));

                   int i_k = 0;
                   int i_size64 = (3*strlen(enc))/4;

                   UIChannel = (int)dec[0];
                   UIMedia = (int)dec[1];
                   UISubGroup = (int)dec[2];
                   UIFx = (int)dec[3];
                   UIAux = (int)dec[4];
                   UIMaster = (int)dec[5];
                   UILineIn = (int)dec[6];

                    int t = 8;
                    //Inputs
                    for (i_k = 0; i_k < (UIChannel); ++i_k) {
                        //for (i = 8; i < s; ++i) {
                            ui_i[i_k].vuPre = (int)dec[t];
                            ui_i[i_k].VuPost = (int)dec[t+1];
                            ui_i[i_k].vuPostFader = (int)dec[t+2];
                            ui_i[i_k].vuGateIn = (int)dec[t+3];
                            ui_i[i_k].vuCompOut = (int)dec[t+4];
                            if((int)dec[t+5] & 0x80){
                                ui_i[i_k].vuCompMeter = 0x7F-((int)dec[t+5]^0x80);
                                ui_i[i_k].Gate = 0x01;
                                //printf("vuCompMeter:%02X Gate ON\n", 0x7F-(vuCompMeter[Canal]^0x80));
                            }else{
                                ui_i[i_k].vuCompMeter = 0x7F-((int)dec[t+5]);
                                ui_i[i_k].Gate = 0x00;
                                //printf("vuCompMeter:%02X Gate OFF\n", 0x7F-vuCompMeter[Canal]);
                            }
                            //printf("%i %02X%02X%02X%02X%02X%02X\n", t, (int)dec[t],(int)dec[t+1],(int)dec[t+2],(int)dec[t+3],(int)dec[t+4],(int)dec[t+5]);
                            t = t+6;
                        //}
                    }
                    //Media
                    for (i_k = 0; i_k < (UIMedia); ++i_k) {
                        //for (i = 8; i < s; ++i) {
                            ui_media[i_k].vuPre = (int)dec[t];
                            ui_media[i_k].VuPost = (int)dec[t+1];
                            ui_media[i_k].vuPostFader = (int)dec[t+2];
                            ui_media[i_k].vuGateIn = (int)dec[t+3];
                            ui_media[i_k].vuCompOut = (int)dec[t+4];
                            if((int)dec[t+5] & 0x80){
                                ui_media[i_k].vuCompMeter = 0x7F-((int)dec[t+5]^0x80);
                                ui_media[i_k].Gate = 0x01;
                                //printf("vuCompMeter:%02X Gate ON\n", 0x7F-(vuCompMeter[Canal]^0x80));
                            }else{
                                ui_media[i_k].vuCompMeter = 0x7F-((int)dec[t+5]);
                                ui_media[i_k].Gate = 0x00;
                                //printf("vuCompMeter:%02X Gate OFF\n", 0x7F-vuCompMeter[Canal]);
                            }
                            //printf("%i %02X%02X%02X%02X%02X%02X\n", t, (int)dec[t],(int)dec[t+1],(int)dec[t+2],(int)dec[t+3],(int)dec[t+4],(int)dec[t+5]);
                            t = t+6;
                        //}
                    }
                    //Sub
                    for (i_k = 0; i_k < (UISubGroup); ++i_k) {
                            ui_s[i_k].vuPostL = (int)dec[t];
                            ui_s[i_k].vuPostR = (int)dec[t+1];
                            ui_s[i_k].vuPostFaderL = (int)dec[t+2];
                            ui_s[i_k].vuPostFaderR = (int)dec[t+3];
                            ui_s[i_k].vuGateIn = (int)dec[t+4];
                            ui_s[i_k].vuCompOut = (int)dec[t+5];
                            if((int)dec[t+6] & 0x80){
                                ui_s[i_k].vuCompMeter = 0x7F-((int)dec[t+6]^0x80);
                                ui_s[i_k].Gate = 0x01;
                                //printf("vuCompMeter:%02X Gate ON\n", 0x7F-(vuCompMeter[Canal]^0x80));
                            }else{
                                ui_s[i_k].vuCompMeter = 0x7F-((int)dec[t+6]);
                                ui_s[i_k].Gate = 0x00;
                                //printf("vuCompMeter:%02X Gate OFF\n", 0x7F-vuCompMeter[Canal]);
                            }
                            //printf("%i %02X%02X%02X%02X%02X%02X%02X\n", t, (int)dec[t],(int)dec[t+1],(int)dec[t+2],(int)dec[t+3],(int)dec[t+4],(int)dec[t+5],(int)dec[t+6]);
                            t = t+7;
                    }
                    //FX
                    for (i_k = 0; i_k < (UIFx); ++i_k) {
                            ui_f[i_k].vuPostL = (int)dec[t];
                            ui_f[i_k].vuPostR = (int)dec[t+1];
                            ui_f[i_k].vuPostFaderL = (int)dec[t+2];
                            ui_f[i_k].vuPostFaderR = (int)dec[t+3];
                            ui_f[i_k].vuGateIn = (int)dec[t+4];
                            ui_f[i_k].vuCompOut = (int)dec[t+5];
                            if((int)dec[t+6] & 0x80){
                                ui_f[i_k].vuCompMeter = 0x7F-((int)dec[t+6]^0x80);
                                ui_f[i_k].Gate = 0x01;
                                //printf("vuCompMeter:%02X Gate ON\n", 0x7F-(vuCompMeter[Canal]^0x80));
                            }else{
                                ui_f[i_k].vuCompMeter = 0x7F-((int)dec[t+6]);
                                ui_f[i_k].Gate = 0x00;
                                //printf("vuCompMeter:%02X Gate OFF\n", 0x7F-vuCompMeter[Canal]);
                            }
                            //printf("%i %02X%02X%02X%02X%02X%02X%02X\n", t, (int)dec[t],(int)dec[t+1],(int)dec[t+2],(int)dec[t+3],(int)dec[t+4],(int)dec[t+5],(int)dec[t+6]);
                            t = t+7;
                    }
                    //Aux
                    for (i_k = 0; i_k < (UIAux); ++i_k) {
                            ui_a[i_k].VuPost = (int)dec[t];
                            ui_a[i_k].vuPostFader = (int)dec[t+1];
                            ui_a[i_k].vuGateIn = (int)dec[t+2];
                            ui_a[i_k].vuCompOut = (int)dec[t+3];
                            if((int)dec[t+4] & 0x80){
                                ui_a[i_k].vuCompMeter = 0x7F-((int)dec[t+4]^0x80);
                                ui_a[i_k].Gate = 0x01;
                                //printf("vuCompMeter:%02X Gate ON\n", 0x7F-(vuCompMeter[Canal]^0x80));
                            }else{
                                ui_a[i_k].vuCompMeter = 0x7F-((int)dec[t+4]);
                                ui_a[i_k].Gate = 0x00;
                                //printf("vuCompMeter:%02X Gate OFF\n", 0x7F-vuCompMeter[Canal]);
                            }
                            //printf("%i %02X%02X%02X%02X%02X\n", t, (int)dec[t],(int)dec[t+1],(int)dec[t+2],(int)dec[t+3],(int)dec[t+4]);
                            t = t+5;
                    }
                    //Master
                    for (i_k = 0; i_k < (UIMaster); ++i_k) {
                            ui_master[i_k].VuPost = (int)dec[t];
                            ui_master[i_k].vuPostFader = (int)dec[t+1];
                            ui_master[i_k].vuGateIn = (int)dec[t+2];
                            ui_master[i_k].vuCompOut = (int)dec[t+3];
                            if((int)dec[t+4] & 0x80){
                                ui_master[i_k].vuCompMeter = 0x7F-((int)dec[t+4]^0x80);
                                ui_master[i_k].Gate = 0x01;
                                //printf("vuCompMeter:%02X Gate ON\n", 0x7F-(vuCompMeter[Canal]^0x80));
                            }else{
                                ui_master[i_k].vuCompMeter = 0x7F-((int)dec[t+4]);
                                ui_master[i_k].Gate = 0x00;
                                //printf("vuCompMeter:%02X Gate OFF\n", 0x7F-vuCompMeter[Canal]);
                            }
                            //printf("%i %02X%02X%02X%02X%02X\n", t, (int)dec[t],(int)dec[t+1],(int)dec[t+2],(int)dec[t+3],(int)dec[t+4]);
                            t = t+5;
                    }
                    //Line
                    for (i_k = 0; i_k < (UILineIn); ++i_k) {
                            ui_l[i_k].vuPre = (int)dec[t];
                            ui_l[i_k].VuPost = (int)dec[t+1];
                            ui_l[i_k].vuPostFader = (int)dec[t+2];
                            ui_l[i_k].vuGateIn = (int)dec[t+3];
                            ui_l[i_k].vuCompOut = (int)dec[t+4];
                            if((int)dec[t+5] & 0x80){
                                ui_l[i_k].vuCompMeter = 0x7F-((int)dec[t+5]^0x80);
                                ui_l[i_k].Gate = 0x01;
                                //printf("vuCompMeter:%02X Gate ON\n", 0x7F-(vuCompMeter[Canal]^0x80));
                            }else{
                                ui_l[i_k].vuCompMeter = 0x7F-((int)dec[t+5]);
                                ui_l[i_k].Gate = 0x00;
                                //printf("vuCompMeter:%02X Gate OFF\n", 0x7F-vuCompMeter[Canal]);
                            }
                            //printf("%i %02X%02X%02X%02X%02X%02X\n", t, (int)dec[t],(int)dec[t+1],(int)dec[t+2],(int)dec[t+3],(int)dec[t+4],(int)dec[t+5]);
                            t = t+6;
                    }
                    //printf("t=%i\n", t);
                    //printf("i_size64=%i\n", i_size64);
                    //printf("\n");
                    free(dec);

//                    Canal = 0;
//                    for (Canal = 0; Canal < UIChannel; ++Canal) {
//                        //printf("Canal(%i) vuPre:%02X VuPost:%02X vuPostFader:%02X vuGateIn%02X vuCompOut:%02X vuCompMeter:%02X Gate:%i\n", Canal, vuPre[Canal], VuPost[Canal], vuPostFader[Canal], vuGateIn[Canal], vuCompOut[Canal], vuCompMeter[Canal], Gate[Canal]);
//
//                        printf("Canal(%i) ", Canal);
//                        printf("vuPre:%02X ", ui_i[Canal].vuPre);
//                        printf("VuPost:%02X ", ui_i[Canal].VuPost);
//                        printf("vuPostFader:%02X ", ui_i[Canal].vuPostFader);
//                        printf("vuGateIn%02X ", ui_i[Canal].vuGateIn);
//                        printf("vuCompOut:%02X ", ui_i[Canal].vuCompOut);
//                        printf("vuCompMeter:%02X ", ui_i[Canal].vuCompMeter);
//                        printf("Gate:%i\n", ui_i[Canal].Gate);
//
//                    }
                    /*  Refresh Vu Meter only 800ms  */
                    i_VuMeter = currentTimeMillis();
                    int i_Flag = 0;
                    if (i_VuMeter >= (i_VuMeter_init + 450)){
                        Canal = 0;
                        for (Canal = 0; Canal < UIChannel; ++Canal) {
                            if(ui_i[Canal].vuPostFader > 0){
                                printf("(%i) %02X ", Canal, ui_i[Canal].vuPostFader);
                                char MidiArray[2] = {0xD0+Canal, ui_i[Canal].vuPostFader};
                                SendMidiOut(midiout, MidiArray);
                                i_Flag = 1;
                            }
                       }
                        i_VuMeter_init = currentTimeMillis();
                        if(i_Flag){printf("\n");}
                    }

				}
				if(strstr(UIMessage, "currentShow^")){
					//SETS^var.currentShow^Studio Stephan

					UICommand = strtok(strstr(UIMessage,"currentShow^"), "\r\n");

                    sprintf(sa_LogMessage, "UI2MCP  <--  UI : Current Show : %s\n", UICommand);
                    LogTrace(hfErr, debug, sa_LogMessage);

					SplitArrayComa=split(UICommand,"^",1);
					strcat(UIShow, SplitArrayComa[1]);
					free(SplitArrayComa);

					char CmdSnapShot[256];
					sprintf(CmdSnapShot,"SNAPSHOTLIST^%s\n", UIShow);

                    sprintf(sa_LogMessage, "UI2MCP  -->  UI : Send command : %s\n", CmdSnapShot);
                    LogTrace(hfErr, debug, sa_LogMessage);
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
                    LogTrace(hfErr, debug, sa_LogMessage);

				}
				if(strstr(UIMessage, "SNAPSHOTLIST^")){

					UICommand = strtok(strstr(UIMessage,"SNAPSHOTLIST^"), "\r\n");

                    sprintf(sa_LogMessage, "UI2MCP  <--  UI : SnapShotList [%s]\n", UICommand);
                    LogTrace(hfErr, debug, sa_LogMessage);

					SplitArrayComa=split(UICommand,"^",1);
					//affichage du resultat
					for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
						if (AdrSplitComa >= 2){
							//printf("i=%d : [%s] current [%s]\n",AdrSplitComa-2,SplitArrayComa[AdrSplitComa], SnapShotCurrent);
							strcat(UISnapShotList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
							SnapShotMax = AdrSplitComa-2;

							if(strcmp(SplitArrayComa[AdrSplitComa], SnapShotCurrent) == 0){
								SnapShotIndex = AdrSplitComa-2;
							}
						}
					}
					free(SplitArrayComa);

                    sprintf(sa_LogMessage, "UI2MCP  <--  UI : SnapShotIndex [%i]\n",SnapShotIndex);
                    LogTrace(hfErr, debug, sa_LogMessage);

				}
				if(strstr(UIMessage, "MODEL^")){
					UIModel = strtok(strstr(UIMessage,"MODEL^"), "\r\n");

                    sprintf(sa_LogMessage, "UI2MCP  <--  UI : Soundcraft Model [%s]\n",UIModel);
                    LogTrace(hfErr, debug, sa_LogMessage);

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
				if(strstr(UIMessage, "SETD^") || strstr(UIMessage, "SETS^")){

					//------------------------------------------------------------------------------------------------------------------------
					// Split de la chaine recu via websocket

					SplitArrayComa=split(UIMessage,"^",1);
					//affichage du resultat
					for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++){
						if(AdrSplitComa==1){
							SplitArrayDot=split(SplitArrayComa[AdrSplitComa],".",1);
							//affichage du resultat
							for(ArraySplitDot=0;SplitArrayDot[ArraySplitDot]!=NULL;ArraySplitDot++){
								//printf("j=%d : %s\n",j,SplitArrayDot[j]);
								if (ArraySplitDot == 0){ sprintf(UIio,"%s",SplitArrayDot[ArraySplitDot]); }
								if (ArraySplitDot == 1){ sprintf(UIchan,"%s",SplitArrayDot[ArraySplitDot]);}
								if (ArraySplitDot == 2){ sprintf(UIfunc,"%s",SplitArrayDot[ArraySplitDot]);}
								//au passge je désalloue les chaines
								free(SplitArrayDot[ArraySplitDot]);
							}
						}else if(AdrSplitComa!=1){
							//printf("i=%d : %s\n",AdrSplitComa,SplitArrayComa[AdrSplitComa]);
							if (AdrSplitComa == 0){ sprintf(UImsg,"%s",SplitArrayComa[AdrSplitComa]);}
							if (AdrSplitComa == 2){ sprintf(UIval,"%s",SplitArrayComa[AdrSplitComa]);}
							//au passge je désalloue les chaines
							free(SplitArrayComa[AdrSplitComa]);
						}
					}
					free(SplitArrayComa);
					free(SplitArrayDot);

					if( strcmp(UIfunc,"bpm") == 0 ){
						UIBpm = atof(UIval);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : UIbpm = %f\n", atof(UIval));
                        LogTrace(hfErr, debug, sa_LogMessage);

					}
					else if( strcmp(UIio,"mgmask") == 0 || strcmp(UIfunc,"mgmask") == 0){

						if( strcmp(UIio,"mgmask") == 0 )
						{

							strcat(UIMuteMask, UIval);

                            sprintf(sa_LogMessage,"UI2MCP <-- UI : %s : Mute Mask : %i\n", UIMessage, atoi(UIMuteMask));
                            LogTrace(hfErr, debug, sa_LogMessage);

							unsigned bit;
							for (bit = 0; bit < 6; bit++)
							{
                                sprintf(sa_LogMessage,"UI2MCP <-- UI : bit %i : %i\n", bit, atoi(UIMuteMask) & (1u << bit));
                                LogTrace(hfErr, debug, sa_LogMessage);
								GroupMaskMute[bit]= atoi(UIMuteMask) & (1u << bit);
							}

							for (Canal = 0; Canal < UIChannel; Canal++){
								if( (GroupMaskMute[0] == (MidiTable [Canal][MaskMuteValue] & (1u << 0)) && (MidiTable [Canal][MaskMuteValue] & (1u << 0)) != 0) ||
									(GroupMaskMute[1] == (MidiTable [Canal][MaskMuteValue] & (1u << 1)) && (MidiTable [Canal][MaskMuteValue] & (1u << 1)) != 0) ||
									(GroupMaskMute[2] == (MidiTable [Canal][MaskMuteValue] & (1u << 2)) && (MidiTable [Canal][MaskMuteValue] & (1u << 2)) != 0) ||
									(GroupMaskMute[3] == (MidiTable [Canal][MaskMuteValue] & (1u << 3)) && (MidiTable [Canal][MaskMuteValue] & (1u << 3)) != 0) ||
									(GroupMaskMute[4] == (MidiTable [Canal][MaskMuteValue] & (1u << 4)) && (MidiTable [Canal][MaskMuteValue] & (1u << 4)) != 0) ||
									(GroupMaskMute[5] == (MidiTable [Canal][MaskMuteValue] & (1u << 5)) && (MidiTable [Canal][MaskMuteValue] & (1u << 5)) != 0)){

									MidiTable [Canal][MaskMute] = 1;
								}
								else{
									MidiTable [Canal][MaskMute] = 0;
								}

                                int d = 0x7F;
                                int i_OrMute = (MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) & ( ! (MidiTable [Canal][ForceUnMute]));
                                if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : mgmask : Update Light : Canal(%i) : Mute | MaskMute | ! ForceUnMute = %i * 0x7F\n", Canal, i_OrMute);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMute*d};
                                    SendMidiOut(midiout, MidiArray);
                                }
                                sprintf(sa_LogMessage,"UI2MCP <-- UI : mgmask : Canal(%i) [Mute=%i][ForceUnMute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, MidiTable [Canal][Mute], MidiTable [Canal][ForceUnMute], MidiTable [Canal][MaskMute], MidiTable [Canal][MaskMuteValue]);
                                LogTrace(hfErr, debug, sa_LogMessage);
								//printf( "Canal %i AddrMute %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiMute, NbMidiFader, AddrMidiTrack, v);
							}
						}
						if( (strcmp(UIio,"i") == 0 && strcmp(UIfunc,"mgmask") == 0)){

                            sprintf(sa_LogMessage,"UI2MCP <-- UI : %s\n", UIMessage);
                            LogTrace(hfErr, debug, sa_LogMessage);

							Canal = atoi(UIchan);

                            sprintf(sa_LogMessage,"UI2MCP <-- UI : Mgmask : Type IO: %s Channel: %i Function: %s Value: %i\n", UIio, atoi(UIchan),UIfunc,atoi(UIval));
                            LogTrace(hfErr, debug, sa_LogMessage);

							if( debug == 2){
								printf("(GroupMaskMute[0]=%i,Value=%i)\n", GroupMaskMute[0], atoi(UIval) & (1u << 0));
								printf("(GroupMaskMute[1]=%i,Value=%i)\n", GroupMaskMute[1], atoi(UIval) & (1u << 1));
								printf("(GroupMaskMute[2]=%i,Value=%i)\n", GroupMaskMute[2], atoi(UIval) & (1u << 2));
								printf("(GroupMaskMute[3]=%i,Value=%i)\n", GroupMaskMute[3], atoi(UIval) & (1u << 3));
								printf("(GroupMaskMute[4]=%i,Value=%i)\n", GroupMaskMute[4], atoi(UIval) & (1u << 4));
								printf("(GroupMaskMute[5]=%i,Value=%i)\n", GroupMaskMute[5], atoi(UIval) & (1u << 5));
							}

							if( (GroupMaskMute[0] == (atoi(UIval) & (1u << 0)) && (atoi(UIval) & (1u << 0)) != 0) ||
								(GroupMaskMute[1] == (atoi(UIval) & (1u << 1)) && (atoi(UIval) & (1u << 1)) != 0) ||
								(GroupMaskMute[2] == (atoi(UIval) & (1u << 2)) && (atoi(UIval) & (1u << 2)) != 0) ||
								(GroupMaskMute[3] == (atoi(UIval) & (1u << 3)) && (atoi(UIval) & (1u << 3)) != 0) ||
								(GroupMaskMute[4] == (atoi(UIval) & (1u << 4)) && (atoi(UIval) & (1u << 4)) != 0) ||
								(GroupMaskMute[5] == (atoi(UIval) & (1u << 5)) && (atoi(UIval) & (1u << 5)) != 0)){

								MidiTable [Canal][MaskMute]=1;
								MidiTable [Canal][MaskMuteValue]=atoi(UIval);
							}
							else{
								MidiTable [Canal][MaskMute]=0;
								MidiTable [Canal][MaskMuteValue]=atoi(UIval);
							}

							int d = 0x7F;
                            int i_OrMute = (MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) & ( ! (MidiTable [Canal][ForceUnMute]));
                            if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mgmask : Update Light : Canal(%i) : Mute | MaskMute | ! ForceUnMute =%i * 0x7F\n", Canal, i_OrMute);
                                LogTrace(hfErr, debug, sa_LogMessage);
                                char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMute*d};
                                SendMidiOut(midiout, MidiArray);
                            }
                            sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mgmask : Canal(%i) [Mute=%i][ForceUnMute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, MidiTable [Canal][Mute], MidiTable [Canal][ForceUnMute], MidiTable [Canal][MaskMute], MidiTable [Canal][MaskMuteValue]);
                            LogTrace(hfErr, debug, sa_LogMessage);
							//printf( "Canal %i AddrMute %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiMute, NbMidiFader, AddrMidiTrack, v);
						}
					}
					else if( strcmp(UIio,"i") == 0 && (strcmp(UIfunc,"mute") == 0 || strcmp(UIfunc,"forceunmute") == 0)){

						int d,v;

						Canal = atoi(UIchan);
						v = atoi(UIval);

                        int i_OrMuteForceunmute = (MidiTable [Canal][Mute] | MidiTable [Canal][MaskMute] | ( ! (MidiTable [Canal][ForceUnMute])));
                                // Bug !!!                (MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) & ( ! (MidiTable [Canal][ForceUnMute]));

                        sprintf(sa_LogMessage,"UI2MCP <-- UI : %s\n", UIMessage);
                        LogTrace(hfErr, debug, sa_LogMessage);

						if (strcmp(UIfunc,"forceunmute") == 0){
                            if (MidiTable [Canal][MaskMute] == 1){
                                if (v == 1){
                                    MidiTable [Canal][ForceUnMute] = 1;
                                    d = 0x00;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.forceunmute : forceunmute to 1 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                                else if (v == 0){
                                    MidiTable [Canal][ForceUnMute] = 0;
                                    d = 0x7F;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.forceunmute :  forceunmute to 0 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                                if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.forceunmute : io.forecunmute : Canal(%i) : Mute | MaskMute | ! ForceUnMute =%i * 0x7F\n", Canal, i_OrMuteForceunmute);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                    //char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , (MidiTable [Canal][Mute] | MidiTable [Canal][MaskMute] | ( ! (MidiTable [Canal][ForceUnMute])))*d};
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMuteForceunmute*d};
                                    SendMidiOut(midiout, MidiArray);
                                }
                            }else if (MidiTable [Canal][MaskMute] == 0){
                                if (v == 1){
                                    MidiTable [Canal][ForceUnMute] = 1;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.forceunmute : update only forceunmute to 1 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                                else if (v == 0){
                                    MidiTable [Canal][ForceUnMute] = 0;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.forceunmute : update only forceunmute to 0 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                            }
						}
						else if(strcmp(UIfunc,"mute") == 0){
                            if (MidiTable [Canal][MaskMute] == 0){
                                if (v == 0){
                                    MidiTable [Canal][Mute] = 0;
                                    d = 0x00;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mute : mute to 0 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                                else if (v == 1){
                                    MidiTable [Canal][Mute] = 1;
                                    d = 0x7F;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mute : mute to 1 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                                if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mute : io.mute : Canal(%i) : Mute | MaskMute | ! ForceUnMute =%i * 0x7F\n", Canal, i_OrMuteForceunmute);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                    //char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , (MidiTable [Canal][Mute] | MidiTable [Canal][MaskMute] | ( ! (MidiTable [Canal][ForceUnMute])))*d};
                                    char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , i_OrMuteForceunmute*d};
                                    SendMidiOut(midiout, MidiArray);
                                }
                            }else if (MidiTable [Canal][MaskMute] == 1){
                                if (v == 0){
                                    MidiTable [Canal][Mute] = 0;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mute : update only mute to 0 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                                else if (v == 1){
                                    MidiTable [Canal][Mute] = 1;
                                    sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mute : update only mute to 1 on Canal(%i)\n", Canal);
                                    LogTrace(hfErr, debug, sa_LogMessage);
                                }
                            }
						}
                        sprintf(sa_LogMessage,"UI2MCP <-- UI : io.mute/forceunmute : Canal(%i) [Mute=%i][ForceUnMute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, MidiTable [Canal][Mute], MidiTable [Canal][ForceUnMute], MidiTable [Canal][MaskMute], MidiTable [Canal][MaskMuteValue]);
                        LogTrace(hfErr, debug, sa_LogMessage);
						//printf( "Canal %i AddrMute %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiMute, NbMidiFader, AddrMidiTrack, v);
					}
					else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"mtkrec") == 0 ){

						int d,v;
						Canal = atoi(UIchan);
						v = atoi(UIval);
						if (v == 0){
							MidiTable [Canal][Rec] = 0;
							d = 0x00;
						}
						else if (v == 1){
							MidiTable [Canal][Rec] = 1;
							d = 0x7F;
						}

						if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
							char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+Canal-(NbMidiFader*AddrMidiTrack) , MidiTable [Canal][Rec]*d};
                            SendMidiOut(midiout, MidiArray);
						}
						//printf( "Canal %i AddrRec %02x NbMidiFader %02x AddrMidiTrack %i ValueRec %i\n", Canal, AddrMidiRec, NbMidiFader, AddrMidiTrack, v);
					}
					else if( strcmp(UIio,"m") == 0 && strcmp(UIchan,"dim") == 0 ){
						//int d = 0;
						int v = 0;
						v = atoi(UIval);
						if (v == 0){
							DimMaster = 0;
						}
						else if (v == 1){
							DimMaster = 1;
						}
						//printf( "Canal %i AddrRec %02x NbMidiFader %02x AddrMidiTrack %i ValueRec %i\n", Canal, AddrMidiRec, NbMidiFader, AddrMidiTrack, v);
					}
					else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"name") == 0 ){

						Canal = atoi(UIchan);
                        strcpy(NameChannel[Canal], UIval);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : Name %i: %s\n", Canal, NameChannel[Canal]);
                        LogTrace(hfErr, debug, sa_LogMessage);

                        /*  Send Name to MIDI LCD  */
						if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){

							char Cmd[32] = "";
							char c_Canal[32] = "";
							char c_CanalText[32] = "";

                            strcpy(Cmd, "12");
                            sprintf(c_Canal, "%02X", Canal);
                            sprintf(c_CanalText, "%i", Canal+1);
                            strcat(Cmd, c_Canal);
                            strcat(Cmd, "0100");
							SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

                            strcpy(Cmd, "12");
                            sprintf(c_Canal, "%02X", Canal);
                            strcat(Cmd, c_Canal);
                            strcat(Cmd, "0000");
							SendSysExTextOut(midiout, SysExHdr, Cmd, NameChannel[Canal]);
						}
					}
					else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"solo") == 0 ){

						int d,v;
						Canal = atoi(UIchan);
						v = atoi(UIval);
						if (v == 0){
							MidiTable [Canal][Solo] = 0;
							d = 0x00;
						}
						else if (v == 1){
							MidiTable [Canal][Solo] = 1;
							d = 0x7F;
						}
						if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
							char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+Canal-(NbMidiFader*AddrMidiTrack) , MidiTable [Canal][Solo]*d};
                            SendMidiOut(midiout, MidiArray);
						}
						//printf( "Canal %i AddrSolo %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiSolo, NbMidiFader, AddrMidiTrack, v);
					}
					else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"mix") == 0 ){

						Canal = atoi(UIchan);
						MixMidi[Canal] = atof(UIval);

                        int MidiValue = 0;
                        //float unit = 0.0078740157480315;
						MidiValue = (127 * MixMidi[Canal]);

                        sprintf(sa_LogMessage, "UI2MCP  <--  UI : Fader %i: %f %i\n", Canal, MixMidi[Canal], MidiValue);
                        LogTrace(hfErr, debug, sa_LogMessage);

						if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
							char MidiArray[3] = {AddrMidiMix+Canal-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                            SendMidiOut(midiout, MidiArray);
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
        LogTrace(hfErr, debug, "UI2MCP <-- MIDI : Midi read\n");

		InMidi = (int)Midibuffer[0];
		MidiCC = (int)Midibuffer[j+1];
		MidiValue = (int)Midibuffer[j+2];

        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Midi IN: %02X %02X %02X\n",InMidi, MidiCC, MidiValue);
        LogTrace(hfErr, debug, sa_LogMessage);

        sprintf(sa_LogMessage,"UI2MCP : Address Track : %i\n", AddrMidiTrack);
        LogTrace(hfErr, debug, sa_LogMessage);

		if (InMidi >= AddrMidiMix && InMidi <= AddrMidiMix + NbMidiFader - 1){                                            /*  Midi command Fader  */
			Canal = InMidi % AddrMidiMix +(NbMidiFader*AddrMidiTrack);
//			if(MidiCC == MidiValue){
				char sendui[256];
				sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
				send(sock , sendui, strlen(sendui) , 0 );

				int fs;
				float db;
				float unit = 0.0078740157480315;
				for (fs = 0; fs < status; fs+=3){
					status = snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer));
					if (InMidi >= AddrMidiMix && InMidi <= AddrMidiMix + NbMidiFader - 1){
						InMidi = (int)Midibuffer[fs];
						MidiCC = (int)Midibuffer[fs+1];
						MidiValue = (int)Midibuffer[fs+2];
						db = MidiValue * unit;
                        MixMidi[Canal] = db;
						printf("Status %i Fader %i: %02X %02X, %.10f\n", status, Canal, MidiCC, MidiValue, db);

						char sendui[256];
						sprintf(sendui,"SETD^i.%d.mix^%.10f\n", Canal, db);
						send(sock , sendui, strlen(sendui) , 0 );
					}
				//}
			}
		}
		else if (InMidi == AddrMidiEncoderPan){                                                                                                   /*  Midi command Encoder  */
			if (MidiCC >= AddrMidiPan && MidiCC <= AddrMidiPan + NbMidiFader - 1){
				Canal = MidiCC % AddrMidiPan +(NbMidiFader*AddrMidiTrack);

				char sendui[256];
				sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
				send(sock , sendui, strlen(sendui) , 0 );

				if(MidiValue == 0x41 && PanMidi[Canal] > 0){
					PanMidi[Canal] =  fabs (PanMidi[Canal]-.03);
				}
				else if(MidiValue == 0x01 && PanMidi[Canal] < 1){
					PanMidi[Canal] = PanMidi[Canal]+.03;
				}
				printf("Pan %i: %f\n", Canal, PanMidi[Canal]);

				//char sendui[256];
				sprintf(sendui,"SETD^i.%d.pan^%.10f\n", Canal, PanMidi[Canal]);
				send(sock , sendui, strlen(sendui) , 0 );
			}
		}
		else if (InMidi == AddrMidiButtonLed){                                                                                                                            /*  Midi command Button & Led  */
			if (MidiCC >= AddrMidiRec && MidiCC <= AddrMidiRec + NbMidiFader - 1){                                    /*  Record  */

				Canal = MidiCC +(NbMidiFader*AddrMidiTrack);

				if(MidiValue == 0x7F){
                    char sendui[256];
                    sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                    send(sock , sendui, strlen(sendui) , 0 );
				}

				if(MidiValue == 0x7F && MidiTable [Canal][Rec] ==0){
					MidiTable [Canal][Rec] = 1;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.mtkrec^1\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                    SendMidiOut(midiout, MidiArray);
				}
				else if(MidiValue == 0x7F && MidiTable [Canal][Rec] ==1){
					MidiTable [Canal][Rec] = 0;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.mtkrec^0\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                    SendMidiOut(midiout, MidiArray);
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}
			else if (MidiCC >= AddrMidiSolo && MidiCC <= AddrMidiSolo + NbMidiFader -1){                             /*  Solo  */

				Canal = MidiCC % AddrMidiSolo +(NbMidiFader*AddrMidiTrack);

				if(MidiValue == 0x7F){
                    char sendui[256];
                    sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                    send(sock , sendui, strlen(sendui) , 0 );
				}

				//Init value array
				for (j = 0; j < UIChannel; j++){
					if(j != Canal){
						MidiTable [j][Solo] = 0;
						char sendui[256];
						sprintf(sendui,"SETD^i.%d.solo^0\n", j);
						send(sock , sendui, strlen(sendui) , 0 );
					}
				}

				if(MidiValue == 0x7F && MidiTable [Canal][Solo] ==0){
					MidiTable [Canal][Solo] = 1;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.solo^1\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                    SendMidiOut(midiout, MidiArray);

					for (j = 0; j < UIChannel; j++){
						if(MidiTable [j][Solo] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
							char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x00};
                            SendMidiOut(midiout, MidiArray);
						}
					}
				usleep( 100000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
				}
				else if(MidiValue == 0x00 && MidiTable [Canal][Solo] ==1){
					MidiTable [Canal][Solo] = 0;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.solo^0\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                    SendMidiOut(midiout, MidiArray);

					for (j = 0; j < UIChannel; j++){
						if(MidiTable [j][Solo] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
							char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x00};
                            SendMidiOut(midiout, MidiArray);
						}
					}
				}
				usleep( 100000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}
			else if (MidiCC >= AddrMidiMute && MidiCC <= AddrMidiMute + NbMidiFader - 1){                        /*  Mute  */

				Canal = MidiCC % AddrMidiMute +(NbMidiFader*AddrMidiTrack);

				if(MidiValue == 0x7F){
                    char sendui[256];
                    sprintf(sendui,"BMSG^SYNC^%s^%i\n", c_SyncId, Canal);
                    send(sock , sendui, strlen(sendui) , 0 );
				}

				if(MidiValue == 0x7F && MidiTable [Canal][Mute] ==0){
					MidiTable [Canal][Mute] = 1;
					char sendui[256];
					if(MidiTable [Canal][MaskMute]==1){
						sprintf(sendui,"SETD^i.%d.forceunmute^0\n", Canal);
						printf(sendui,"SETD^i.%d.forceunmute^0\n", Canal);
					}
					else{
						sprintf(sendui,"SETD^i.%d.mute^1\n", Canal);
						printf(sendui,"SETD^i.%d.mute^1\n", Canal);
					}
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                    SendMidiOut(midiout, MidiArray);
				}
				else if(MidiValue == 0x7F && MidiTable [Canal][Mute] ==1){
					MidiTable [Canal][Mute] = 0;
					char sendui[256];
					if(MidiTable [Canal][MaskMute]==1){
						sprintf(sendui,"SETD^i.%d.forceunmute^1\n", Canal);
						printf(sendui,"SETD^i.%d.forceunmute^1\n", Canal);
					}
					else{
						sprintf(sendui,"SETD^i.%d.mute^0\n", Canal);
						printf(sendui,"SETD^i.%d.mute^0\n", Canal);
					}
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                    SendMidiOut(midiout, MidiArray);
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
			else if (MidiCC == IdTrackNext){                                                                                                         /*  Button for Track Next */

				if(MidiValue == 0x7F && AddrMidiTrack == 0){
					AddrMidiTrack++;

					char RewindLedOn[3] = {AddrMidiButtonLed, 0x5C, 0x7F};
                    SendMidiOut(midiout, RewindLedOn);
				}
				else if(MidiValue == 0x7F && AddrMidiTrack < NbMidiTrack-1){
					AddrMidiTrack++;

					char ForwardLedOn[3] = {AddrMidiButtonLed, 0x5B, 0x00};
                    SendMidiOut(midiout, ForwardLedOn);
					char RewindLedOn[3] = {AddrMidiButtonLed, 0x5C, 0x7F};
                    SendMidiOut(midiout, RewindLedOn);
				}
				//printf("Track Left: %i\n", AddrMidiTrack);
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Mute;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){

                    int i_OrMute = (MidiTable [j][MaskMute] | MidiTable [j][Mute]) & ( ! (MidiTable [j][ForceUnMute]));
                    //(MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute] | | MidiTable [Canal][ForceUnMute])
//					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
					if(i_OrMute == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Track Left : Canal(%i) : AddrModuloValue 0x%02X (Mute | MaskMute | ! ForceUnMute = %i * 0x7F)\n", j, AddrMidiMute+(j % NbMidiFader), i_OrMute);
                        LogTrace(hfErr, debug, sa_LogMessage);
						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+(j % NbMidiFader), 0x7F};
                        SendMidiOut(midiout, MidiArray);
					}
//					else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
					else if(i_OrMute == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Track Left : Canal(%i) : AddrModuloValue 0x%02X (Mute | MaskMute | ! ForceUnMute = %i * 0x00)\n", j, AddrMidiMute+(j % NbMidiFader), i_OrMute);
                        LogTrace(hfErr, debug, sa_LogMessage);
						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+(j % NbMidiFader), 0x00};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Solo;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x7F};
                        SendMidiOut(midiout, MidiArray);
					}
					else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x00};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Rec;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+(j % NbMidiFader), 0x7F};
                        SendMidiOut(midiout, MidiArray);
					}
					else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+(j % NbMidiFader), 0x00};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
                    int MidiValue = 0;
                    MidiValue = (127 * MixMidi[j]);
					if((j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

                        char MidiArray[3] = {AddrMidiMix+j-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				for (j = 0; j < UIChannel; j++){
                        /*  Send Name to MIDI LCD  */
						if(j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
							char Cmd[32] = "";
							char c_Canal[32] = "";
							char c_CanalText[32] = "";

                            strcpy(Cmd, "12");
                            sprintf(c_Canal, "%02X", j-(NbMidiFader*AddrMidiTrack));
                            sprintf(c_CanalText, "%i", j+1);
                            strcat(Cmd, c_Canal);
                            strcat(Cmd, "0100");
							SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

                            strcpy(Cmd, "12");
                            sprintf(c_Canal, "%02X", j-(NbMidiFader*AddrMidiTrack));
                            strcat(Cmd, c_Canal);
                            strcat(Cmd, "0000");
							SendSysExTextOut(midiout, SysExHdr, Cmd, NameChannel[j]);
						}
                    }
			}
			else if (MidiCC == IdTrackPrev){                                                                                                         /*  Button for Track Prev  */

				if(MidiValue == 0x7F && AddrMidiTrack == 0){
					char ForwardLedOn[3] = {AddrMidiButtonLed, 0x5B, 0x7F};
                    SendMidiOut(midiout, ForwardLedOn);
					char RewindLedOn[3] = {AddrMidiButtonLed, 0x5C, 0x00};
                    SendMidiOut(midiout, RewindLedOn);
				}
				else if(MidiValue == 0x7F && AddrMidiTrack > 0){
					AddrMidiTrack--;

					char ForwardLedOn[3] = {AddrMidiButtonLed, 0x5B, 0x7F};
                    SendMidiOut(midiout, ForwardLedOn);

					if(AddrMidiTrack == 0){
						char RewindLedOn[3] = {AddrMidiButtonLed, 0x5C, 0x00};
                        SendMidiOut(midiout, RewindLedOn);
					}
				}
				//printf("Track Left: %i\n", AddrMidiTrack);
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Mute;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
                    int i_OrMute = (MidiTable [j][MaskMute] | MidiTable [j][Mute]) & ( ! (MidiTable [j][ForceUnMute]));
					if(i_OrMute == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
					//if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Track Right : Canal(%i) : AddrModuloValue 0x%02X (Mute | MaskMute | ! ForceUnMute = %i * 0x7F)\n", j, AddrMidiMute+(j % NbMidiFader), i_OrMute);
                        LogTrace(hfErr, debug, sa_LogMessage);
						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+(j % NbMidiFader), 0x7F};
                        SendMidiOut(midiout, MidiArray);
					}
					//else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
					else if(i_OrMute == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){
                        sprintf(sa_LogMessage,"UI2MCP <-- MIDI : Track Right : Canal(%i) : AddrModuloValue 0x%02X (Mute | MaskMute | ! ForceUnMute = %i * 0x00)\n", j, AddrMidiMute+(j % NbMidiFader), i_OrMute);
                        LogTrace(hfErr, debug, sa_LogMessage);
						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+(j % NbMidiFader), 0x00};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Solo;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x7F};
                        SendMidiOut(midiout, MidiArray);
					}
					else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+(j % NbMidiFader), 0x00};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Rec;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+(j % NbMidiFader), 0x7F};
                        SendMidiOut(midiout, MidiArray);
					}
					else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

						char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+(j % NbMidiFader), 0x00};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
                    int MidiValue = 0;
                    MidiValue = (127 * MixMidi[j]);
					if((j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)){

                        char MidiArray[3] = {AddrMidiMix+j-(NbMidiFader*AddrMidiTrack) , MidiValue, MidiValue};
                        SendMidiOut(midiout, MidiArray);
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				for (j = 0; j < UIChannel; j++){
                        /*  Send Name to MIDI LCD  */
						if(j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1){
							char Cmd[32] = "";
							char c_Canal[32] = "";
							char c_CanalText[32] = "";

                            strcpy(Cmd, "12");
                            sprintf(c_Canal, "%02X", j-(NbMidiFader*AddrMidiTrack));
                            sprintf(c_CanalText, "%i", j+1);
                            strcat(Cmd, c_Canal);
                            strcat(Cmd, "0100");
							SendSysExTextOut(midiout, SysExHdr, Cmd, c_CanalText);

                            strcpy(Cmd, "12");
                            sprintf(c_Canal, "%02X", j-(NbMidiFader*AddrMidiTrack));
                            strcat(Cmd, c_Canal);
                            strcat(Cmd, "0000");
							SendSysExTextOut(midiout, SysExHdr, Cmd, NameChannel[j]);
						}
                    }
			}
			else if (MidiCC == IdStop){                                                                                                                     /*  TRANSPORT STOP button for Track view with Led  */

				if(MidiValue == 0x7F){
						printf("STOP MTK\n");
						MtkPlay = 0;

						char sendui[256];
						sprintf(sendui,"SETD^var.mtk.currentState^0\n");
						send(sock , sendui, strlen(sendui) , 0 );

						char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);
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

                        if (MidiCC == i_ConfirmStopUI2Mcp && MidiValue == 0x7F){
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
			else if (MidiCC == IdPlay){                                                                                                                     /*  TRANSPORT PLAY button for Track view with Led  */

				if(MidiValue == 0x7F){
					if(MtkPlay == 0){
						printf("PLAY MTK\n");
						MtkPlay = 1;

						char sendui[256];
						sprintf(sendui,"SETD^var.mtk.currentState^2\n");
						send(sock , sendui, strlen(sendui) , 0 );

						char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                        SendMidiOut(midiout, MidiArray);
					}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}
			else if (MidiCC == IdRec){                                                                                                                     /*  TRANSPORT REC button for Track view with Led  */

				if(MidiValue == 0x7F && MtkRec ==0){
					MtkRec = 1;
					char sendui[256];
					sprintf(sendui,"SETD^var.mtk.currentState^1\n");
					send(sock , sendui, strlen(sendui) , 0 );

					sprintf(sendui,"SETD^var.mtk.rec.busy^0\n");
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                    SendMidiOut(midiout, MidiArray);

				}
				else if(MidiValue == 0x7F && MtkRec ==1){
					MtkRec = 0;
					char sendui[256];
					sprintf(sendui,"SETD^var.mtk.currentState^0\n");
					send(sock , sendui, strlen(sendui) , 0 );

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                    SendMidiOut(midiout, MidiArray);
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}
			else if (MidiCC == i_Dim){                                                                                                                     /*  DIM Master  */

				if(MidiValue == 0x7F && DimMaster ==0){
					DimMaster = 1;
					char sendui[256];
					sprintf(sendui,"SETD^m.dim^1\n");
					send(sock , sendui, strlen(sendui) , 0 );
					printf("Dim ON\n");

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x7F};
                    SendMidiOut(midiout, MidiArray);
				}
				else if(MidiValue == 0x7F && DimMaster ==1){
					DimMaster = 0;
					char sendui[256];
					sprintf(sendui,"SETD^m.dim^0\n");
					send(sock , sendui, strlen(sendui) , 0 );
					printf("Dim OFF\n");

					char MidiArray[3] = {AddrMidiButtonLed, MidiCC, 0x00};
                    SendMidiOut(midiout, MidiArray);
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}
			else if (MidiCC == i_SnapShotNavUp){                                                                                                                     /*  Snapshot Navigation up  */

				if(MidiValue == 0x7F){
					printf("Marker Left (SnapShot Nagigation)\n");

					if(SnapShotIndex > 0){
						SnapShotIndex--;
					}
					printf("SnapShot(%i/%i) = [%s]\n", SnapShotIndex, SnapShotMax, UISnapShotList[SnapShotIndex]);

					char sendui[256];
					sprintf(sendui,"LOADSNAPSHOT^Studio Stephan^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

					sprintf(sendui,"MSG^$SNAPLOAD^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}
			else if (MidiCC == i_SnapShotNavDown){                                                                                                                     /*  Snapshot navigation down  */

				if(MidiValue == 0x7F){
					printf("Marker Right (SnapShot Nagigation)\n");

					if(SnapShotIndex < SnapShotMax){
						SnapShotIndex++;
					}
					printf("SnapShot(%i/%i) = [%s]\n", SnapShotIndex, SnapShotMax, UISnapShotList[SnapShotIndex]);

					char sendui[256];
					sprintf(sendui,"LOADSNAPSHOT^Studio Stephan^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

					sprintf(sendui,"MSG^$SNAPLOAD^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}
			else{
			}
		}
		else{
			//printf("Midi IN: %02X %02X %02X\n",InMidi, MidiCC, MidiValue);
		}
	}

	//------------------------------------------------------------------------------------------------------------------------
	// Clear Midibuffer
	for (j = 0; j < status; j++){
		InMidi = Midibuffer[j];
		//printf("Status %i InMidi %s\n", status, InMidi);
	}

	if(debug == 2){
		for (j = 0; j < UIChannel; j++){
			for (i = 0; i < 5; i++){
				printf("Midi Controler CanalId %i Value %i \n", j,MidiTable [j][i]);
			}
		}
	}
 }while(!stop);

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
/* End of the program */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */

 /*  Reset Button light on Midi Controler  */
 for (j = 0; j < NbMidiFader; j++){
    char MidiArray[3] = {AddrMidiButtonLed, AddrMidiMute+j, 0x00};
    SendMidiOut(midiout, MidiArray);
 }

 for (j = 0; j < NbMidiFader; j++){
 	char MidiArray[3] = {AddrMidiButtonLed, AddrMidiSolo+j, 0x00};
    SendMidiOut(midiout, MidiArray);
 }

 for (j = 0; j < NbMidiFader; j++){
	char MidiArray[3] = {AddrMidiButtonLed, AddrMidiRec+j, 0x00};
    SendMidiOut(midiout, MidiArray);
 }

 for (j = 0; j < NbMidiFader; j++){
    int MidiValue = 0;
    char MidiArray[3] = {AddrMidiMix+j, MidiValue, MidiValue};
    SendMidiOut(midiout, MidiArray);
 }

 for (j = 0; j < NbMidiFader; j++){
    char Cmd[32] = "";
    char c_Canal[32] = "";

    strcpy(Cmd, "13");
    sprintf(c_Canal, "%02X", j);
    strcat(Cmd, c_Canal);
    strcat(Cmd, "10");
    SendSysExTextOut(midiout, SysExHdr, Cmd, "");
 }

 char testledsoff[1024] = {0x90, 0x56, 0x00, 0x90, 0x5B, 0x00, 0x90, 0x5D, 0x00, 0x90, 0x5C, 0x00, 0x90, 0x5D, 0x00, 0x90, 0x5E, 0x00, 0x90, 0x5F, 0x00};
 SendMidiOut(midiout, testledsoff);

 char ForwardLedOff[3] = {AddrMidiButtonLed, 0x5B, 0x00};
 SendMidiOut(midiout, ForwardLedOff);

 LogTrace(hfErr, debug, "UI2MCP --> UI : IOSYS^Disconnexion UI2MCP\n");
 command = "IOSYS^Disconnexion UI2MCP\n";
 send(sock , command, strlen(command) , 0 );

 LogTrace(hfErr, debug, "UI2MCP : Close MIDI socket\n");
 snd_rawmidi_close(midiout);
 snd_rawmidi_close(midiin);
 midiout = NULL;   // snd_rawmidi_close() does not clear invalid pointer,
 midiin = NULL;     // snd_rawmidi_close() does not clear invalid pointer,

 char sendui[256];
 sprintf(sendui,"QUIT\n");
 send(sock , sendui, strlen(sendui) , 0 );
 LogTrace(hfErr, debug, "UI2MCP  --> Ui : Quit\n");

 LogTrace(hfErr, debug, "UI2MCP : Close HTTP socket\n");
 close(sock);

 /*  End of the program  */

 LogTrace(hfErr, debug, "UI2MCP : End\n");
 return EXIT_SUCCESS;

}
