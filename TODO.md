[ui2mcp] oject

Take in account Mute ALL with "SETD^mgmask^8388608".  --> DONE

Detection de la liste des péripherie MIDI
Utilisation du code de amidi.

Correction of Mute / ForceUnmute with mutiple configuration:  --> DONE
    On channel --> ForceUnMute = MuteMask disable = Light on MIDI Controler!!!  ---> Corrected V0.9.0
    On channel --> Light On MIDI Controler for ForceUnMute on start !!!  ---> Corrected V0.9.0
    For all Channel --> Mute on two channels, mute 2 mask, unmute mask the last light on !!!!!!  ---> Corrected V0.9.0

Correction when Unmute MIDI controler on stereo link channel
    StereoLink --> Mix, Solo, Mute, MaskMute, ForceUnmute, Pan 0 for Channel 0 Pan & for Channel +1

Pan function:
    Center Pan when push PAN button
    Pan Channel on selected SET button
    LCD Pan update

Pan Midi:
    Encoder or Potentionmeter option.

Optimize update with global function same Track Next & Prev

Light On Write button when UI sent to Midi

For Controler with LCD and only one REC button, do inverse channel number when REC on on channel
