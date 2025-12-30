#include <Wire.h>

#define SLAVE_ADDRESS 0x40 // I2C Slave Address

const int pot_pin = 2; // Analog input from potentiometer
const int motor_a_pin = 9;
const int motor_b_pin = 8;
const int motor_pwm_pin = 0;
const int led_pin = 5;

bool update = false;

int low_calib = 0;
int high_calib = 1024;

volatile int setpoint = 511;
int deadzone = 14;
int hysteresis = 6;

int low_speed = 70;

//#define REG_STATUS 0x00 // This is our status register at address 0.
#define REG_POT 0x00 // This is our potentiometer register at address 1.
volatile byte registerAddress = 0xFF;
volatile int pot_pos = 0;
volatile int i2c_pos = 0;

int deviation;
bool direction = false; // False = going down
bool move = false;

/*
These are two functions define the I2C functionality. The i2c_receive function is always called whenever
there is a I2C transaction from the master. The i2c_request function is called whenever the master wants
to read a register, however the i2c_receive function is also called. The i2c_receive function gets the
requested register address, and this is used in the i2c_request function.
*/

void i2c_receive(int numBytes) { // I2C Receive Interrupt-driven function
  if (numBytes > 0) {
    registerAddress = Wire.read();
  }
  if (numBytes > 1) { // If there is more than 1 byte, there is data to put into the register
    if (registerAddress == REG_POT) {
      setpoint = Wire.read()<<2;
      update = true;
    }
  }
}

void i2c_request() {  // I2C Request Interrupt-driven function
  // Check which register the master is requesting data from (based on the previous 'receiveEvent')
  if (registerAddress == REG_POT) {
    if (i2c_pos < 0) {
      i2c_pos = 0;
    }
    if (i2c_pos > 1023) {
      i2c_pos = 1023;
    }
    Wire.write(i2c_pos >> 2);
    //Wire.write(deviation);
  } else {
    Wire.write(0xFF); // If an unknown register is requested, send an error or zero
  }
  registerAddress = 0xFF; // Reset the register address after the transaction is complete
}

void calibrate() {
  // Move all the way down
  digitalWrite(motor_a_pin, LOW);
  digitalWrite(motor_b_pin, HIGH);
  analogWrite(motor_pwm_pin, 255);
  delay(500); // Delay to let it get there
  // Turn motor off
  digitalWrite(motor_a_pin, LOW);
  digitalWrite(motor_b_pin, LOW);
  digitalWrite(motor_pwm_pin, HIGH);
  delay(500); // Delay to let it settle
  low_calib = analogRead(pot_pin);
  // Move all the way down
  digitalWrite(motor_a_pin, HIGH);
  digitalWrite(motor_b_pin, LOW);
  analogWrite(motor_pwm_pin, 255);
  delay(500); // Delay to let it get there
  // Turn motor off
  digitalWrite(motor_a_pin, LOW);
  digitalWrite(motor_b_pin, LOW);
  digitalWrite(motor_pwm_pin, HIGH);
  delay(500); // Delay to let it settle
  high_calib = analogRead(pot_pin);
}

void initHighFrequencyPwm() {
    // 1. Set the Prescaler to /1 (CLKSEL = 0b001) for ~78kHz frequency.
    TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_CLKSEL_gm; // Clear CLKSEL bits (bits 1:3)
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_CLKSEL_DIV1_gc; // Set prescaler to /1

    // 2. Configure the TCA0 Control B Register (TCA0.SINGLE.CTRLB)
    // Pin D0 (PA4) is TCA0 WO4. Enable Compare Channel 4 (CMP4EN, which is bit 4).
    TCA0.SINGLE.CTRLB |= (1 << 4); // Use numeric bit position for CMP4EN
    
    // 3. Set the Mode (Single-Slope PWM)
    TCA0.SINGLE.CTRLB &= ~TCA_SINGLE_WGMODE_gm; // Clear WGMODE bits
    TCA0.SINGLE.CTRLB |= TCA_SINGLE_WGMODE_SINGLESLOPE_gc; // Set to Single-Slope PWM mode

    // 4. Enable TCA0
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm; // Enable the TCA0 timer
}

void setup() {
  initHighFrequencyPwm();


  // put your setup code here, to run once:
  pinMode(motor_a_pin, OUTPUT);
  pinMode(motor_b_pin, OUTPUT);
  pinMode(motor_pwm_pin, OUTPUT);
  //pinMode(pot_pin, INPUT);

  Wire.begin(SLAVE_ADDRESS);

  Wire.onReceive(i2c_receive);
  Wire.onRequest(i2c_request);

  Serial.begin(115200);

  calibrate();
  setpoint = 512;
  update = true;
}

int print_timer = 0;

void loop() {
  // put your main code here, to run repeatedly:
  pot_pos = ((long)analogRead(pot_pin)-(long)low_calib)*1023/((long)high_calib-(long)low_calib);
  if (pot_pos < 0) {
    pot_pos = 0;
  }
  if (pot_pos > 1023) {
    pot_pos = 1023;
  }
  //Serial.println(pot_pos);
  if (move) {
    if (direction) {
      deviation = pot_pos-setpoint-hysteresis/2;
    } else {
      deviation = pot_pos-setpoint+hysteresis/2;
    }
  } else {
    deviation = setpoint-pot_pos;
  }
  
  /*
  if (print_timer+100 < millis()) {
    Serial.println(deviation);
    print_timer = millis();
  }
  */
  

  if (update) {
    if (abs(deviation) > deadzone/2) {
      move = true;

      long strength = (abs((long)deviation)*(255-(long)low_speed)/500)+(long)low_speed;
      //Serial.println(strength);

      if (deviation > 0) { // If we're too far up, move down
        digitalWrite(motor_a_pin, LOW);
        digitalWrite(motor_b_pin, HIGH);
        analogWrite(motor_pwm_pin, strength);
        direction = false;
      } else {  // If we're too far down, move up
        digitalWrite(motor_a_pin, HIGH);
        digitalWrite(motor_b_pin, LOW);
        analogWrite(motor_pwm_pin, strength);
        direction = true;
      }
    } else {
      digitalWrite(motor_pwm_pin, HIGH);
      digitalWrite(motor_a_pin, LOW);
      digitalWrite(motor_b_pin, LOW);
      move = false;
      update = false;
    }
    //digitalWrite(4, HIGH);
    //delay(500);
    //digitalWrite(4, LOW);
    //delay(500);
  }
  i2c_pos = pot_pos;
}
