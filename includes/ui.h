#ifndef UI_H
#define UI_H

//typedef struct UI UI;
struct UiI
{
    // Struture for In Media Line
	//int Channel;
    char Type;
    int Color;
	double PanMidi;
	double MixMidi;
	char NameChannel;
	int vuPre;
	int VuPost;
	int vuPostFader;
	int vuGateIn;
	int vuCompOut;
	int vuCompMeter;
	int Gate;
};

struct UiSubFx
{
    // Struture for Sub Fx
    //int Channel;
    char Type;
    int Color;
    double PanMidi;
	double MixMidi;
	char NameChannel;
	int vuPostL;
	int	vuPostR;
	int vuPostFaderL;
	int vuPostFaderR;
	int vuGateIn;
	int vuCompOut;
	int vuCompMeter;
	int Gate;
};

struct UiAux
{
    // Struture for Aux Master
    //int Channel;
    char Type;
    int Color;
	double MixMidi;
	char NameChannel;
	int VuPost;
	int vuPostFader;
	int vuGateIn;
	int vuCompOut;
	int vuCompMeter;
	int Gate;
};

struct UiMaster
{
    // Struture for Aux Master
    //int Channel;
    char Type;
    int Color;
	double PanMidi;
	double MixMidi;
	char NameChannel;
	int VuPost;
	int vuPostFader;
	int vuGateIn;
	int vuCompOut;
	int vuCompMeter;
	int Gate;
};

/*static const int UIColor0;
					Recu 24 [SETD^i.23.color^1]  =  Noir  	=> R0V0B0
					Recu 24 [SETD^i.23.color^2]  =  Bordeau => R139V0B0
					Recu 24 [SETD^i.23.color^3]  =  Rouge	=> R255V0B0
					Recu 24 [SETD^i.23.color^4]  =  Orange	=> R255V165B0
					Recu 24 [SETD^i.23.color^5]  =  Jaune	=> R255V255B0
					Recu 24 [SETD^i.23.color^6]  =  Vert  	=> R86V222B67
					Recu 24 [SETD^i.23.color^7]  =  Blue  	=> R0V145B194
					Recu 24 [SETD^i.23.color^8]  =  Magenta => R148V0B211
					Recu 24 [SETD^i.23.color^9]  =  Gris	=> R128V128B128
					Recu 24 [SETD^i.23.color^10] =  Blanc	=> R255V255B255
					Recu 24 [SETD^i.23.color^11] =  Cyan	=> R255V20B147

*/

#endif //UI_h
