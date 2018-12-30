30 December 2018
   released version 1.3.608 of ui2mcp

     Change log:
        -Cleaned: Clean code
        -Updated: Log level function
        -Updated: MTK Player function
        -Updated: Pan function corrected for the direction
        -Added: Update LCD when REC change
        -Fixed: Problem in PAN code for encoder
        -Updated: MTK Player function
        -Added: Sound Check function
        -Fixed: Bug in REC function with MIDI OUT overflow
        -Added: LCD Pan bar function
        -Added: LCD text mode for message
        -Updated: Debug function in arg input
        -Fixed: Clear array for shows and snapshots list
        -Added: Master mode strip fader

16 December 2018
   released version 0.9.171 of ui2mcp

     Change log:
        -Updated: function Pan with encoder
        -Updated: Changed LCD text inverted when channel have status REC
        -Updated: Log message for stereoindex, pan and solomode
        -Updated: manage stereoindex change on MIDI controler
        -Updated: optimize manage stereoindex on change
        -Updated: change mix management with stereoindex when mix change on UI
        -Updated: change mix management with stereoindex when changes on MIDI Controler

16 December 2018
   released version 0.9.153 of ui2mcp

     Change log:
        -Updated: function to manage type pan encoder or potentiometer
        -Added: new function for manage pan information to ui
        -Added: new function pan center via parameter button for pan
        -Cleaned: clean code and and comment
        -Removed: button led flashing when receive ui message
        -Added: new function for manage REC on Midi controler with only one button

15 December 2018
   released version 0.9.104 of ui2mcp

     Change log:
        -Added: integrate function Pan update after Select button is selected
        -Fixed: optimize Midibuffer buffer for midi raw and remove midi clear function
        -Fixed: Improve pan function for potentiometer

14 December 2018
   released version 0.9.81 of ui2mcp

     Change log:
        -Added: integrated update function for Select button

12 December 2018
   released version 0.9.79 of ui2mcp

     Change log:
        -Updated: Function to update Midi Controler with array struture UI variable.
        -Added: Add New parameter of channel StereoIndex
        -Added: Add function Clear Mute
        -Added: Add function Clear Solo
        -Added: Add management REC button with multiple on channel and only one for all channel.

10 December 2018
   released version 0.9.78 of ui2mcp

     Change log:
        -Added: New function TAP tempo for FX

09 December 2018
   released version 0.9.77 of ui2mcp

     Change log:
        -Updated: Correct Mute, MaskMute, ForceUnMute
        -Added: Solo Mode option
        -Added: Mute All and Mute Fx support

25 November 2018
   released version 0.9.70 of ui2mcp

     Change log:
        -Added: Decode VU message and send LCD vu meter information on midi controler FaderControl 2

03 November 2018
   released version 0.9.54 of ui2mcp

     Change log:
        -Updated: Send message to Ui to stop correctly the websocket connexion.
        -Added: Synchronization this UI and selected touch channel.
        -Added: Send Midi message for channel number and name on LCD controler.
        -Added: Fonction to stop the software with MIDI button sequence.

03 November 2018
   released version 0.9.53 of ui2mcp

     Change log:
        -Updated: Update print message to Log Trace for several function.
        -Added: Load name of channel in char.

03 November 2018
   released version 0.9.52 of ui2mcp

     Change log:
        -Fixed: Bug when the message is truncate in the buffer [STX].
        -Fixed: Put in memory the rest of the message and concat with the next message.

02 November 2018
   released version 0.9.51 of ui2mcp

     Change log:
        -Added: MidiValue On/Off for light variable.
        -Updated: Cleanup the code
        -Fixed: Bug on Mute function when button on MIDI Controler is used and Track function is used. !!! not find

02 November 2018
   released version 0.9.50 of ui2mcp

     Change log:
        -Added: LogTrace function for debug in log file
        -Added: Function with Mute Mask on UIx
        -Added:	UIx <--> MIDI Controler : Mute buttons for fader with Mask Mute option (Mute/Forceunmute/Mask)
        -Updated: Print messages with error message & Log
        -Fixed: Bug with multiple interaction of Mute

28 October 2018
   released version 0.9.0 of ui2mcp

     Change log:
        -Added: Initial version if the program.
        -Added: Hardware supported is Korg nanoKontrol2 with MCP mode (static configuration).
        -Added:	UIx <--> MIDI Controler : Mix fader
        -Added:	UIx <--> MIDI Controler : Solo buttons for fader
        -Added:	UIx <--> MIDI Controler : Rec buttons for fader linkedto Multitrkconfig
        -Added:	MIDI Controler --> UIx  : Dim on Master
        -Added:	MIDI Controler          : Track function for 24 channels
