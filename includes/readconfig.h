#ifndef READCONFIG_H_INCLUDED
#define READCONFIG_H_INCLUDED

#define MAXBUF 1024

struct config
{
   char ControlerName[MAXBUF];
   char ControlerMode[MAXBUF];
   char Lcd[MAXBUF];
   char NbMidiFader[MAXBUF];
   char AddrMidiMix[MAXBUF];
   char AddrMidiEncoderPan[MAXBUF];
   char AddrMidiPan[MAXBUF];
   char AddrMidiButtonLed[MAXBUF];
   char AddrMidiRec[MAXBUF];
   char AddrMidiMute[MAXBUF];
   char AddrMidiSolo[MAXBUF];
   char AddrMidiTouch[MAXBUF];
   char IdTrackPrev[MAXBUF];
   char IdTrackNext[MAXBUF];
   char IdLoop[MAXBUF];
   char IdMarkerSet[MAXBUF];
   char IdMarkerLeft[MAXBUF];
   char IdMarkerRight[MAXBUF];
   char IdRewind[MAXBUF];
   char IdForward[MAXBUF];
   char IdStop[MAXBUF];
   char IdPlay[MAXBUF];
   char IdRec[MAXBUF];
   char AddrMidiBar[MAXBUF];
   char AddrMidiValueBar[MAXBUF];
   char SysExHdr[MAXBUF];
   char i_Tap[MAXBUF];
   char i_Dim[MAXBUF];
   char i_SnapShotNavUp[MAXBUF];
   char i_SnapShotNavDown[MAXBUF];
};

/**
 * Function for read and load in variable.
 */
struct config get_config(char *filename);

#endif // READCONFIG_H_INCLUDED
