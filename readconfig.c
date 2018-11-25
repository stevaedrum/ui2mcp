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

							if (i == 0){
									memcpy(ControlerConfig.ControlerName,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.imgserver);
							} else if (i == 1){
									memcpy(ControlerConfig.ControlerMode,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.ccserver);
							} else if (i == 2){
									memcpy(ControlerConfig.Lcd,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.port);
							} else if (i == 3){
									memcpy(ControlerConfig.NbMidiFader,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.imagename);
							} else if (i == 4){
									memcpy(ControlerConfig.AddrMidiMix,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 5){
									memcpy(ControlerConfig.AddrMidiEncoderPan,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 6){
									memcpy(ControlerConfig.AddrMidiPan,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 7){
									memcpy(ControlerConfig.AddrMidiButtonLed,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 8){
									memcpy(ControlerConfig.AddrMidiRec,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 9){
									memcpy(ControlerConfig.AddrMidiMute,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 10){
									memcpy(ControlerConfig.AddrMidiSolo,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 11){
									memcpy(ControlerConfig.AddrMidiSelect,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 12){
									memcpy(ControlerConfig.AddrMidiTouch,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 13){
									memcpy(ControlerConfig.IdTrackPrev,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 14){
									memcpy(ControlerConfig.IdTrackNext,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 15){
									memcpy(ControlerConfig.IdLoop,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 16){
									memcpy(ControlerConfig.IdMarkerSet,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 17){
									memcpy(ControlerConfig.IdMarkerLeft,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 18){
									memcpy(ControlerConfig.IdMarkerRight,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 19){
									memcpy(ControlerConfig.IdRewind,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 20){
									memcpy(ControlerConfig.IdForward,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 21){
									memcpy(ControlerConfig.IdStop,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 22){
									memcpy(ControlerConfig.IdPlay,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 23){
									memcpy(ControlerConfig.IdRec,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 24){
									memcpy(ControlerConfig.AddrMidiBar,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 25){
									memcpy(ControlerConfig.AddrMidiValueBar,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 26){
									memcpy(ControlerConfig.SysExHdr,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 27){
									memcpy(ControlerConfig.i_Tap,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 28){
									memcpy(ControlerConfig.i_Dim,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 29){
									memcpy(ControlerConfig.i_SnapShotNavUp,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 30){
									memcpy(ControlerConfig.i_SnapShotNavDown,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 31){
									memcpy(ControlerConfig.i_StopUI2Mcp,cfline,strlen(cfline)-1);
									//printf("%s",ControlerConfig.getcmd);
							} else if (i == 32){
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
