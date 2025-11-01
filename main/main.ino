#include <Wire.h>

#define SLAVE_ADDRESS 0x42 // I2C Slave Address

const int pot_pin = 2; // Analog input from potentiometer
const int motor_a_pin = 3;
const int motor_b_pin = 4;
const int motor_pwm_pin = 0;
const int led_pin = 5;

int setpoint = 512;
int deadzone = 10;

#define REG_STATUS 0x00 // This is our status register at address 0.
#define REG_POT 0x01 // This is our potentiometer register at address 1.
volatile byte statusRegister = 0;
volatile byte registerAddress = 0xFF;
volatile int pot_pos = 0;

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
    if (registerAddress == REG_STATUS) {
      statusRegister = Wire.read();
    }
  }
}

void i2c_request() {  // I2C Request Interrupt-driven function
  // Check which register the master is requesting data from (based on the previous 'receiveEvent')
  if (registerAddress == REG_STATUS) {
    Wire.write(statusRegister); // Send the current statusRegister value back to the master
  } else if (registerAddress == REG_POT) {
    Wire.write(pot_pos >> 2);
  } else {
    Wire.write(0xFF); // If an unknown register is requested, send an error or zero
  }
  registerAddress = 0xFF; // Reset the register address after the transaction is complete
}



void setup() {
  // put your setup code here, to run once:
  pinMode(motor_a_pin, OUTPUT);
  pinMode(motor_b_pin, OUTPUT);
  pinMode(motor_pwm_pin, OUTPUT);
  //pinMode(pot_pin, INPUT);

  Wire.begin(SLAVE_ADDRESS);

  Wire.onReceive(i2c_receive);
  Wire.onRequest(i2c_request);
}

void loop() {
  // put your main code here, to run repeatedly:
  pot_pos = analogRead(pot_pin);
  int deviation = setpoint-pot_pos;

  if (abs(deviation) > deadzone) {
    if (deviation > 0) {
      digitalWrite(motor_a_pin, LOW);
      digitalWrite(motor_b_pin, HIGH);
      analogWrite(motor_pwm_pin, 100);
    } else {
      digitalWrite(motor_a_pin, HIGH);
      digitalWrite(motor_b_pin, LOW);
      analogWrite(motor_pwm_pin, 100);
    }
  } else {
    digitalWrite(motor_pwm_pin, HIGH);
    digitalWrite(motor_a_pin, LOW);
    digitalWrite(motor_b_pin, LOW);
  }
  //digitalWrite(4, HIGH);
  //delay(500);
  //digitalWrite(4, LOW);
  //delay(500);
}
