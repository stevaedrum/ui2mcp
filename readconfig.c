#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "includes/readconfig.h"

#define MAXBUF 1024
#define DELIM "="

struct config get_config(char *filename)
{
        struct config ControlerConfig;
        FILE *file = fopen (filename, "r");

        if (file != NULL){
                char line[MAXBUF];
                int i = 0;

                while(fgets(line, sizeof(line), file) != NULL)
                {
						if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
							//printf("Saut de ligne : %s", line);
							continue;
						}
						else{
							char *cfline;
							cfline = strstr((char *)line,DELIM);
							cfline = cfline + strlen(DELIM);

							if (strstr(line, "ControlerName")){
									memcpy(ControlerConfig.ControlerName,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.ControlerName);
							} else if (strstr(line, "ControlerMode")){
									memcpy(ControlerConfig.ControlerMode,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.ccserver);
							} else if (strstr(line, "MidiPort")){
									memcpy(ControlerConfig.MidiPort,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.ccserver);
							} else if (strstr(line, "UiAddr")){
									memcpy(ControlerConfig.UiAddr,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.ccserver);
							} else if (strstr(line, "SyncId")){
									memcpy(ControlerConfig.SyncId,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.ccserver);
							} else if (strstr(line, "Lcd")){
									memcpy(ControlerConfig.Lcd,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.port);
							} else if (strstr(line, "NbMidiFader")){
									memcpy(ControlerConfig.NbMidiFader,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.imagename);
							} else if (strstr(line, "AddrMidiMix")){
									memcpy(ControlerConfig.AddrMidiMix,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiEncoderPan")){
									memcpy(ControlerConfig.AddrMidiEncoderPan,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiPan")){
									memcpy(ControlerConfig.AddrMidiPan,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiButtonLed")){
									memcpy(ControlerConfig.AddrMidiButtonLed,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiRec")){
									memcpy(ControlerConfig.AddrMidiRec,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiMute")){
									memcpy(ControlerConfig.AddrMidiMute,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiSolo")){
									memcpy(ControlerConfig.AddrMidiSolo,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiSelect")){
									memcpy(ControlerConfig.AddrMidiSelect,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiTouch")){
									memcpy(ControlerConfig.AddrMidiTouch,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdTrackPrev")){
									memcpy(ControlerConfig.IdTrackPrev,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdTrackNext")){
									memcpy(ControlerConfig.IdTrackNext,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdLoop")){
									memcpy(ControlerConfig.IdLoop,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdMarkerSet")){
									memcpy(ControlerConfig.IdMarkerSet,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdMarkerLeft")){
									memcpy(ControlerConfig.IdMarkerLeft,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdMarkerRight")){
									memcpy(ControlerConfig.IdMarkerRight,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdRewind")){
									memcpy(ControlerConfig.IdRewind,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdForward")){
									memcpy(ControlerConfig.IdForward,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdStop")){
									memcpy(ControlerConfig.IdStop,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdPlay")){
									memcpy(ControlerConfig.IdPlay,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "IdRec")){
									memcpy(ControlerConfig.IdRec,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiBar")){
									memcpy(ControlerConfig.AddrMidiBar,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "AddrMidiValueBar")){
									memcpy(ControlerConfig.AddrMidiValueBar,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "SysExHdr")){
									memcpy(ControlerConfig.SysExHdr,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "i_Tap")){
									memcpy(ControlerConfig.i_Tap,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "i_Dim")){
									memcpy(ControlerConfig.i_Dim,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "i_SnapShotNavUp")){
									memcpy(ControlerConfig.i_SnapShotNavUp,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "i_SnapShotNavDown")){
									memcpy(ControlerConfig.i_SnapShotNavDown,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "i_StopUI2Mcp")){
									memcpy(ControlerConfig.i_StopUI2Mcp,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (strstr(line, "i_ConfirmStopUI2Mcp")){
									memcpy(ControlerConfig.i_ConfirmStopUI2Mcp,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							}
                        i++;
						}
                } // End while
                fclose(file);
        } // End if file



        return ControlerConfig;

}
