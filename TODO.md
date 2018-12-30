[ui2mcp] oject

#################################################################
    DONE
#################################################################

Take in account Mute ALL with "SETD^mgmask^8388608".  --> DONE

Correction of Mute / ForceUnmute with mutiple configuration:  --> DONE
    On channel --> ForceUnMute = MuteMask disable = Light on MIDI Controler!!!  ---> Corrected V0.9.0
    On channel --> Light On MIDI Controler for ForceUnMute on start !!!  ---> Corrected V0.9.0
    For all Channel --> Mute on two channels, mute 2 mask, unmute mask the last light on !!!!!!  ---> Corrected V0.9.0

Correction when Unmute MIDI controler on stereo link channel --> DONE
    StereoLink --> Mix, Solo, Mute, MaskMute, ForceUnmute, Pan 0 for Channel 0 Pan & for Channel +1 --> DONE

Pan function: Center Pan when push PAN button  --> DONE
Pan function: Pan Channel on selected SET button  --> DONE

Pan Midi: Encoder or Potentionmeter option.  --> DONE

Optimize update with global function same Track Next & Prev  --> DONE

For Controler with LCD and only one REC button, do inverse channel number when REC on on channel  --> DONE

Add REC function for only one button with Midi  --> DONE

Bug sur le canal 1 au demarrage sur la position fader. --> DONE by Update Controler after init
Bug sur refresh du LCD pour nom et canal --> DONE by correction on send_hex raz
Bug sur Select all Unselect All REC MTK depuis la page web de l'UI --> DONE

Bug Lot of : Midi Out : 90 00 00 !!!!! --> DONE by correction For loop in update mode

#################################################################
    IN PROGESS
#################################################################

Channel AUX remove PAN control

Bug received trame not complet sometime !!!  --> Not review !!!!

Bug VCA ne fonctionne pas depuis la page web de l'UI
Bug Light on Mute Clear & VCA & Shift Right

Pan function: LCD Pan update

Light On Write button when UI sent to Midi

Detection de la liste des péripherie MIDI (Utilisation du code de amidi.)
