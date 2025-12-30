# OpenFader
A project to modularize motorized faders with I2C control based on the ATtiny1614.

Currently very much in development.

https://github.com/user-attachments/assets/ef170d1e-9a4b-43a3-9d26-493bcd82ac08

![PXL_20251116_050939562 RAW-01 MP COVER](https://github.com/user-attachments/assets/bc069402-9d71-43b5-a8d1-c1ee2bd488f5)


## I2C Registers:




- `0x00` Potentiometer Position (8 bit, Read/Write)

  Register 0x00, the fader position, can always be read to get the fader's current position. Whenever the position is written, the fader moves to the position. Once it has reached the position, the motor turns off. If   the fader cannot reach the position, either it takes too long or it detects movement in the other direction, the motor turns off.
- `0x01` Mode/Settings (8 bit, Read/Write)
  <table>
    <tr>
      <td>7</td>
      <td>6</td>
      <td>5</td>
      <td>4</td>
      <td>3</td>
      <td>2</td>
      <td>1</td>
      <td>0</td>
    </tr>
    <tr>
      <td colspan="3">Setpoint Deadzone</td>
      <td colspan="3">Motor Speed / Strength</td>
      <td colspan="1">Continuous Mode</td>
      <td colspan="1">Proportional Mode</td>
    </tr>
  </table>
  When bit 0 is high, as the fader approaches the setpoint, it slows down.  
  When bit 1 is low, normal operation as described below. When bit 1 is high, the motor is always trying to get back to the setpoint.
- `0x02` Capacitive Touch Reading (8 bit, Read only, Not yet implemented)







