/*
North Paw 2.0 code, Eric Boyd, Sensebridge.net
Released under CC-NC-SA.

You'll need to get Pololu's LSM303 compass library:
https://github.com/pololu/LSM303
*/

#include <Wire.h>
#include <math.h>
#include <LSM303.h>
#include <EEPROM.h> //Needed to access the eeprom read write functions

LSM303 compass;

// define the pins used to run the shift register
int enable_low = 2;
int serial_in  = 6;
int ser_clear_low = 3;
int RCK  = 4;
int SRCK = 7;

// define pin for input of voltage level from LED/resistor
int voltage_pin = 14;  //A0

// variables for turning ourselves off if battery is low (protect batt)
int PowerDown = 0; // if we detect voltage is too low, power down and set this
                  // to 1 to remember that we're down
int voltage_level;
float voltage;

// vars for triggering calibration
int cal_state = 0;  // keep track of state of gravity
long cal_timeout_millis = 0;  // keep track of time
int cal_state_change_count = 0; // keep track of how many times we've been flipped
#define CAL_STATE_UPRIGHT 0
#define CAL_STATE_UPSIDEDOWN 1


// vars for the motor & motor strength
int heading;
int curr_motor = 1;
int prev_motor = 1;
int max_motor_strength = 200;  // 255 = full power
int min_motor_strength = 90; //point under which motors don't run or are unfeelable
  // note that the above two get dymanically recomputed based on battery
  // voltage, so changing them here does nothing
int motor_strength = 120; // holds changing motor strength value
int user_adjust_motor_strength = 0; // add this value to motor_strenght
  // adjust this value to change strength to your preferences
long millis_at_change = 0;
#define MILLIS_TILL_TIMEOUT 120000



// ********************* setup function ************************ //
void setup() {
  Serial.begin(9600);
  Serial.println("North Paw Powering Up...");
  Wire.begin();
  delay(100);  // added delay to see if it helps init start more reliably.
  compass.init();
  delay(100);  // added delay to see if it helps init start more reliably.
  compass.enableDefault();

  // this line is required to fix a problem with recent compass modules
  // where the default setting (greatest sensitivity) causes the compass
  // to output "-4095" readings because of saturation, see
  // https://my.st.com/public/STe2ecommunities/mcu/Lists/STM32F%20MEMS%20%20iNEMO/Flat.aspx?RootFolder=%2fpublic%2fSTe2ecommunities%2fmcu%2fLists%2fSTM32F+MEMS++iNEMO%2fProblem+with+LSM303DLM+compass&FolderCTID=0x01200200770978C69A1141439FE559EB459D7580003E26E7DD54228C428E8F9FB5EE9C5185&currentviews=154#{73A508FB-333E-4938-A32C-B019ED78D257} 
  compass.setMagGain(LSM303::magGain_40);  // set to mid-sensitivity  

  if (EEPROMReadInt(0) == 101) 
  {  // then we've got user set calibration coefficients, so read them
     // out and use them
     compass.m_min.x = EEPROMReadInt(2);
     compass.m_min.y = EEPROMReadInt(4);
     compass.m_min.z = EEPROMReadInt(6);
     compass.m_max.x = EEPROMReadInt(8);
     compass.m_max.y = EEPROMReadInt(10);
     compass.m_max.z = EEPROMReadInt(12);
  }
  else
  { // just use defaults that I discovered with my compass... hopefully
    // it'll be decent for people who fail to calibrate...
    
    // old values, when we used maximum (default) sensitivity
    //M min X: -557 Y: -758 Z: -251 M max X: 516 Y: 386 Z: 772
    //compass.m_min.x = -557; compass.m_min.y = -758; compass.m_min.z = -251;
    //compass.m_max.x = +516; compass.m_max.y = +386; compass.m_max.z = 772;

    // new values, using 4.0 gauss sensitivity
    //M min X: -177 Y: -233 Z: -162 M max X: 234 Y: 143 Z: 182
    compass.m_min.x = -177; compass.m_min.y = -233; compass.m_min.z = -162;
    compass.m_max.x = +234; compass.m_max.y = +143; compass.m_max.z = 182;
  }

  pinMode(enable_low, OUTPUT);  // set shift register pins as outputs
  pinMode(serial_in, OUTPUT);
  pinMode(ser_clear_low, OUTPUT);
  pinMode(RCK, OUTPUT);
  pinMode(SRCK, OUTPUT);
  pinMode(voltage_pin, INPUT);
  
    // make sure we start out all off
  digitalWrite(enable_low, HIGH);
  // this should wipe out the serial buffer on the shift register
  digitalWrite(ser_clear_low, LOW);
  delay(100);   //delay in ms
  
  // the TPIC6 clocks work on a rising edge, so make sure they're low to start.
  digitalWrite(RCK, LOW);
  digitalWrite(SRCK, LOW);
  
  digitalWrite(ser_clear_low, HIGH);   //we are now clear to write into the serial buffer

  //set intial motor strength
  //analogWrite(enable_low, 255-max_motor_strength);
  analogWrite(enable_low, 255); // start with motors off.
  millis_at_change = millis();
  
  Serial.println("North Paw setup complete");
}





// ********************* main loop ********************** //
void loop() {
  heading = SmartAverageCompassRead(10);  // average 10 readings...
  Serial.print("heading: ");
  Serial.print(heading);

  // see if they are triggering calibration
  detectCalInit();

  // switch motor order if compass is upside down
  if (cal_state == CAL_STATE_UPRIGHT)
    curr_motor = CalcMotor(8, heading);
  else
    curr_motor = curr_motor = 9 - CalcMotor(8, heading);

  // switch to northmost motor
  TurnOnMotor(curr_motor); 
  
  // figure out motor power level based on battery voltage and user movement
  AdjustMotorPower();
}







//This function will write a 2 byte integer to the eeprom at the specified address and address + 1
void EEPROMWriteInt(int p_address, int p_value)
{
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);

  EEPROM.write(p_address, lowByte);
  EEPROM.write(p_address + 1, highByte);
}

//This function will read a 2 byte integer from the eeprom at the specified address and address + 1
int EEPROMReadInt(int p_address)
{
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);

  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}


void detectCalInit()
{ // function which monitors the compass for it being turned upside
  // down three times in a row, which is the users method to trigger a 
  // recalibration routine.
  
  /*
  Serial.print("Accel x: ");
  Serial.print(compass.a.x);
  Serial.print(" y: ");
  Serial.print(compass.a.y);
  Serial.print(" z: ");
  Serial.println(compass.a.z);
   // in typical orientation, gravity is in y data, >800, <-800
  if(compass.a.y > 800)
    Serial.println("Right Side Up!");
  else if (compass.a.y < -800)
    Serial.println("Up Side Down!");
  */
  if (cal_state_change_count == 0) cal_timeout_millis = millis();
  if ((millis() - cal_timeout_millis) > 10000) cal_state_change_count = 0;
  if (compass.a.y > 800)
  {  // current state is "right side up"
     if (cal_state == CAL_STATE_UPSIDEDOWN)
     {
       cal_state_change_count++;
       cal_state = CAL_STATE_UPRIGHT;
     } 
  }
  if (compass.a.y < -800)
  {  // current state is "upside down"
     if (cal_state == CAL_STATE_UPRIGHT)
     {
       cal_state_change_count++;
       cal_state = CAL_STATE_UPSIDEDOWN;
     } 
  }
  /*
  Serial.print("Cal_state: ");
  Serial.print(cal_state);
  Serial.print("Cal__timeout_millis: ");
  Serial.print(cal_timeout_millis);
  Serial.print("Cal_state_change_count: ");
  Serial.println(cal_state_change_count);
  */
  if (cal_state_change_count >6)
  {
    cal_state_change_count = 0;
    Calibrate_compass();
  }
}

void Calibrate_compass()
{  // function to recalibrate the compass, store the results in EEPROM
  // first, let the user know they have succceeded in entering the cal mode
  LSM303::vector running_min = {2047, 2047, 2047}, running_max = {-2048, -2048, -2048};

  Serial.println("Starting Calibration Routine!");
  analogWrite(enable_low, 0);  // strong!
  for (int i = 1; i<25; i++)
  {
    TurnOnMotor(i%8+1);      //turn on the new motor at current motor strength
    delay(100);
  }
  analogWrite(enable_low, 255);  // motors off
  
  // ok, now get started collecting data for calibration
  for (int i = 0; i<300; i++)
  {
    compass.read();
    running_min.x = min(running_min.x, compass.m.x);
    running_min.y = min(running_min.y, compass.m.y);
    running_min.z = min(running_min.z, compass.m.z);
    running_max.x = max(running_max.x, compass.m.x);
    running_max.y = max(running_max.y, compass.m.y);
    running_max.z = max(running_max.z, compass.m.z);
    delay(97);  // bringing the total time to near 100ms
  }
  
  Serial.print("M min ");
  Serial.print("X: ");
  Serial.print((int)running_min.x);
  Serial.print(" Y: ");
  Serial.print((int)running_min.y);
  Serial.print(" Z: ");
  Serial.print((int)running_min.z);

  Serial.print(" M max ");  
  Serial.print("X: ");
  Serial.print((int)running_max.x);
  Serial.print(" Y: ");
  Serial.print((int)running_max.y);
  Serial.print(" Z: ");
  Serial.println((int)running_max.z);
  
  // first, set the currently used values
  compass.m_min.x = running_min.x;
  compass.m_min.y = running_min.y; 
  compass.m_min.z = running_min.z;
  compass.m_max.x = running_max.x; 
  compass.m_max.y = running_max.y; 
  compass.m_max.z = running_max.z;

  // now store the values in the EEPROM
  EEPROMWriteInt(0, 101); // code which says we've actually put numbers in!
  // spot 1 reserved for future use.
  EEPROMWriteInt(2, running_min.x);
  EEPROMWriteInt(4, running_min.y);
  EEPROMWriteInt(6, running_min.z);
  EEPROMWriteInt(8, running_max.x);
  EEPROMWriteInt(10, running_max.y);
  EEPROMWriteInt(12, running_max.z);
  
  /*
  // read them back, to be sure we'll still cool...
  Serial.print("M min ");
  Serial.print("X: ");
  Serial.print(EEPROMReadInt(2));
  Serial.print(" Y: ");
  Serial.print(EEPROMReadInt(4));
  Serial.print(" Z: ");
  Serial.print(EEPROMReadInt(6));

  Serial.print(" M max ");  
  Serial.print("X: ");
  Serial.print(EEPROMReadInt(8));
  Serial.print(" Y: ");
  Serial.print(EEPROMReadInt(10));
  Serial.print(" Z: ");
  Serial.println(EEPROMReadInt(12));
  */
  
  Serial.println("Ending Calibration Routine!");
  analogWrite(enable_low, 0);  // strong!
  for (int i = 1; i<25; i++)
  {
    TurnOnMotor(i%8+1);      //turn on the new motor at current motor strength
    delay(100);
  }
  analogWrite(enable_low, 255-motor_strength);  // back to regular strength...  
}

void AdjustMotorPower()
{  // function to adjust motor strength based on users activity level
  // and the voltage of the battery
  voltage_level = analogRead(voltage_pin-14);  // minus 14 because analogRead actally uses A0-A5 numbers...
  voltage = (float)voltage_level*(-0.00827586)+7.93448; // this is only approximate because it's a linear fit to a non-linear curve
  //Serial.print("battery voltage: ");
  //Serial.print(voltage);
  //Serial.println("V");

  if ((voltage_level > (560+20)) && (PowerDown == 0))
  { // then the battery is very flat, let's stop running the motors
    // warn the user with 3 "flashes" at full power
    // 560 is the calculated point of lowest power, but there is actually
    // some "noise" (basically brownouts caused by motors starting) that 
    // causes 560 to trigger sometimes when it shouldn't, so use 560+20.
    TurnOnMotor(1);
    Serial.println("Power Down!");
    for (int i = 0 ; i<3; i++)
    { 
      analogWrite(enable_low, 0);
      delay(1000);
      analogWrite(enable_low, 255);
      delay(1000);
    }
    PowerDown = 1;  // set so that we don't do this again
    // note, we could probably just put the ATMEGA to sleep too...
  }

  if (PowerDown == 0) {  // don't do anything if batteries are low...
  if (curr_motor != prev_motor)
  {  // motor has changed, so the user is moving around, set power at good level
     // TODO: should maybe handle case when we're flip/floping at motor edge
     // they could be inactive, yet we wouldn't power down...
     millis_at_change = millis();
     prev_motor = curr_motor;
     //recalibrate max & min motor strength based on voltage reading
    // now use y=mx+b, these constants target "3V" output (based on PWM strength)
    max_motor_strength = (int)(0.42633*(float)voltage_level-6.9279); 
    // now use y=mx+b, these constants target "1.8V" output (based on PWM strength)
    min_motor_strength = (int)(0.25578*(float)voltage_level-4.1567);
    // to generate your own power curves, see MotorStrengthCalcs google doc
    // set motor strength to the mid point of the above two
    motor_strength = (max_motor_strength + min_motor_strength)/2;
    motor_strength = motor_strength + user_adjust_motor_strength;
    if (motor_strength > 255) motor_strength = 255;
    if (motor_strength < 0) motor_strength = 0;
    analogWrite(enable_low, 255-motor_strength);
  }
  else
  {
    if ( (millis_at_change + MILLIS_TILL_TIMEOUT) < millis())
    {  // then we've overrun the timeout, so turn the motors off
      if (motor_strength > 51)
        Serial.println("Fading the motor off due to user inactivity...");
      while (motor_strength > 50) //50 is "off" - too little power to make motors move
      {
        motor_strength--;
        analogWrite(enable_low, 255-motor_strength); 
        delay(50); // gives about 5 seconds for typical motor strength...
      }
      analogWrite(enable_low, 255); // turn off completely to save power.
 
      // check to see if we're at the end of a period, if so, lets
      // turn the motors back on to keep them informed...     
      float period_check =  (float)(millis() - millis_at_change)/(float)MILLIS_TILL_TIMEOUT;
      period_check = period_check - floor(period_check);
      if (period_check > 0.98)
      { 
        motor_strength = (max_motor_strength+min_motor_strength)/2;
        analogWrite(enable_low, 255-motor_strength);
      }
    }
  }
  }
  Serial.print("   Strength: ");
  Serial.println(motor_strength);
}

void TurnOnMotor(int which){
  // accept which from 1 to 8
  // send message to shift register as appropiate
  delayMicroseconds(100);  //slow and steady
  Serial.print("   Motor: ");
  Serial.print(which); // print angle
  switch(which){
    case 1:
      shiftOut(serial_in, SRCK, LSBFIRST, B01000000);
      break;
    case 2:
      shiftOut(serial_in, SRCK, LSBFIRST, B00010000);
      break;
    case 3:
      shiftOut(serial_in, SRCK, LSBFIRST, B00000100);
      break;
    case 4:
      shiftOut(serial_in, SRCK, LSBFIRST, B00100000);
      break;
    case 5:
      shiftOut(serial_in, SRCK, LSBFIRST, B10000000);
      break;
    case 6:
      shiftOut(serial_in, SRCK, LSBFIRST, B00000010);
      break;
    case 7:
      shiftOut(serial_in, SRCK, LSBFIRST, B00000001);
      break;
    case 8:
      shiftOut(serial_in, SRCK, LSBFIRST, B00001000);
      break;
    case 9:
      shiftOut(serial_in, SRCK, LSBFIRST, B00000000);
      break;
    case 10:
      shiftOut(serial_in, SRCK, LSBFIRST, B11111111);
      break;
    default:
      // turn them all off
      shiftOut(serial_in, SRCK, LSBFIRST, B00000000);
  } 
  //in all cases, pulse RCK to pop that into the outputs
  delayMicroseconds(100);
  digitalWrite(RCK, HIGH);
  delayMicroseconds(100);
  digitalWrite(RCK, LOW);
}




int CalcAngle(int howMany, int which)
{  // function which calculates the "switch to next motor" angle
  // given how many motors there are in a circle and which position you want
  // assume which is 1-indexed (i.e. first position is 1, not zero)
  // assume circle is 0-360, we can always offset later...
  
  return (360/howMany*(which-0.5));
}

int CalcMotor(int howMany, int angle)
{  // function to calculate which motor to turn on, given
  // how many motors there are and what the current angle is
  // assumes motor 1 = angle 0
  // assumes angle is from 0-360
  int i;
  for (i = 1; i<howMany;i++)
  {
    if ( (angle >= CalcAngle(howMany, i)) & (angle <= CalcAngle(howMany, i+1)) )
       return i+1; 
  } 
  // if we're still here, it's the last case, the loop over case, which
  // is actually motor 1 by assumption
  return 1;
}


// function below has bad failure case when heading varies around 0/359
int AverageCompassRead(int n) 
{
  int headingSum = 0;
  for (int i = 0; i<n; i++)
  {
    compass.read();
    heading = compass.heading((LSM303::vector){1,0,0});
    Serial.println(heading);
    headingSum += heading;
    delay(8);
  }
  Serial.print("headingSum: ");
  Serial.println(headingSum);
  return (headingSum / n);
}

int SmartAverageCompassRead(int n)
{  // does fancy mod averaging routine in order to get reasonable
   // answers when there is noise around 0/359 degrees...
   // code adapted from http://stackoverflow.com/questions/5347653/filtering-compass-readings
   //  (see solution number 2)
  int average = 0;
  compass.read();
  int oldd = compass.heading((LSM303::vector){1,0,0});
  int newd = oldd;

  for (int i = 0; i<n; i++)
  {
    delay(10); // takes time to get a new reading from compass...
    compass.read();
    newd = compass.heading((LSM303::vector){1,0,0});
    //Serial.println(newd);
    
    if((newd +180) < oldd)
        {
            newd +=360; oldd = newd;
            average = average + newd;
            continue;
        }
        if((newd - 180) > oldd) 
        {
            newd -=360;oldd = newd;
            average = average + newd;
            continue;
        }
        average = average + newd;
        oldd = newd;
  }
  int avg_heading = ((average / n) % 360);
  if (avg_heading < 0) avg_heading += 360;
  return avg_heading;
}

