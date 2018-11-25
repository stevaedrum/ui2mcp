# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Changed
- Update and improvement of Polish translation from [@m-aciek](https://github.com/m-aciek).

-------------------------------------------------------------------------------------------------------------------------------------------------
## [0.9.0] - 2018-10-28
### Added
- Initial version if the program.
- Hardware supported is Korg nanoKontrol2 with MCP mode (static configuration).
- Function supported:
	UIx <--> MIDI Controler : Mix fader
	UIx <--> MIDI Controler : Solo buttons for fader
	UIx <--> MIDI Controler : Rec buttons for fader linkedto Multitrkconfig
	MIDI Controler --> UIx  : Dim on Master
	MIDI Controler          : Track function for 24 channels

-------------------------------------------------------------------------------------------------------------------------------------------------
## [0.9.50] - 2018-11-02

### Added
- LogTrace function for debug in log file
- Function with Mute Mask on UIx
	UIx <--> MIDI Controler : Mute buttons for fader with Mask Mute option (Mute/Forceunmute/Mask)

### Changed
- Print messages with error message & Log

### Fixed
- Bug with multiple interaction of Mute

-------------------------------------------------------------------------------------------------------------------------------------------------
## [0.9.51] - 2018-11-02

### Added
- MidiValue On/Off for light variable.

### Changed
- Cleanup the code

### Fixed
- Bug on Mute function when button on MIDI Controler is used and Track function is used. !!! not find

-------------------------------------------------------------------------------------------------------------------------------------------------
## [0.9.52] - 2018-11-03

### Fixed
- Bug when the message is truncate in the buffer [STX].
  Put in memory the rest of the message and concat with the next message.

  -------------------------------------------------------------------------------------------------------------------------------------------------
## [0.9.53] - 2018-11-03

### Changed
- Update print message to Log Trace for several function.

### Added
- Load name of channel in char.

  -------------------------------------------------------------------------------------------------------------------------------------------------
## [0.9.54] - 2018-11-03

### Changed
- Send message to Ui to stop correctly the websocket connexion.

### Added
- Synchronization this UI and selected touch channel.
- Send Midi message for channel number and name on LCD controler.
- Fonction to stop the software with MIDI button sequence.

  -------------------------------------------------------------------------------------------------------------------------------------------------
## [0.9.70] - 2018-11-25

### Changed
-

### Added
- Decode VU message and send LCD vu meter information on midi controler FaderControl 2
