# name=OpenFader
# url=
# supportedDevices=OpenFader
# version 2025.1

import patterns
import mixer
import device
import transport
import ui
import midi

# --- Configuration Constants ---
# Pitch Bend messages for fader 1 (Channel 1) use MIDI Channel 0 in the MIDI Status byte (E0-EF).
# The mixer script handles all tracks starting from master (track 0).
FADER_TRACK_INDEX = 1 # We'll control Mixer Track 1 (Mixer.getTrackNum(0) is Master, so 1 is Track 1)
FADER_MIDI_CHANNEL = 0 # Fader 1 uses MIDI Channel 1, which is offset 0 in MIDI status bytes (E0)
SLIDER_EVENT_ID = mixer.getTrackPluginId(FADER_TRACK_INDEX, 0) + midi.REC_Mixer_Vol
SLIDER_NAME = mixer.getTrackName(FADER_TRACK_INDEX) + ' - Vol'

# Pitch Bend Range: 0 to 16383 (14-bit)
# Signed Range: -8192 to 8191 (Used by MIDI.py utility)

# This class manages the communication for your single motorized fader
class TOpenFader:
    
    def __init__(self):
        # Initialize last known position to force an initial update
        self.LastSentValue = -1
        self.SmoothSpeed = 469 # Standard MCU smoothing speed
    
    def OnInit(self):
        """Called once when the device script is initialized."""
        # This tells FL Studio we want to receive meter data (useful for updates)
        device.setHasMeters()
        ui.set.showHint('OpenFader (Track 1) Initialized')
        print('OpenFader Init: Ready for Fader 1 communication.')
        
    def OnDeInit(self):
        """Called when the script is unloaded."""
        ui.set.showHint('OpenFader De-Initialized')
        print('OpenFader DeInit: Shutting down.')

    def OnMidiIn(self, event):
        """Called when any MIDI message is received from your device."""
        
        ui.set.showHint('MIDI INPUT DETECTED')

        # --- 1. Handle Fader Input (Physical Fader -> FL Studio) ---
        if event.midiId == midi.MIDI_PITCHBEND and event.midiChan == FADER_MIDI_CHANNEL:
            # Check if it's Fader 1 (Channel 0)
            
            # The 'event.inEv' is the raw 14-bit Pitch Bend value (0 to 16383).
            # The 'event.outEv' is the signed value (-8192 to 8191).
            # We use the raw value for the volume event:
            
            # Automate the mixer volume event ID with the 14-bit value
            mixer.automateEvent(SLIDER_EVENT_ID, event.inEv, midi.REC_MIDIController, self.SmoothSpeed)
            
            # Display hint
            ui.set.showHint(SLIDER_NAME + ': ' + ui.getHintValue(event.inEv, midi.FromMIDI_Max))
            
            # Mark the event as handled to prevent further processing
            event.handled = True
        
        # --- 2. Ignore All Other Messages (Buttons, Knobs, CCs, Notes, SYSEX) ---
        # This is the most crucial step to prevent lag and transport conflicts.
        # By not setting event.handled = True for other messages, FL Studio's default
        # MCU implementation won't link those signals to internal commands.
        # We rely only on the dedicated Pitch Bend handler above.
        
        # Pass the event through if unhandled (will be ignored by default)
        
    def OnUpdateFaders(self, flags):
        """Called frequently (like your Arduino loop) to update motors/lights."""
        
        # --- 3. Handle Fader Output (FL Studio -> Motor/LEDs) ---
        # Get the current mixer volume value from FL Studio (0 to 16383)
        currentValue = mixer.getEventValue(SLIDER_EVENT_ID, midi.MaxInt, False)
        
        # Only send data if the value has changed significantly since the last send
        if abs(currentValue - self.LastSentValue) > 100: # Use a large threshold (100 is approx 0.6%)
            
            # Send the Pitch Bend message back to the controller
            # The 'midi.MidiOutMsg' function expects the full 3-byte MIDI message
            # Pitch Bend (E0) on Channel 1 (offset 0)
            # Data 1 (LSB: bits 0-6), Data 2 (MSB: bits 7-13)
            
            # LSB is bits 0-6 (value & 0x7F)
            lsb = currentValue & 0x7F
            # MSB is bits 7-13 (value >> 7)
            msb = currentValue >> 7 
            
            # Full MIDI message: E0 (status) + LSB (data1) + MSB (data2)
            # The format is a 32-bit integer: (status) + (data1 << 8) + (data2 << 16)
            midi_message = midi.MIDI_PITCHBEND + (lsb << 8) + (msb << 16)
            
            device.midiOutMsg(midi_message)
            self.LastSentValue = currentValue

        # This prevents the script from updating the transport LEDs, VU meters, etc.
        # which can also cause lag on simple controllers.
        
# --- Script Entry Point ---
# Create an instance of the class
OpenFader = TOpenFader()

def OnInit():
    OpenFader.OnInit()

def OnDeInit():
    OpenFader.OnDeInit()

def OnMidiIn(event):
    OpenFader.OnMidiIn(event)

def OnUpdateFaders(flags):
    OpenFader.OnUpdateFaders(flags)