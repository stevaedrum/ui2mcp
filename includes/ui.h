#ifndef UI_H
#define UI_H

struct Ui_Bus
{
    //char Type[4];
	int Mute;
	double PanMidi;
	double MixMidi;
};

//typedef struct UI UI;
struct Ui
{
    int Position;
    int Numb;
    char Name[256];
    int Solo;
	int Mute;
	int StereoIndex;
	int ForceUnMute;
	int MaskMute;
	int MaskMuteValue;
    char Type[4];
    int Rec;
    int Color;
	double PanMidi;
	double MixMidi;
	double GainMidi;
	int vuPre;
	int vuPost;
	int vuPostL;
	int vuPostR;
	int vuPostFader;
	int vuPostFaderL;
	int vuPostFaderR;
	int vuGateIn;
	int vuCompOut;
	int vuCompMeter;
	int Gate;
	/* structure variable for Aux, FX of UI  */
	struct Ui_Bus Aux[10];
	struct Ui_Bus Fx[4];
};

#endif //UI_h
