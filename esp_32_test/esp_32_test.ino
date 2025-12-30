#include <Wire.h>
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

#define FADER_MIDI_CHANNEL_START 1

// --- Fader Mapping Constants ---
// Your custom analog range
const int ANALOG_MAX = 255; 
// MIDI Control Change range
const int MIDI_PB_SIGNED_MAX = 8191; 
const int MIDI_PB_SIGNED_MIN = -8192;

// A variable to hold the last sent MIDI position to prevent sending too often
long last_fader_pos[] = {-1, -1, -1, -1, -1};

// --- Initialize TinyUSB MIDI ---
Adafruit_USBD_MIDI usb_midi;

int sending_movement_timer = 0;
int physical_movement_timer = 0;

// 2. Create an instance of the MIDI Library using the USB transport
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

#define OPEN_FADER_ADDRESS_START 0x40

void send_data(int index, uint8_t data_byte_1, uint8_t data_byte_2) {
  Wire.beginTransmission(OPEN_FADER_ADDRESS_START + index);
  Wire.write(data_byte_1);
  Wire.write(data_byte_2);
  Wire.endTransmission();
}

uint8_t read_byte(int index, uint8_t register_to_read) {
  byte data = 0xFF;
  Wire.beginTransmission(OPEN_FADER_ADDRESS_START + index);
  Wire.write(register_to_read);
  Wire.endTransmission(false);
  Wire.requestFrom(OPEN_FADER_ADDRESS_START + index, 1);
  data = Wire.read();
  return data;
}



// --- NEW: Pitch Bend Callback (Receiving data from FL Studio) ---
void usb_midi_pitch_bend_callback(byte channel, int value) {
    // Print the received Pitch Bend value and channel for debugging
    /*
    Serial0.print("RX PB: Ch ");
    Serial0.print(channel);
    Serial0.print(", Value ");
    Serial0.println(value);
    */

    Serial0.print("Detected movement on channel ");
    Serial0.println(channel);

    // Pitch Bend data ranges from 0 to 16383 (center is 8192)
    for (int i=0; i<5; i++) {
      if (channel == FADER_MIDI_CHANNEL_START + i) {

        
        
        // 1. Convert MIDI Pitch Bend value (0-16383) to your Analog Range (0-255)
        // Note: The 'value' from the MIDI library is already centered around 0-8192-16383, 
        // but it's often more straightforward to map directly.
        // The value passed here is the 14-bit value: 0 to 16383.
        
        // Use a long for mapping to prevent overflow with 16383
        long targetAnalog = map(value, MIDI_PB_SIGNED_MIN, MIDI_PB_SIGNED_MAX, 0, ANALOG_MAX);

        // Ensure the value is clamped to the safe motor control range
        if (targetAnalog < 0) targetAnalog = 0;
        if (targetAnalog > ANALOG_MAX) targetAnalog = ANALOG_MAX;
        
        // Ensure the value fits in the required byte size
        send_data(i, 0x00, (uint8_t)targetAnalog);
        sending_movement_timer = millis();
      }
    }
    
}


void setup() {
  

  // Initialize the USB MIDI device
  MIDI.setHandlePitchBend(usb_midi_pitch_bend_callback);
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // put your setup code here, to run once:
  Serial0.begin(115200);
  Serial0.println("Hello World!");

  Serial0.println("ESP32-S3 MIDI Fader Initialized using MCU Pitch Bend.");
  Serial0.println("Connect to the Native USB port for MIDI.");

  delay(100);

  Serial0.println("Done with MIDI Setup");

  Wire.begin(15, 16);

  Serial0.println("Done with Wire.begin");
}

int fader_pos = 0;

void loop() {
  // Must be called continuously to process incoming MIDI messages
  MIDI.read();
  for (int i=0; i<5; i++) {
    fader_pos = read_byte(i, 0x00); // Read the physical fader's position (0-255)
    //Serial0.print(fader_pos);
    //Serial0.print("  ");
    if (fader_pos < 1) {
      fader_pos = 1;
    }
    if (fader_pos > 254) {
      fader_pos = 254;
    }
    

    if (abs(fader_pos - last_fader_pos[i]) > 4 || (abs(fader_pos - last_fader_pos[i]) > 0 && (fader_pos == 1 || fader_pos == 254))) { // Send on any change

      long midiPosition = map(fader_pos, 1, 254, -8192, 8191);


      if (millis() > sending_movement_timer + 200) {
        MIDI.sendPitchBend((int)midiPosition, FADER_MIDI_CHANNEL_START+i);
        last_fader_pos[i] = fader_pos;
      }
    }
  }
  //Serial0.println();
  //delay(10);
}
