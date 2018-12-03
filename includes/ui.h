#ifndef UI_H
#define UI_H

//typedef struct UI UI;
struct Ui
{
    int Position;
    int Numb;
    char Name[256];
    int Solo;
	int Mute;
	int ForceUnMute;
	int MaskMute;
	int MaskMuteValue;
    char Type[4];
    int Rec;
    int Color;
	double PanMidi;
	double MixMidi;
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
};

//struct UiI
//{
//    // Struture for In Media Line
//    int Position;
//    char Name[256];
//    int Solo;
//	int Mute;
//	int ForceUnMute;
//	int MaskMute;
//	int MaskMuteValue;
//    char Type[4];
//    int Rec;
//    int Color;
//	double PanMidi;
//	double MixMidi;
//	int vuPre;
//	int vuPost;
//	int vuPostFader;
//	int vuGateIn;
//	int vuCompOut;
//	int vuCompMeter;
//	int Gate;
//};
//
//struct UiSubFx
//{
//    // Struture for Sub Fx
//    int Position;
//    char Name[256];
//    int Solo;
//	int Mute;
//	int ForceUnMute;
//	int MaskMute;
//	int MaskMuteValue;
//    char Type[4];
//    int Color;
//    double PanMidi;
//	double MixMidi;
//	int vuPostL;
//	int vuPostR;
//	int vuPostFaderL;
//	int vuPostFaderR;
//	int vuGateIn;
//	int vuCompOut;
//	int vuCompMeter;
//	int Gate;
//};
//
//struct UiAux
//{
//    // Struture for Aux Master
//    int Position;
//    char Name[256];
//    int Solo;
//	int Mute;
//	int ForceUnMute;
//	int MaskMute;
//	int MaskMuteValue;
//    char Type[4];
//    int Color;
//	double MixMidi;
//	int vuPost;
//	int vuPostFader;
//	int vuGateIn;
//	int vuCompOut;
//	int vuCompMeter;
//	int Gate;
//};
//
//struct UiVca
//{
//    // Struture for VCA
//    int Position;
//    char Name[256];
//	int Mute;
//	int ForceUnMute;
//	int MaskMute;
//	int MaskMuteValue;
//    char Type[4];
//    int Color;
//	double MixMidi;
//};
//
//struct UiMaster
//{
//    // Struture for Aux Master
//    int Position;
//	int Mute;
//	int ForceUnMute;
//	int MaskMute;
//	int MaskMuteValue;
//    char Type[4];
//    int Color;
//	double PanMidi;
//	double MixMidi;
//	int vuPost;
//	int vuPostFader;
//	int vuGateIn;
//	int vuCompOut;
//	int vuCompMeter;
//	int Gate;
//};

#endif //UI_h
