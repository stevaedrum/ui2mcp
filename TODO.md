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

Channel AUX & VCA remove PAN control --> DONE
Bug in Mix on Master in Master mode  --> DONE
Correct PAN in Master Mode --> DONE
Master Fader on Midi controler --> DONE
Pan function: LCD Pan update --> DONE
Add Value pan in the 3rd line of LCD when move channel --> DONE
Bug VCA ne fonctionne pas depuis la page web de l'UI --> DONE
Correct PAN in not Master Mode --> DONE
Bug with fader update in UI web console for stereo link mode --> DONE

#################################################################
    IN PROGESS
#################################################################

Bug received trame not complet sometime !!!  --> Not review !!!!
Bug Light on Mute Clear & VCA & Shift Right !!!  --> Not review !!!!

In stereo link mode move left fader don't update right fader --> DONE
In stereo update must update the second fader after aftertouch = 00 to improve function --> DONE

#################################################################
    IDEAS
#################################################################

Add Value db in the 3rd line of LCD when move channel, Improve computing of the value !!!

Improve fader control in stereo case & in general

Function View with pre-selectionned channel like as view mode in UI

Light On Write button when UI sent to Midi

Detection de la liste des péripherie MIDI (Utilisation du code de amidi.)
