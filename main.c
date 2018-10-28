//
// Programmer:    Stephan Allene <stephan.allene]gmail.com>
// Creation Date: Sat October  14 18:57:48 PDT 2009
// Last Modified: Sat May  9 19:48:31 PDT 2009
// Filename:      alsarawmidiout.c
// Syntax:        C; ALSA 1.0
// $Smake:        gcc -o ui2mcp ui2mcp.c -lasound -Wall
//				  gcc -o %b %f -lasound
//
// Description:	  Send a MIDI note to a synthesizer using the ALSA rawmidi
//                interface.  Reverse engineered from amidi.c (found in
//                ALSA utils 1.0.19 program set).
//
// First double-check that you have the ALSA system installed on your computer
// by running this command-line command:
//    cat /proc/asound/version
// Which should return something like:
//   Advanced Linux Sound Architecture Driver Version 1.0.17.
// This example program should work if the version number (1.0.17 in this
// case) is "1".
//

#include <alsa/asoundlib.h>     /* Interface to the ALSA system */
#include <unistd.h>             /* for sleep() function */
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <math.h>

#define PORT 80
#define SOCKET_ERROR -1
#define TRUE = 1
#define FALSE = 0

//Drapeau d'arrêt du programme. Sa valeur d'origine est 0.
unsigned short stop = 0;

/***********************************
* Fonction de reception des signaux.
* Si le signal reçu est SIGINT Ctl^c, alors on increment le nombre de signaux de ce type reçu.
* Quand ce nombre est egal a 5 on met stop à 1.
************************************/
void signalHandler(int signal){
	if(signal==SIGINT || signal==SIGKILL){
		static unsigned short nbSignal = 0;
		nbSignal++;
		printf("Signal Ctrl^c n°%i reçu\n",nbSignal);
		if(nbSignal==1) stop=1;
	}
}

// function declarations:
void errormessage(const char *format, ...);

long currentTimeMillis() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

char **str_split (char *s, const char *ct)
{
  char **tab = NULL;

  if (s != NULL && ct != NULL)
  {
    int i;
    char *cs = NULL;
    size_t size = 1;

    for (i = 0; (cs = strtok (s, ct)); i++)
    {
      if (size <= i + 1)
      {
        void *tmp = NULL;

        size <<= 1;
        tmp = realloc (tab, sizeof (*tab) * size);
        if (tmp != NULL)
        {
          tab = tmp;
        }
        else
        {
          fprintf (stderr, "Memoire insuffisante\n");
          free (tab);
          tab = NULL;
          exit (EXIT_FAILURE);
        }
      }
      tab[i] = cs;
      s = NULL;
    }
    tab[i] = NULL;
  }
  return tab;
}

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

///////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {

    // Main variale & MIDI
	char clavier;
	int i,j,z;
	int status;
	//int mode = SND_RAWMIDI_SYNC;
	int mode = SND_RAWMIDI_NONBLOCK;
	snd_rawmidi_t* midiout = NULL;
	snd_rawmidi_t* midiin = NULL;
	int current_time_init;
	int current_time;

    // Network websocket
	struct sockaddr_in address;
    int sock = 0;
    struct sockaddr_in serv_addr;
	char *ack = "GET /raw HTTP1.1\n\n";
	char *command = "";
    //char buffer[1024] = "";
    char buffer[262144] = "";

	int bpm = 120;
	int delay = 60000/bpm;

	int debug = 0;

	// Parameter of UI device
	int init = 0;
	int UIChannel = 24;

	int AdrSplitComa, ArraySplitDot;
	char UImsg[64];
	char UIio[64];
	char UIfunc[64];
	char UIval[64];
	char UIchan[24];
	char** SplitArrayComa;
	char** SplitArrayDot;

	float zeroDbPos = .7647058823529421;  //   0 db
	float maxDb = 1;					  // +10 db

	char *uimodel = "";
	char *uicommand = "";
	char *uisnapshot = "";

	//UI MuteMask
	char UIMuteMask[16] = "";
	unsigned GroupMaskMute[5];

	//UI SnapShot
	char UIShow[256] = ""; //"Studio Stephan";
	char UISnapShotList [200][256];
	int SnapShotIndex = 0;
	int SnapShotMax = 0;
	char SnapShotCurrent[256] = "";

	int UiChannelsolo[24];
	int UiChannelmute[24];
	int UiChannelpan[24];
	int UiChannelmix[24];
	int Uibpm;

	// Parameter of MIDI device
	char *ControlerMode = "MCP"; //"CC"
	//mapping midi controler to UI for fader 0
	int AddrMidiMix = 0xE0;
	int AddrMidiRec = 0x00;
	int AddrMidiMute = 0x10;
	int AddrMidiSolo = 0x08;
	int AddrMidiPan = 0x10;

	unsigned char Midibuffer[2048];        // Storage for input buffer received
	int Canal;

	int NbMidiFader = 8;

	int AddrMidiTrack = 0;	// value of translation fader

	int NbMidiTrack = UIChannel/NbMidiFader;

	const char* portname = "hw:1,0,0";  // see alsarawportlist.c example program

//	int MidiTable [NbMidiFader][5];		// Array of Mix, Mute, Solo, Rec
//	double PanMidi[NbMidiFader];		// Pan
	int MidiTable [UIChannel][7];		// Array of Mix, Mute, Solo, Rec, MaskMute
	int Mix = 1; 						// Fader MIX //MidiTable [Fader][1] = Mix Potentiometer
	int Solo = 2;						// Solo button //MidiTable [Fader][2] = Solo Button
	int Mute = 3; 						// Mute button //MidiTable [Fader][3] = Mute Button
	int Rec = 4;						// Rec button //MidiTable [Fader][4] = Rec Button
	int MaskMute = 6;					// Mute button //MidiTable [Fader][6] = Mute Button with UI Mask
	int MaskMuteValue = 7;				// Mute button //MidiTable [Fader][7] = Mute Button with UI Mask

	int MtkStop;						// Value for Mtk STOP
	int MtkPlay;						// Value for Mtk PLAY
	int MtkRec;							// Value for Mtk REC
	int DimMaster;						// Value for Dim Master
	double PanMidi[UIChannel];			// Pan //MidiTable = Pan potentiometer 0=left 0.5=center 1=right
	double MixMidi[UIChannel];			// Mix //MidiTable = Pan potentiometer 0=left 0.5=center 1=right

	//const char *InMidi = "";
	int InMidi, MidiValue, MidiCC;
	//const int MidiCMD = 0xB0;

	/**********************************************************
	MISE EN PLACE DE LA GESTION DU SIGNAL CONTROL^C (SIGINT)
	**********************************************************/
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
	/*********************************************************/

	if ((argc > 1) && (strncmp("hw:", argv[1], 3) == 0)) {
      portname = argv[1];
	}

	// Connection MIDI device
	if ((status = snd_rawmidi_open(&midiin, &midiout, portname, mode)) < 0) {
      errormessage("Problem opening MIDI input & output: %s", snd_strerror(status));
      exit(1);
	}

if(debug != 1){

	// Connection socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "192.168.0.10", &serv_addr.sin_addr)<=0)
//    if(inet_pton(AF_INET, "192.168.0.151", &serv_addr.sin_addr)<=0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    send(sock , ack , strlen(ack) , 0 );
    printf("MESSAGE GET send\n");
}
	//sleep(1);

	//char noteon[3] = {0x90, 0x56, 0x7F};
	//char noteoff[3] = {0x90, 0x56, 0x00};

	//Init value array
	for (j = 0; j < UIChannel; j++){
		for (i = 0; i < 8; i++){
			MidiTable [j][i] = 0;
		}
		PanMidi[j] = .5;			// Pan //MidiTable = Pan potentiometer 0=left 0.5=center 1=right
	}
	for (j = 0; j < 6; j++){
		GroupMaskMute[j] = 0;
		//printf("j %i\n",j);
	}

	// Reset Button light
	for (j = 0; j < NbMidiFader; j++){
		//printf("Test :%0X\n",  AddrMidiMute+j);
		char noteon1[3] = {0x90, AddrMidiMute+j, 0x00};
		if ((status = snd_rawmidi_write(midiout, noteon1, sizeof(noteon1))) < 0) {
			errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
			exit(1);
		}
	}

	for (j = 0; j < NbMidiFader; j++){
		//printf("Test :%0X\n",  AddrMidiMute+j);
		char noteon1[3] = {0x90, AddrMidiSolo+j, 0x00};
		if ((status = snd_rawmidi_write(midiout, noteon1, sizeof(noteon1))) < 0) {
			errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
			exit(1);
		}
	}

	for (j = 0; j < NbMidiFader; j++){
		//printf("Test :%0X\n",  AddrMidiMute+j);
		char noteon1[3] = {0x90, AddrMidiRec+j, 0x00};
		if ((status = snd_rawmidi_write(midiout, noteon1, sizeof(noteon1))) < 0) {
			errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
			exit(1);
		}
	}

	// CC mode
	char noteon[3]  = {0xB0, 0x3E, 0x7F};
	char noteoff[3] = {0xB0, 0x3E, 0x00};

	//char cycleon[3]  = {0xB0, 0x2E, 0x7F};
	//char cycleoff[3]  = {0xB0, 0x2E, 0x00};

	char testledson[1024] = {0x90, 0x56, 0x7F, 0x90, 0x5B, 0x7F, 0x90, 0x5D, 0x7F, 0x90, 0x5C, 0x7F, 0x90, 0x5D, 0x7F, 0x90, 0x5E, 0x7F, 0x90, 0x5F, 0x7F};
	printf("All LEDs on %i\n", sizeof(testledson));
	if ((status = snd_rawmidi_write(midiout, testledson, sizeof(testledson))) < 0) {
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}
	sleep(1);  // pause the program for one second to allow note to sound.

	char testledsoff[1024] = {0x90, 0x56, 0x00, 0x90, 0x5B, 0x00, 0x90, 0x5D, 0x00, 0x90, 0x5C, 0x00, 0x90, 0x5D, 0x00, 0x90, 0x5E, 0x00, 0x90, 0x5F, 0x00};
	printf("All LEDs off %i\n", sizeof(testledson));
	if ((status = snd_rawmidi_write(midiout, testledsoff, sizeof(testledsoff))) < 0) {
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}

	// Led Forward On pour AddrMidiTrack = 0
	char ForwardLedOn[3] = {0x90, 0x5B, 0x7F};
	if ((status = snd_rawmidi_write(midiout, ForwardLedOn, 3)) < 0) {
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}

z=0;
current_time_init = currentTimeMillis();
printf("Demarrage Midi2Ui...\n");
do {

	//putchar(clavier);

	z++;
	if (z >= 100 && debug != 1)
	{
		z = 0;
		//printf("ALIVE ***********************************************************************************\n");
		command = "ALIVE\n";
		send(sock , command, strlen(command) , 0 );
	}

 	// Test de timer pour TAP
	delay = 60000/bpm;
	current_time = currentTimeMillis();
	if (current_time >= (current_time_init + delay))
	{
		char bpmon[3]  = {0x90, 0x56, 0x7F};
		char bpmoff[3] = {0x90, 0x56, 0x00};
		if ((status = snd_rawmidi_write(midiout, bpmon, 3)) < 0) {
			errormessage("Problem writing to MIDI output1: %s", snd_strerror(status));
			exit(1);
		}
		usleep( 25000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
		if ((status = snd_rawmidi_write(midiout, bpmoff, 3)) < 0) {
			errormessage("Problem writing to MIDI output1: %s", snd_strerror(status));
			exit(1);
		}
		current_time_init = currentTimeMillis();
	}

	if ((status = snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer))) < 0)
	{
		//errormessage("Problem reading MIDI input: %s", snd_strerror(status));
		//break;

		memset(buffer, 0, sizeof(buffer));
		if(recv(sock, buffer, sizeof(buffer), 0) != SOCKET_ERROR)
		{

			char** ArrayBuffer=NULL;
			char** UIMessage=NULL;
			int AdrSplitCRLF;
			char** SplitArrayCRLF;
			int sizeStr;
			int sizeUIMessage;

			if( debug == 1){printf("Len %i Recu : [%s]\n", strlen(buffer), buffer);}

			////char* str = "";
			//char* str="SETD^a.4.eq.peak.18^0.5\r\nSETD^a.4.dyn.gain^0.75\r\nSETD^a.4.dyn.threshold^0.875\r\nSETD^a.4.dyn.softknee^0\r\nSETD^a.4.dyn.ratio^1\r\nSETD^a.4.dyn.release^0.4887695312\r\nSETD^a.4.dyn.attack^0.34375\r\nSETD^a.4.gate.enabled^1\r\nSETD^a.4.dyn.outgain^0.3334960938\r\nSETD^a.4.eq.peak.29^0.5\r\nSETD^a.4.eq.peak.28^0.5\r\nSETD^a.4.eq.hpf^0\r\nSETD^a.4.eq.peak.30^0.5\r\nSETD^a.4.eq.lpf^1\r\nSETD^a.4.dyn.bypass^0\r\nSETD^a.4.dyn.prmod^0\r\nSETD^a.4.gate.prmod^0";
			//strcpy(str, (char)UIMessage);
			sizeStr=strlen(buffer);
			//UIMessage= (char**) realloc(buffer,sizeof(char*)*sizeStr);
			ArrayBuffer=(char*) malloc( sizeof(char)*(sizeStr+1) );
			strncpy(ArrayBuffer,buffer,sizeStr);
			//strcpy(UIMessage[sizeof(buffer)-1],buffer);

			if( debug == 1){printf("ArrayBuffer - [%s]\n", ArrayBuffer);}

			SplitArrayCRLF=split(ArrayBuffer,"\n",1);
			for(AdrSplitCRLF=0;SplitArrayCRLF[AdrSplitCRLF]!=NULL;AdrSplitCRLF++) {

				if( debug == 1){printf("SplitArrayCRLF - [%s]\n", SplitArrayCRLF[AdrSplitCRLF]);}

				sizeUIMessage=strlen(SplitArrayCRLF[AdrSplitCRLF]);
				UIMessage=(char*) malloc( sizeof(char)*(sizeUIMessage+1) );
				strncpy(UIMessage,SplitArrayCRLF[AdrSplitCRLF],sizeUIMessage);

				//sleep(2);

				//char UIMessage = SplitArrayCRLF[AdrSplitCRLF];

				char *MODEL = strstr(UIMessage, "MODEL^");
				char *SETD = strstr(UIMessage, "SETD^");
				char *RTA = strstr(UIMessage, "RTA^");
				char *SNAPSHOTLIST = strstr(UIMessage, "SNAPSHOTLIST^");
				char *SNAPSHOT = strstr(UIMessage, "currentSnapshot^");
				//char *LISTEN = strstr(buffer, "\r\n");

				if(strlen(UIMessage) == 1)
				{
					//printf("Len %i Recu : [%s]\n", strlen(buffer), buffer);

	//				if(init == 1){
	//					init = 2;
	//					command = "INIT\n";
	//					printf("Send command INIT\n");
	//					if( debug != 1){send(sock , command, strlen(command) , 0 );}
	//				}
					if(init == 0){
						init = 1;

						command = "IOSYS^Connexion UI2MCP\n";
						printf("Send command IOSYS\n");
						if( debug != 1){send(sock , command, strlen(command) , 0 );}

						command = "MODEL\n";
						printf("Send command MODEL\n");
						if( debug != 1){send(sock , command, strlen(command) , 0 );}
					}
				}

				if(RTA)
				{
					//printf("Recu : %s\n", UIMessage);
				}
				if(strstr(UIMessage, "currentShow^"))
				{
					//SETS^var.currentShow^Studio Stephan

					uisnapshot = strtok(strstr(UIMessage,"currentShow^"), "\r\n");
					printf("Recu [%s]\n",uisnapshot);

					SplitArrayComa=split(uisnapshot,"^",1);
					//for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++) {
					//	if (AdrSplitComa == 1){
					//		printf("i=%d : %s\n",AdrSplitComa,SplitArrayComa[AdrSplitComa]);
					//		//strcat(UISnapShotList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
							strcat(UIShow, SplitArrayComa[1]);
							//SnapShotCurrent = SplitArrayComa[1];
					//	}
					//}
					free(SplitArrayComa);
					//strcat(SnapShotCurrent, SplitArrayComa[1]);
					//SnapShotCurrent = SplitArrayComa[1];
//					printf("Recu [%s]\n",UIShow);

					char CmdSnapShot[256];
					//strcat(command, "SNAPSHOTLIST^");
					//strcat(command, UIShow);
					//strcat(command, "\n");
					sprintf(CmdSnapShot,"SNAPSHOTLIST^%s\n", UIShow);
					//command = "SNAPSHOTLIST^Studio Stephan\n";
					printf("Send command : %s\n", CmdSnapShot);
					send(sock , CmdSnapShot, strlen(CmdSnapShot) , 0 );


				}
				if(SNAPSHOT)
				{
					//3:::SETS^var.currentSnapshot^Snailz

					uisnapshot = strtok(strstr(UIMessage,"currentSnapshot^"), "\r\n");
					printf("Recu [%s]\n",uisnapshot);

					SplitArrayComa=split(uisnapshot,"^",1);
					//for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++) {
					//	if (AdrSplitComa == 1){
					//		printf("i=%d : %s\n",AdrSplitComa,SplitArrayComa[AdrSplitComa]);
					//		//strcat(UISnapShotList[AdrSplitComa-2],SplitArrayComa[AdrSplitComa]);
							strcat(SnapShotCurrent, SplitArrayComa[1]);
							//SnapShotCurrent = SplitArrayComa[1];
					//	}
					//}
					free(SplitArrayComa);
					//strcat(SnapShotCurrent, SplitArrayComa[1]);
					//SnapShotCurrent = SplitArrayComa[1];
					printf("Recu [%s]\n",SnapShotCurrent);

				}
				if(SNAPSHOTLIST)
				{
					uisnapshot = strtok(strstr(UIMessage,"SNAPSHOTLIST^"), "\r\n");
					printf("Recu [%s]\n",uisnapshot);

					SplitArrayComa=split(uisnapshot,"^",1);
					//affichage du resultat
					for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++) {
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
					printf("SnapShotIndex [%i]\n",SnapShotIndex);

				}
				if(MODEL)
				{
					uimodel = strtok(strstr(UIMessage,"MODEL^"), "\r\n");
					printf("Recu [%s]\n",uimodel);
					if(strstr(uimodel,"ui12")){
						UIChannel = 12;
					}else if (strstr(uimodel,"ui16")){
						UIChannel = 16;
					}else if (strstr(uimodel,"ui24")){
						UIChannel = 24;
					}else{
						printf("Sorry not supported.\n");
					}
				}
				if(strstr(UIMessage, "SETD^"))
				{
					//------------------------------------------------------------------------------------------------------------------------
					// Reception SETD message

					//printf("SETD - Len %i Recu : [%s]\n", strlen(UIMessage), UIMessage);
					//uicommand = strtok(strstr(UIMessage,"SETD^"), "\r\n");
					//printf("Recu : %s\n", buffer);
				//	printf("Recu %i [%s]\n", channel, uicommand);

				//	int AdrSplitComa, ArraySplitDot;
				//	char UImsg[64];
				//	char UIio[64];
				//	char UIfunc[64];
				//	char UIval[64];
				//	char UIchan[24];
				//	//char* str="SETD^i.0.pan^0.544450";
				//	char** SplitArrayComa;
				//	char** SplitArrayDot;

					//printf("Chaine initiale : %s \n",str);

					//------------------------------------------------------------------------------------------------------------------------
					// Split de la chaine recu via websocket

					SplitArrayComa=split(UIMessage,"^",1);
					//affichage du resultat
					for(AdrSplitComa=0;SplitArrayComa[AdrSplitComa]!=NULL;AdrSplitComa++) {
						if(AdrSplitComa==1){
							SplitArrayDot=split(SplitArrayComa[AdrSplitComa],".",1);
							//affichage du resultat
							for(ArraySplitDot=0;SplitArrayDot[ArraySplitDot]!=NULL;ArraySplitDot++) {
								//printf("j=%d : %s\n",j,SplitArrayDot[j]);
								if (ArraySplitDot == 0){ sprintf(UIio,"%s",SplitArrayDot[ArraySplitDot]); }
								if (ArraySplitDot == 1){ sprintf(UIchan,"%s",SplitArrayDot[ArraySplitDot]); }
								if (ArraySplitDot == 2){ sprintf(UIfunc,"%s",SplitArrayDot[ArraySplitDot]); }
								//au passge je désalloue les chaines
								free(SplitArrayDot[ArraySplitDot]);
							}
						}else if(AdrSplitComa!=1){
							//printf("i=%d : %s\n",AdrSplitComa,SplitArrayComa[AdrSplitComa]);
							if (AdrSplitComa == 0){ sprintf(UImsg,"%s",SplitArrayComa[AdrSplitComa]); }
							if (AdrSplitComa == 2){ sprintf(UIval,"%s",SplitArrayComa[AdrSplitComa]); }
							//au passge je désalloue les chaines
							free(SplitArrayComa[AdrSplitComa]);
						}
					}
					free(SplitArrayComa);
					free(SplitArrayDot);

					//printf("UI Message: %s\n", UImsg);
					//printf("Type IO: %s\n", UIio);
					//printf("Channel: %i\n", atoi(UIchan));
					//printf("Function: %s\n", UIfunc);
					////printf("Value: %f\n", atof(UIval));
					//printf("Value: %i\n", atoi(UIval));

	/*
					REC Multitrack fonction
					SETD^i.0.mtkrec^0			//Mode REC sur une piste

					Recu 24 [SETD^var.mtk.rec.currentState^1]  // Mode Record
					Recu 24 [SETD^var.mtk.rec.currentState^0]  // Stop Record

					Recu 24 [SETD^var.mtk.rec.time^14]	// Second d'enregistrement

					Recu 24 [SETD^var.mtk.currentTrackPos^0.0003866768849]	//Curdeur lecture

					Recu 24 [SETD^var.mtk.currentState^2]  //Play
					Recu 24 [SETD^var.mtk.currentState^1]	//Pause
					Recu 24 [SETD^var.mtk.currentState^0]	Stop

	*/
					if( strcmp(UIfunc,"bpm") == 0 )
					{
						//------------------------------------------------------------------------------------------------------------------------
						// Reception SETD - TAP
						printf( "bpm = %f\n", atof(UIval));
						bpm = atof(UIval);
					}else if( strcmp(UIio,"mgmask") == 0 || strcmp(UIfunc,"mgmask") == 0)
					{
						if( 1 == 1){printf("Message=%s\n", UIMessage);}
						//unsigned j = 0;
						int d;
						if( strcmp(UIio,"mgmask") == 0 )
						{
							strcat(UIMuteMask, UIval);
							printf("Mute Mask : %i\n", atoi(UIMuteMask));
							unsigned bit;
							for (bit = 0; bit < 6; bit++)
							{
								if( debug == 1){printf("bit %i : %i\n", bit, atoi(UIMuteMask) & (1u << bit));}
								GroupMaskMute[bit]= atoi(UIMuteMask) & (1u << bit);
							}

							for (Canal = 0; Canal < UIChannel; Canal++){
								if( (GroupMaskMute[0] == (MidiTable [Canal][MaskMuteValue] & (1u << 0)) && (MidiTable [Canal][MaskMuteValue] & (1u << 0)) != 0) ||
									(GroupMaskMute[1] == (MidiTable [Canal][MaskMuteValue] & (1u << 1)) && (MidiTable [Canal][MaskMuteValue] & (1u << 1)) != 0) ||
									(GroupMaskMute[2] == (MidiTable [Canal][MaskMuteValue] & (1u << 2)) && (MidiTable [Canal][MaskMuteValue] & (1u << 2)) != 0) ||
									(GroupMaskMute[3] == (MidiTable [Canal][MaskMuteValue] & (1u << 3)) && (MidiTable [Canal][MaskMuteValue] & (1u << 3)) != 0) ||
									(GroupMaskMute[4] == (MidiTable [Canal][MaskMuteValue] & (1u << 4)) && (MidiTable [Canal][MaskMuteValue] & (1u << 4)) != 0) ||
									(GroupMaskMute[5] == (MidiTable [Canal][MaskMuteValue] & (1u << 5)) && (MidiTable [Canal][MaskMuteValue] & (1u << 5)) != 0))
								{
									MidiTable [Canal][Mute] = 1;
									MidiTable [Canal][MaskMute]=1;
									//MidiTable [Canal][MaskMuteValue]=atoi(UIval);
								}else{
									MidiTable [Canal][Mute] = 0;
									MidiTable [Canal][MaskMute]=0;
									//MidiTable [Canal][MaskMuteValue]=atoi(UIval);
								}
								if( 1 == 1){
									printf("MidiTable Canal(%i) [Mute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, MidiTable [Canal][Mute], MidiTable [Canal][MaskMute], MidiTable [Canal][MaskMuteValue]);
									printf("MaskMute | Mute = %i]\n", MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]);
								}
								if ((MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) == 0){
									MidiTable [Canal][Mute] = 0;
									d = 0x00;
									printf("ICI mgmask %i maskmute 0\n", Canal);
								}else if ((MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) == 1){
									MidiTable [Canal][Mute] = 1;
									d = 0x7F;
									printf("ICI mgmask %i maskmute 1\n", Canal);
								}
								if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)
								{
									char noteon[3] = {0x90, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , MidiTable [Canal][Mute]*d};
									if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
										errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
										exit(1);
									}
								}
								//printf( "Canal %i AddrMute %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiMute, NbMidiFader, AddrMidiTrack, v);
							}
						}
						if( (strcmp(UIio,"i") == 0 && strcmp(UIfunc,"mgmask") == 0))
						{
							int d,v;
							Canal = atoi(UIchan);
							v = atoi(UIval);
							printf("Type IO: %s Channel: %i Function: %s Value: %i\n", UIio, atoi(UIchan),UIfunc,atoi(UIval));
							if( debug == 1){
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
								(GroupMaskMute[5] == (atoi(UIval) & (1u << 5)) && (atoi(UIval) & (1u << 5)) != 0))
							{
								MidiTable [Canal][Mute] = 1;
								MidiTable [Canal][MaskMute]=1;
								MidiTable [Canal][MaskMuteValue]=atoi(UIval);
							}else{
								MidiTable [Canal][Mute] = 0;
								MidiTable [Canal][MaskMute]=0;
								MidiTable [Canal][MaskMuteValue]=atoi(UIval);
							}
							if( 1 == 1){
								printf("MidiTable Canal(%i) [Mute=%i][MaskMute=%i][MaskMuteValue=%i]\n", Canal, MidiTable [Canal][Mute], MidiTable [Canal][MaskMute], MidiTable [Canal][MaskMuteValue]);
								printf("MaskMute | Mute = %i]\n", MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]);
							}

							//printf("Channel: %i\n", atoi(UIchan));
							//printf("Function: %s\n", UIfunc);
							//printf("Value: %i\n", atoi(UIval));

//							if (MidiTable [Canal][MaskMute]==0){
							if ((MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) == 0){
								MidiTable [Canal][Mute] =0;
								d = 0x00;
								printf("ICI io.mgmask %i maskmute 0\n", Canal);
							}else if ((MidiTable [Canal][MaskMute] | MidiTable [Canal][Mute]) == 1){
								MidiTable [Canal][Mute] = 1;
								d = 0x7F;
								printf("ICI io.mgmask %i maskmute 1\n", Canal);
							}
							if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)
							{
								char noteon[3] = {0x90, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , MidiTable [Canal][Mute]*d};
								if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
									errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
									exit(1);
								}
							}
							//printf( "Canal %i AddrMute %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiMute, NbMidiFader, AddrMidiTrack, v);
						}

					}else if( strcmp(UIio,"i") == 0 && (strcmp(UIfunc,"mute") == 0 || strcmp(UIfunc,"forceunmute") == 0))
					//}else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"mute") == 0 )
					{
						//printf("SETD - Len %i Recu : [%s]\n", strlen(UIMessage), UIMessage);
						//------------------------------------------------------------------------------------------------------------------------
						// Reception SETD - Mix Channel 0
						int d,v;
						Canal = atoi(UIchan);
						v = atoi(UIval);


/*						}else{
							if (v == 0 || MidiTable [Canal][MaskMute]==0){
								MidiTable [Canal][Mute] =0;
								d = 0x00;
								printf("ICI maskmute 0\n");
							}else if (v == 1 || MidiTable [Canal][MaskMute]==1){
								MidiTable [Canal][Mute] = 1;
								d = 0x7F;
								printf("ICI maskmute 1\n");
							}
*/

						if (strcmp(UIfunc,"forceunmute") == 0){
						//if (strcmp(UIfunc,"toto") == 0){
							if (v == 0 && MidiTable [Canal][MaskMute]==1){
								MidiTable [Canal][Mute] = 1;
								d = 0x7F;
								printf("ICI io-i %i forceunmute 0\n", Canal);
							}else if (v == 1 && MidiTable [Canal][MaskMute]==1){
								MidiTable [Canal][Mute] = 0;
								d = 0x00;
								printf("ICI io-i %i forceunmute 1\n", Canal);
							}
						}else if(strcmp(UIfunc,"mute") == 0){
							if (v == 0){
								MidiTable [Canal][Mute] = 0;
								d = 0x00;
								printf("ICI io-i %i mute 0\n", Canal);
							}else if (v == 1){
								MidiTable [Canal][Mute] = 1;
								d = 0x7F;
								printf("ICI io-i %i mute 1\n", Canal);
							}
						}

/*
						if (v == 0 || MidiTable [Canal][MaskMute]==0){
							MidiTable [Canal][Mute] =0;
							d = 0x00;
							printf("Function: %s\n", UIfunc);
							printf("ICI maskmute 0\n");
						}else if (v == 1 || MidiTable [Canal][MaskMute]==1){
							MidiTable [Canal][Mute] = 1;
							d = 0x7F;
							printf("Function: %s\n", UIfunc);
							printf("ICI maskmute 1\n");
						}
*/
						if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)
						{
							char noteon[3] = {0x90, AddrMidiMute+Canal-(NbMidiFader*AddrMidiTrack) , MidiTable [Canal][Mute]*d};
							if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
								errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
								exit(1);
							}
						}
						//printf( "Canal %i AddrMute %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiMute, NbMidiFader, AddrMidiTrack, v);
					}else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"mtkrec") == 0 )
					{
						//printf("SETD - Len %i Recu : [%s]\n", strlen(UIMessage), UIMessage);
						//------------------------------------------------------------------------------------------------------------------------
						// Reception SETD - Mix Channel 0
						int d,v;
						Canal = atoi(UIchan);
						v = atoi(UIval);
						if (v == 0){
							MidiTable [Canal][Rec] = 0;
							d = 0x00;
							//char noteon[3] = {0x90, AddrMidiRec+Canal-(NbMidiFader*AddrMidiTrack) , d};
							//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							//	exit(1);
							//}
						}else if (v == 1){
							MidiTable [Canal][Rec] = 1;
							d = 0x7F;
							//char noteon[3] = {0x90, AddrMidiRec+Canal-(NbMidiFader*AddrMidiTrack) , d};
							//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							//	exit(1);
							//}
						}

						if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)
						{
							char noteon[3] = {0x90, AddrMidiRec+Canal-(NbMidiFader*AddrMidiTrack) , MidiTable [Canal][Rec]*d};
							if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
								errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
								exit(1);
							}
						}
						//printf( "Canal %i AddrRec %02x NbMidiFader %02x AddrMidiTrack %i ValueRec %i\n", Canal, AddrMidiRec, NbMidiFader, AddrMidiTrack, v);
					}else if( strcmp(UIio,"m") == 0 && strcmp(UIchan,"dim") == 0 )
					{
						int d,v;
						v = atoi(UIval);
						if (v == 0){
							DimMaster = 0;
							d = 0x00;
							//char noteon[3] = {0x90, AddrMidiRec+Canal-(NbMidiFader*AddrMidiTrack) , d};
							//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							//	exit(1);
							//}
						}else if (v == 1){
							DimMaster = 1;
							d = 0x7F;
							//char noteon[3] = {0x90, AddrMidiRec+Canal-(NbMidiFader*AddrMidiTrack) , d};
							//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							//	exit(1);
							//}
						}
						//printf( "Canal %i AddrRec %02x NbMidiFader %02x AddrMidiTrack %i ValueRec %i\n", Canal, AddrMidiRec, NbMidiFader, AddrMidiTrack, v);
					}else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"solo") == 0 )
					{
						//printf("SETD - Len %i Recu : [%s]\n", strlen(UIMessage), UIMessage);
						//------------------------------------------------------------------------------------------------------------------------
						// Reception SETD - Mix Channel 0
						int d,v;
						Canal = atoi(UIchan);
						v = atoi(UIval);
						if (v == 0){
							MidiTable [Canal][Solo] = 0;
							d = 0x00;
							//char noteon[3] = {0x90, AddrMidiSolo+Canal-(NbMidiFader*AddrMidiTrack) , d};
							//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							//	exit(1);
							//}
						}else if (v == 1){
							MidiTable [Canal][Solo] = 1;
							d = 0x7F;
							//char noteon[3] = {0x90, AddrMidiSolo+Canal-(NbMidiFader*AddrMidiTrack) , d};
							//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							//	exit(1);
							//}
						}
						if(Canal >= NbMidiFader*AddrMidiTrack && Canal <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)
						{
							char noteon[3] = {0x90, AddrMidiSolo+Canal-(NbMidiFader*AddrMidiTrack) , MidiTable [Canal][Solo]*d};
							if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
								errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
								exit(1);
							}
						}
						//printf( "Canal %i AddrSolo %02x NbMidiFader %02x AddrMidiTrack %i ValueMute %i\n", Canal, AddrMidiSolo, NbMidiFader, AddrMidiTrack, v);
					}else if( strcmp(UIio,"i") == 0 && strcmp(UIfunc,"mix") == 0 )
					{
						//printf("UI Message: %s\n", UImsg);
						//printf("Type IO: %s\n", UIio);
						//printf("Channel: %i\n", atoi(UIchan));
						//printf("Function: %s\n", UIfunc);
						//printf("Value: %f\n", atof(UIval));
						//printf("Value: %i\n", atoi(UIval));

						//printf("SETD - Len %i Recu : [%s]\n", strlen(UIMessage), UIMessage);
						//------------------------------------------------------------------------------------------------------------------------
						// Reception SETD - Mix Channel 0
						int d;
						float v;
						Canal = atoi(UIchan);
						v = atof(UIval);

						//MixMidi
						//if (v == 0){
						//	MidiTable [Canal][Solo] = 0;
						//	d = 0x00;
						//	char noteon[3] = {0x90, AddrMidiSolo+Canal-(NbMidiFader*AddrMidiTrack) , d};
						//	if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						//		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						//		exit(1);
						//	}
						//}else if (v == 1){
						//	MidiTable [Canal][Solo] = 1;
						//	d = 0x7F;
						//	char noteon[3] = {0x90, AddrMidiSolo+Canal-(NbMidiFader*AddrMidiTrack) , d};
						//	if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						//		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						//		exit(1);
						//	}
						//}
						//printf( "Canal %i AddrMix %02x NbMidiFader %02x AddrMidiTrack %i ValueMix %f\n", Canal, AddrMidiSolo, NbMidiFader, AddrMidiTrack, v);
					}
				}
			}
			free(SplitArrayCRLF);
		}
	}else
	{
		//if(debug == 1){printf("Midi read [Control Change] ");}
		InMidi = (int)Midibuffer[0];
		MidiCC = (int)Midibuffer[j+1];
		MidiValue = (int)Midibuffer[j+2];
		if(debug == 1){printf("Midi IN: %02X %02X %02X\n",InMidi, MidiCC, MidiValue);}
		if(debug == 1){printf("Address Track : %i\n", AddrMidiTrack);}

		//------------------------------------------------------------------------------------------------------------------------
		// Fader MIX //MidiTable [Fader][1] = Mix Potentiometer
		if (InMidi >= 0xE0 && InMidi <= 0xE7)
		{
			Canal = InMidi % 0xE0 +(NbMidiFader*AddrMidiTrack);
			if(MidiCC == MidiValue){
				//------------------------------------------------------------------------------------------------------------------------
				// Clear Midibuffer
				int fs;
				float db;
				float unit = 0.0078740157480315;
				for (fs = 0; fs < status; fs+=3)
				{
					status = snd_rawmidi_read(midiin, &Midibuffer, sizeof(Midibuffer));
					if (InMidi >= 0xE0 && InMidi <= 0xE7)
					{
						InMidi = (int)Midibuffer[fs];
						MidiCC = (int)Midibuffer[fs+1];
						MidiValue = (int)Midibuffer[fs+2];
						db = MidiValue * unit;
						printf("Status %i Fader %i: %02X %02X, %.10f\n", status, Canal, MidiCC, MidiValue, db);

						char sendui[256];
						sprintf(sendui,"SETD^i.%d.mix^%.10f\n", Canal, db);
						send(sock , sendui, strlen(sendui) , 0 );
					}
				}
			}
		//------------------------------------------------------------------------------------------------------------------------
		// Pan //MidiTable [Fader][1] = Pan potentiometer 0=left 0.5=center 1=right
		}else if (InMidi == 0xB0)
		{
			if (MidiCC >= 0x10 && MidiCC <= 0x17)
			{
				Canal = MidiCC % 0x10 +(NbMidiFader*AddrMidiTrack);
				if(MidiValue == 0x41 && PanMidi[Canal] > 0)
				{
					PanMidi[Canal] =  fabs (PanMidi[Canal]-.03);
				}else if(MidiValue == 0x01 && PanMidi[Canal] < 1)
				{
					PanMidi[Canal] = PanMidi[Canal]+.03;
				}
				printf("Pan %i: %f\n", Canal, PanMidi[Canal]);

				char sendui[256];
				sprintf(sendui,"SETD^i.%d.pan^%.10f\n", Canal, PanMidi[Canal]);
				send(sock , sendui, strlen(sendui) , 0 );

				//usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//}else if (MidiCC == 0x90 && MidiValue == 0x56)
			//{
			//	//NULL;
			}
		}else if (InMidi == 0x90)
		{
			//------------------------------------------------------------------------------------------------------------------------
			// Rec button //MidiTable [Fader][4] = Rec Button
			if (MidiCC >= 0x00 && MidiCC <= 0x07)
			{
				//Canal = MidiCC;
				//Canal = MidiCC % 0x00 +(NbMidiFader*AddrMidiTrack);
				Canal = MidiCC +(NbMidiFader*AddrMidiTrack);
				//printf("MidiCC %i NbMidiRec %i AddrTack %i\n", MidiCC % 0x10, NbMidiFader, AddrMidiTrack);
				if(MidiValue == 0x7F && MidiTable [Canal][Rec] ==0)
				{
					MidiTable [Canal][Rec] = 1;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.mtkrec^1\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x7F};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}

				}else if(MidiValue == 0x7F && MidiTable [Canal][Rec] ==1)
				{
					MidiTable [Canal][Rec] = 0;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.mtkrec^0\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x00};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// Solo button //MidiTable [Fader][2] = Solo Button
			}else if (MidiCC >= 0x08 && MidiCC <= 0x0F)
			{

				Canal = MidiCC % 0x8 +(NbMidiFader*AddrMidiTrack);
				//printf("MidiCC %i NbMidiFader %i AddrTack %i\n", MidiCC % 0x8, NbMidiFader, AddrMidiTrack);

				//Init value array
				for (j = 0; j < UIChannel; j++){
					if(j != Canal)
					{
						MidiTable [j][Solo] = 0;
						char sendui[256];
						sprintf(sendui,"SETD^i.%d.solo^0\n", j);
						send(sock , sendui, strlen(sendui) , 0 );
					}

					//if(j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1)
					//{
					//	char noteon[3] = {0x90, MidiCC, 0x00};
					//	if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
					//		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
					//		exit(1);
					//	}
					//}
				}

				if(MidiValue == 0x7F && MidiTable [Canal][Solo] ==0)
				//if(MidiValue == 0x7F)
				{
					//}else if(MidiValue == 0x7F && MidiTable [Canal][Mute] ==1)
					MidiTable [Canal][Solo] = 1;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.solo^1\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x7F};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}

					for (j = 0; j < UIChannel; j++){
						if(MidiTable [j][Solo] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
						{
					//		char sendui[256];
					//		sprintf(sendui,"SETD^i.%d.solo^0\n", j);
					//		send(sock , sendui, strlen(sendui) , 0 );

							char noteon[3] = {0x90, AddrMidiSolo+(j % NbMidiFader), 0x00};
							if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
								errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
								exit(1);
							}
						}
					}
				usleep( 100000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
				}else if(MidiValue == 0x00 && MidiTable [Canal][Solo] ==1)
				//if(MidiValue == 0x7F)
				{
					//}else if(MidiValue == 0x7F && MidiTable [Canal][Mute] ==1)
					MidiTable [Canal][Solo] = 0;
					char sendui[256];
					sprintf(sendui,"SETD^i.%d.solo^0\n", Canal);
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x00};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}

					for (j = 0; j < UIChannel; j++){
						if(MidiTable [j][Solo] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
						{
					//		char sendui[256];
					//		sprintf(sendui,"SETD^i.%d.solo^0\n", j);
					//		send(sock , sendui, strlen(sendui) , 0 );

							char noteon[3] = {0x90, AddrMidiSolo+(j % NbMidiFader), 0x00};
							if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
								errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
								exit(1);
							}
						}
					}
				}
				//if(MidiValue == 0x7F && MidiTable [Canal][Solo] == 1)
				//{
				//	MidiTable [Canal][Solo] = 0;
				//	char sendui[256];
				//	sprintf(sendui,"SETD^i.%d.solo^0\n", Canal);
				//	send(sock , sendui, strlen(sendui) , 0 );
				//
				//	char noteon[3] = {0x90, MidiCC, 0x00};
				//	if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
				//		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
				//		exit(1);
				//	}
				//
				//	for (j = 0; j < UIChannel; j++){
				//		if(MidiTable [j][Solo] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
				//		{
				//	//		char sendui[256];
				//	//		sprintf(sendui,"SETD^i.%d.solo^0\n", j);
				//	//		send(sock , sendui, strlen(sendui) , 0 );
				//
				//			char noteon[3] = {0x90, AddrMidiSolo+(j % NbMidiFader), 0x00};
				//			if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
				//				errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
				//				exit(1);
				//			}
				//		}
				//	}
				//}
				usleep( 100000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// Mute button //MidiTable [Fader][3] = Mute Button
			}else if (MidiCC >= 0x10 && MidiCC <= 0x17)
			{

				Canal = MidiCC % 0x10 +(NbMidiFader*AddrMidiTrack);
				//printf("MidiCC %i NbMidiFader %i AddrTack %i\n", MidiCC % 0x10, NbMidiFader, AddrMidiTrack);
				if(MidiValue == 0x7F && MidiTable [Canal][Mute] ==0)
				{
					MidiTable [Canal][Mute] = 1;
					char sendui[256];
					if(MidiTable [Canal][MaskMute]==1)
					{
						sprintf(sendui,"SETD^i.%d.forceunmute^0\n", Canal);
						printf(sendui,"SETD^i.%d.forceunmute^0\n", Canal);
					}else
					{
						sprintf(sendui,"SETD^i.%d.mute^1\n", Canal);
						printf(sendui,"SETD^i.%d.mute^1\n", Canal);
					}
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x7F};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}

				}else if(MidiValue == 0x7F && MidiTable [Canal][Mute] ==1)
				{
					MidiTable [Canal][Mute] = 0;
					char sendui[256];
					if(MidiTable [Canal][MaskMute]==1)
					{
						sprintf(sendui,"SETD^i.%d.forceunmute^1\n", Canal);
						printf(sendui,"SETD^i.%d.forceunmute^1\n", Canal);
					}else
					{
						sprintf(sendui,"SETD^i.%d.mute^0\n", Canal);
						printf(sendui,"SETD^i.%d.mute^0\n", Canal);
					}
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x00};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// Rewind button for Track view with Led
			}else if (MidiCC == 0x2F)
			{
				//AddrMidiTrack
				//Canal = MidiCC % 0x10;
				if(MidiValue == 0x7F && AddrMidiTrack == 0)
				{
					AddrMidiTrack++;

					char RewindLedOn[3] = {0x90, 0x5C, 0x7F};
					if ((status = snd_rawmidi_write(midiout, RewindLedOn, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
				}else if(MidiValue == 0x7F && AddrMidiTrack < NbMidiTrack-1)
				{
					AddrMidiTrack++;

					char ForwardLedOn[3] = {0x90, 0x5B, 0x00};
					if ((status = snd_rawmidi_write(midiout, ForwardLedOn, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
					char RewindLedOn[3] = {0x90, 0x5C, 0x7F};
					if ((status = snd_rawmidi_write(midiout, RewindLedOn, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
				}
				//printf("Track Left: %i\n", AddrMidiTrack);
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Mute;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiMute+(j % NbMidiFader), 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiMute+(j % NbMidiFader), 0x00};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Solo;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiSolo+(j % NbMidiFader), 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiSolo+(j % NbMidiFader), 0x00};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Rec;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiRec+(j % NbMidiFader), 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiRec+(j % NbMidiFader), 0x00};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
			//------------------------------------------------------------------------------------------------------------------------
			// Forward button for Track view with Led
			}else if (MidiCC == 0x2E)
			{
				//AddrMidiTrack
				//Canal = MidiCC % 0x10;
				if(MidiValue == 0x7F && AddrMidiTrack == 0)
				{
					AddrMidiTrack == 0;

					char ForwardLedOn[3] = {0x90, 0x5B, 0x7F};
					if ((status = snd_rawmidi_write(midiout, ForwardLedOn, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
					char RewindLedOn[3] = {0x90, 0x5C, 0x00};
					if ((status = snd_rawmidi_write(midiout, RewindLedOn, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}


				}else if(MidiValue == 0x7F && AddrMidiTrack > 0)
				{
					AddrMidiTrack--;

					char ForwardLedOn[3] = {0x90, 0x5B, 0x7F};
					if ((status = snd_rawmidi_write(midiout, ForwardLedOn, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
					if(AddrMidiTrack == 0){
						char RewindLedOn[3] = {0x90, 0x5C, 0x00};
						if ((status = snd_rawmidi_write(midiout, RewindLedOn, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}
				}
				//printf("Track Left: %i\n", AddrMidiTrack);
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */

				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Mute;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiMute+(j % NbMidiFader), 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiMute+(j % NbMidiFader), 0x00};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Solo;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiSolo+(j % NbMidiFader), 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiSolo+(j % NbMidiFader), 0x00};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
				// Update Midi Controler with Array valuefor (j = 0; j < UIChannel; j++){
				i = Rec;
				//printf("Track N°%i to N°%i\n", NbMidiFader*AddrMidiTrack, (NbMidiFader*AddrMidiTrack)+NbMidiFader-1);
				for (j = 0; j < UIChannel; j++){
					if(MidiTable [j][i] == 1 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiRec+(j % NbMidiFader), 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}else if(MidiTable [j][i] == 0 && (j >= NbMidiFader*AddrMidiTrack && j <= (NbMidiFader*AddrMidiTrack)+NbMidiFader-1))
					{
						if( debug == 1){printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));}
						char noteon[3] = {0x90, AddrMidiRec+(j % NbMidiFader), 0x00};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}
					//printf("Midi Controler CanalId %i Value %i ModuloValue %i\n", j,MidiTable [j][i], AddrMidiMute+(j % NbMidiFader));
				}
			//------------------------------------------------------------------------------------------------------------------------
			// TRANSPORT STOP button for Track view with Led
			}else if (MidiCC == 0x5D)
			{
				//printf("MidiCC %i MidiValue %i\n", MidiCC, MidiValue);
				//Recu 24 [SETD^var.mtk.currentState^2]  //Play
				//Recu 24 [SETD^var.mtk.currentState^1]	//Pause
				//Recu 24 [SETD^var.mtk.currentState^0]	Stop
//				if(MidiValue == 0x7F && MtkPlay ==0)
//				{
//					MtkPlay = 1;
//					char sendui[256];
//					sprintf(sendui,"SETD^var.mtk.currentState^0\n");
//					send(sock , sendui, strlen(sendui) , 0 );
//
//					char noteon[3] = {0x90, MidiCC, 0x7F};
//					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
//						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
//						exit(1);
//					}
//
//				}else if(MidiValue == 0x7F && MtkPlay ==1)
				if(MidiValue == 0x7F)
				{
					//if(MtkPlay == 1){
						printf("STOP MTK\n");
						MtkPlay = 0;

						char sendui[256];
						sprintf(sendui,"SETD^var.mtk.currentState^0\n");
						send(sock , sendui, strlen(sendui) , 0 );

						char noteon[3] = {0x90, MidiCC, 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					//}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// TRANSPORT PLAY button for Track view with Led
			}else if (MidiCC == 0x5E)
			{
				//printf("MidiCC %i MidiValue %i\n", MidiCC, MidiValue);
				//Recu 24 [SETD^var.mtk.currentState^2]  //Play
				//Recu 24 [SETD^var.mtk.currentState^1]	//Pause
				//Recu 24 [SETD^var.mtk.currentState^0]	Stop
				if(MidiValue == 0x7F)
				{
					if(MtkPlay == 0){
						printf("PLAY MTK\n");
						MtkPlay = 1;

						char sendui[256];
						sprintf(sendui,"SETD^var.mtk.currentState^2\n");
						send(sock , sendui, strlen(sendui) , 0 );

						char noteon[3] = {0x90, MidiCC, 0x7F};
						if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
							errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
							exit(1);
						}
					}

//				}else if(MidiValue == 0x7F && MtkPlay ==1)
//				{
//					MtkPlay = 0;
//					char sendui[256];
//					sprintf(sendui,"SETD^var.mtk.currentState^0\n");
//					send(sock , sendui, strlen(sendui) , 0 );
//
//					char noteon[3] = {0x90, MidiCC, 0x7F};
//					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
//						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
//						exit(1);
//					}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// TRANSPORT REC button for Track view with Led
			}else if (MidiCC == 0x5F)
			{
				//printf("MidiCC %i MidiValue %i\n", MidiCC, MidiValue);
				//Recu 24 [SETD^var.mtk.currentState^2]  //Play
				//Recu 24 [SETD^var.mtk.currentState^1]	//Pause
				//Recu 24 [SETD^var.mtk.currentState^0]	Stop
				if(MidiValue == 0x7F && MtkRec ==0)
				{
					MtkRec = 1;
					char sendui[256];
					sprintf(sendui,"SETD^var.mtk.currentState^1\n");
					send(sock , sendui, strlen(sendui) , 0 );

					sprintf(sendui,"SETD^var.mtk.rec.busy^0\n");
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x7F};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}

				}else if(MidiValue == 0x7F && MtkRec ==1)
				{
					MtkRec = 0;
					char sendui[256];
					sprintf(sendui,"SETD^var.mtk.currentState^0\n");
					send(sock , sendui, strlen(sendui) , 0 );

					char noteon[3] = {0x90, MidiCC, 0x00};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// Marker SET button with Led
			}else if (MidiCC == 0x59)
			{
				//printf("MidiCC %i MidiValue %i\n", MidiCC, MidiValue);
				//Recu 24 [SETD^m.dim^0]
				//Recu 24 [SETD^m.dim^1]
				if(MidiValue == 0x7F && DimMaster ==0)
				{
					DimMaster = 1;
					char sendui[256];
					sprintf(sendui,"SETD^m.dim^1\n");
					send(sock , sendui, strlen(sendui) , 0 );
					printf("Dim ON\n");

					char noteon[3] = {0x90, MidiCC, 0x7F};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}

				}else if(MidiValue == 0x7F && DimMaster ==1)
				{
					DimMaster = 0;
					char sendui[256];
					sprintf(sendui,"SETD^m.dim^0\n");
					send(sock , sendui, strlen(sendui) , 0 );
					printf("Dim OFF\n");

					char noteon[3] = {0x90, MidiCC, 0x00};
					if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
						errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
						exit(1);
					}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// Marker left button
			}else if (MidiCC == 0x58)
			{
				//printf("MidiCC %i MidiValue %i\n", MidiCC, MidiValue);
				if(MidiValue == 0x7F)
				{
					printf("Marker Left\n");

					if(SnapShotIndex > 0){
						SnapShotIndex--;
					}
					printf("SnapShot(%i/%i) = [%s]\n", SnapShotIndex, SnapShotMax, UISnapShotList[SnapShotIndex]);

					char sendui[256];
					sprintf(sendui,"LOADSNAPSHOT^Studio Stephan^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

					sprintf(sendui,"MSG^$SNAPLOAD^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

					//char noteon[3] = {0x90, MidiCC, 0x7F};
					//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
					//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
					//	exit(1);
					//}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			//------------------------------------------------------------------------------------------------------------------------
			// Marker right button
			}else if (MidiCC == 0x5A)
			{
				//printf("MidiCC %i MidiValue %i\n", MidiCC, MidiValue);
				if(MidiValue == 0x7F)
				{
					printf("Marker Right\n");

					if(SnapShotIndex < SnapShotMax){
						SnapShotIndex++;
					}
					printf("SnapShot(%i/%i) = [%s]\n", SnapShotIndex, SnapShotMax, UISnapShotList[SnapShotIndex]);

					char sendui[256];
					sprintf(sendui,"LOADSNAPSHOT^Studio Stephan^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

					sprintf(sendui,"MSG^$SNAPLOAD^%s\n", UISnapShotList[SnapShotIndex]);
					send(sock , sendui, strlen(sendui) , 0 );

					//char noteon[3] = {0x90, MidiCC, 0x7F};
					//if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
					//	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
					//	exit(1);
					//}
				}
				usleep( 250000 ); /* Sleep 100000 micro seconds = 100 ms, etc. */
			}else
			{
				//printf("Midi IN: %02X %02X %02X\n",InMidi, MidiCC, MidiValue);
			}
		}else
		{
			//printf("Midi IN: %02X %02X %02X\n",InMidi, MidiCC, MidiValue);
		}
	}

	//------------------------------------------------------------------------------------------------------------------------
	// Clear Midibuffer
	for (j = 0; j < status; j++)
	{
		InMidi = Midibuffer[j];
		//printf("Status %i InMidi %s\n", status, InMidi);
	}

	if(debug == 1){
		for (j = 0; j < UIChannel; j++){
			for (i = 0; i < 5; i++){
				printf("Midi Controler CanalId %i Value %i \n", j,MidiTable [j][i]);
			}
		}
	}
}while(!stop);

// Reset Button light
for (j = 0; j < NbMidiFader; j++){
	//printf("Test :%0X\n",  AddrMidiMute+j);
	char noteon1[3] = {0x90, AddrMidiMute+j, 0x00};
	if ((status = snd_rawmidi_write(midiout, noteon1, sizeof(noteon1))) < 0) {
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}
}

for (j = 0; j < NbMidiFader; j++){
	//printf("Test :%0X\n",  AddrMidiMute+j);
	char noteon1[3] = {0x90, AddrMidiSolo+j, 0x00};
	if ((status = snd_rawmidi_write(midiout, noteon1, sizeof(noteon1))) < 0) {
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}
}

for (j = 0; j < NbMidiFader; j++){
	//printf("Test :%0X\n",  AddrMidiMute+j);
	char noteon1[3] = {0x90, AddrMidiRec+j, 0x00};
	if ((status = snd_rawmidi_write(midiout, noteon1, sizeof(noteon1))) < 0) {
		errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
		exit(1);
	}
}

printf("All LEDs off %i\n", sizeof(testledsoff));
if ((status = snd_rawmidi_write(midiout, testledsoff, sizeof(testledsoff))) < 0) {
	errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
	exit(1);
}

command = "IOSYS^Disconnexion UI2MCP\n";
printf("Send command IOSYS\n");
if( debug != 1){send(sock , command, strlen(command) , 0 );}


snd_rawmidi_close(midiout);
snd_rawmidi_close(midiin);
midiout = NULL;   // snd_rawmidi_close() does not clear invalid pointer,
midiin = NULL;    // snd_rawmidi_close() does not clear invalid pointer,
return 0;         // so might be a good idea to erase it after closing.

/************************
        FIN
************************/
return EXIT_SUCCESS;

}

///////////////////////////////////////////////////////////////////////////

//////////////////////////////
//
// error -- Print an error message.
//

void errormessage(const char *format, ...) {
   va_list ap;
   va_start(ap, format);
   vfprintf(stderr, format, ap);
   va_end(ap);
   putc('\n', stderr);
}


