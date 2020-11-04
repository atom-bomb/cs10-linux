# cs10-linux

here is an alsa midi sequencer application to filter the midi output of a JLCooper CS-10 Control Station into control commands that are usable by Ardour or maybe some other linux DAW, for all i know.

## build

just type `make`

cs10-linux needs the alsa development packages, so make sure you have those in your system library and header search paths.

## install

just type `make install`

for now, the Makefile assumes that you want your executables to go in /usr/local/bin and that you have ardour5 installed, with it's associated midi control maps in /usr/share/ardour5/midi\_maps.

## run

### run cs10-linux

run cs10-linux like this:

```
cs10-linux -p xx:yy
```

where 'xx:yy' is the midi port of your hardware interface.
if you don't know the midi port of your hardware interface, take a look at the output of:

```
aseqdump -l
```

### connect ardour to cs10-linux

launch ardour in the usual way

navigate to `Window->MIDI Connections`
select `Hardware` from the top tab and `Ardour Misc` from the right tab
select the boxes connecting `mmc-io (in)` to `Generic MIDI Control In` and `MMC in`
select `Ardour Misc` from the top tab and `Hardware` from the right tab
select the boxes connecting `Generic MIDI Control Out`, `MMC out`, `MTC out` and `MIDI clock out` to `mmc-io (Out)`

navigate to `Window->Preferences->Show`
select `Control Surfaces`
select `Generic MIDI` and check `Enable`
select `Show Protocol Settings`
set `Incoming MIDI on` to `mmc-io (In)`
set `Outgoing MIDI on` to `mmc-io (Out)`
set `MIDI Bindings` to `cs10-linux`
set `Current Bank` to 1
check `Enable Feedback`
uncheck `Motorised`
set `Smoothing` to 15

### use the controller

transport controls (ff, rw, stop, play, rec) do what you expect.
the jog wheel moves the record and play head.

save a play head position by holding down shift, record and then pressing an 'F' button.
restore a play head position by holding down shift and pressing an 'F' button.
jump to the last play position by holding down shift and pressing play.
jump to the last record position by holding down shift and pressinf record.

saved positions are saved to disk and loaded when you next run cs10-linux.

save mixer state by holding down record and pressing an 'F' button.
restore mixer state by pressing an 'F' button.

saved mixer states are saved to disk and loaded when you next run cs10-linux.
NB, it takes a few seconds to re-send the entire mixer state to ardour. be patient.

press that weird 4-way button up or down to toggle between showing the SMPTE time of the current play position or the virtual bank of mixers.

when showing the smpte time, use the left and right buttons to display hours, minutes, seconds or frames.

when showing the virtual mixer bank, use the left and right buttons to select a different mixer bank.

set the mode switch to 'SEL' to pick which track to control using the parameter knobs.
set the mode switch to 'LOC' to enable recording on tracks.
set the mode switch to 'MUTE' to mute tracks.
set the mode switch to 'SOLO' to solo tracks.

when SEL, LOC, MUTE and SOLO are all lit, the controller is in 'Nullify' mode.
in this mode, you can use move the faders and knobs around until the 'NULL' lights go out, indicating that you have moved the fader to the position corresponding with the current state of the mixer, as set by either feedback from ardour or by restoring a mixer state using the 'F' keys.
