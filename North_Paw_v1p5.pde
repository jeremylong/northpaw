/* Skory & Eric, sensebridge.net/northpaw
orders@sensebridge.net
April 18th, 2010
License: http://creativecommons.org/licenses/by-nc-sa/3.0/
*/


/* Some code from:
 * 2009-03-24, pager motor test, lamont lucas
/*
Some Hitachi HM55B Compass reading code copied from: kiilo kiilo@kiilo.org
License:  http://creativecommons.org/licenses/by-nc-sa/2.5/ch/
 */

#include <math.h>

// define the pins used to run the shift register
int enable_low = 3;
int serial_in  = 6;
int ser_clear_low = 2;
int RCK  = 4;
int SRCK = 7;

// define pin for input of voltage level from LED/resistor
int voltage_pin = 14;  //A0

// define pins used to operate the digital compass (HM55B)
byte CLK_pin = 17;   //A3
byte EN_pin = 16;     //A2
byte DIO_pin = 15;    //A1
int X_Data = 0;
int Y_Data = 0;
int angle;
int status;

//timing vars
unsigned long counter = 0;
int prev_motor = 1;
int curr_motor = 1;
int cycles_per_second = 15; //board and compass specific - must measure
int count;
int activity = 100;
int max_activity = 200;

unsigned long serialTimer = millis();

// motor strength vars, note these are adjusted dynamically based on 
// battery voltage and activity level
int max_motor_strength = 200;  // 255 = full power
int min_motor_strength = 90; //point under which motors don't run or are unfeelable
int motor_strength = 190; // holds changing motor strength vals
int PowerDown = 0; // if we detect voltage is too low, power down and set this
                  // to 1 to remember that we're down

int voltage_level;
float voltage;

void setup() {
  pinMode(enable_low, OUTPUT);  // set shift register pins as outputs
  pinMode(serial_in, OUTPUT);
  pinMode(ser_clear_low, OUTPUT);
  pinMode(RCK, OUTPUT);
  pinMode(SRCK, OUTPUT);
  pinMode(voltage_pin, INPUT);
    
  // use some serial for debugging
  Serial.begin(57600);
  Serial.println("Setting up board");
  
  // make sure we start out all off
  digitalWrite(enable_low, HIGH);
  // this should wipe out the serial buffer on the shift register
  digitalWrite(ser_clear_low, LOW);
  delay(100);   //delay in ms
  
  // the TPIC6 clocks work on a rising edge, so make sure they're low to start.
  digitalWrite(RCK, LOW);
  digitalWrite(SRCK, LOW);
  
  digitalWrite(ser_clear_low, HIGH);   //we are now clear to write into the serial buffer

  Serial.println("Board is setup");

  // setup for HM55B compass chip
  pinMode(EN_pin, OUTPUT);
  pinMode(CLK_pin, OUTPUT);
  pinMode(DIO_pin, INPUT);

  HM55B_Reset();

  //set intial motor strength
  analogWrite(enable_low, 255-max_motor_strength);
}


void loop() {
  int i;

  if (PowerDown == 0)
  {  // only take a reading if we're got power
    // make the compass get a reading  
    HM55B_StartMeasurementCommand(); // necessary!!
    delay(40); // the data is ready 40ms later
    status = HM55B_ReadCommand();
  }
//  Serial.print(status); // read data and print Status
//  Serial.print(" ");
  X_Data = ShiftIn(11); // Field strength in X
  Y_Data = ShiftIn(11); // and Y direction
  X_Data = X_Data * -1;  // In current rig, chip
  Y_Data = Y_Data * -1;  // is upside-down; compensate
  Serial.print("X: ");
  Serial.print(X_Data); // print X strength
  Serial.print(" Y: ");
  Serial.print(Y_Data); // print Y strength
  Serial.print(" ANG: ");
  digitalWrite(EN_pin, HIGH); // ok deselect chip
  angle = 180 * (atan2(-1 * Y_Data , X_Data) / M_PI); // angle is atan( -y/x) !!!
  angle = angle - 180; // adjust for position of compass with respect to motor 1
  if (angle < 0) angle = (360 + angle); //offset neg angles
  Serial.print(angle); // print angle
  Serial.print(" ");

  //recalibrate max & min motor strength based on voltage reading
  voltage_level = analogRead(voltage_pin-14);  // minus 14 because analogRead actally uses A0-A5 numbers...
  voltage = (float)voltage_level*(-0.00827586)+7.93448;
  // this is only approximate because it's a linear fit to a non-linear curve
  Serial.print("approx. battery voltage: ");
  Serial.print(voltage);
  Serial.print("  vreading: ");
  Serial.println(voltage_level);
  // now use y=mx+b, these constants target "3V" output (based on PWM strength)
  max_motor_strength = (int)(0.42633*(float)voltage_level-6.9279); 
  // now use y=mx+b, these constants target "1.8V" output (based on PWM strength)
  min_motor_strength = (int)(0.25578*(float)voltage_level-4.1567);
  // to generate your own power curves, see MotorStrengthCalcs google doc
  //if (motor_strength < min_motor_strength) motor_strength = min_motor_strength;
  //if (motor_strength > max_motor_strength) motor_strength = max_motor_strength;

  //Turn on the appropriate motor while keeping track of time and varying motor strength
  // 9 minus CalcMotor because in V1.5 the compass is upside down compared
  // to it's location in V1.0
  /*# Timing Code Overview Comment
#    -define an integer activity level as a function of how often the wearer changes orientation.
#    -increase that level by one each time the wearer changes from one motor to the next
#    -define an interval of time based on that activity level
#    -each time that interval passes, descrease the activity level by 13
#        -this means that the level decreases **non-linearly**
#    -at high activity levels keep the motor always on
#    -at mid activity levels, pulse the motor on and off
#        -the amount of time on relative to off decreases with the activity level
#    -at the minimum activity level, leave the motor always off
    
1. Has the compass reading changed enough to activate a different motor?
    Yes: goto (2) No: goto (6)

    2. Activate the new motor.
    3. Reset the timing counter
    4. Increase how active we think the wearer is unless we're already at the maximum activity level.
    5. Update motor strength to be at a point between the min. motor strength and max. motor strength 
        that is proportional to the ratio of the current activity level to the maximum activity level.
    
    6. Define the interval for a motor to stay on when we're not moving around as:
        (activity level / 10) * the average number of cycles of this processor, per second (this 
        needs to be found and hard-coded)
        
    7. Is the timing counter less than the interval defined in (6)?
        Yes: goto (8) No: goto (11)
        
        8. Is the current motor strength less than the maximum for this activity level?
            Yes: goto (9) No: goto (14)
            
            9. Increase motor strength by 1 bit
            10. Wait 50 ms. and goto (8)    #(Crescendo)
        
        11. Is the current motor strength greater than 50?
            #(Don't remember why 50 instead of min_motor_strength here...)
            Yes: goto (12) No: goto (14)
            
            12. Decrease motor strength by one bit
            13. Wait 50 ms. and goto (11)   #(Decrescendo)
            
    14. Increment the timing counter
    15. Define the interval at which we decrease how active we think the wearer is as:
         (600 * cycles of processor per second) / activity level
         
    16. Is the timing counter greater than the interval defined in (15)?
        Yes: goto (17) No: goto (19)
        
        17. Reset the counter.
        18. Reduce the activity level by thirteen unless its already < 13.
        
19. Continue with main loop!
*/
  curr_motor = 9 - CalcMotor(8, angle);
  if (curr_motor != prev_motor) 
  { // motor has changed, make activity level higher
    TurnOnMotor(curr_motor);      //turn on the new motor
    counter = 0;                  //reset counter
    if (activity < max_activity)
    {
      activity = activity + 1;      //increase activity level up to 200
      motor_strength = (((float)activity / (float)max_activity) * (max_motor_strength - min_motor_strength) + min_motor_strength); //set m_strength proportianately to activity
    }									   // within range of min_ms-max_mas
  } 
  else 
  { // motor has NOT changed, so reduce activity level
    if (counter < (activity / 10) * cycles_per_second) 
    { //only keep same motor on for
      analogWrite(enable_low, 255-motor_strength);		 //less than cycles * activity level
      TurnOnMotor(curr_motor);
      while (motor_strength < ((float)activity / (float)max_activity) * (max_motor_strength - min_motor_strength) + min_motor_strength)
      {  //if m_strength is low (motors off)
        motor_strength++;						//crescendo the m_strength
        Serial.print(" MS: ");
        Serial.println(motor_strength);
        analogWrite(enable_low, 255-motor_strength);
        delay(50);
      }
    }
    else 
    { //if counter runs to upper limit
      while (motor_strength > 50)
      {	//if m_strength is high (motors on)
        motor_strength--;						//decrescendo the m_strength
        analogWrite(enable_low, 255-motor_strength); //50 seems like point motor
        Serial.print(" MS: ");	
        Serial.println(motor_strength);
        delay(50);
      }
      TurnOnMotor(0);            				//then turn all motors off
    }
    counter++;                   //increment counter
    if (counter > (600 * cycles_per_second) / activity )
    {
      counter = 0;               //reset counter
      if (activity > 13)
      {        //lower activity level
        activity = activity - 13;  //max val(s) 0-12
      }
    }
  }

  if ((voltage_level > (560+20)) && (PowerDown == 0))
  { // then the battery is very flat, let's stop running the motors
    // warn the user with 3 "flashes" at full power
    // 560 is the calculated point of lowest power, but there is actually
    // some "noise" (basically brownouts caused by motors starting) that 
    // causes 560 to trigger sometimes when it shouldn't, so use 560+20.
    TurnOnMotor(1);
    Serial.println("Power Down!");
    for (i = 0 ; i<3; i++)
    { 
      analogWrite(enable_low, 0);
      delay(1000);
      analogWrite(enable_low, 255);
      delay(1000);
    }
    PowerDown = 1;  // set so that we don't do this again  
  }
  
  if (PowerDown == 0)
  {
    if (motor_strength > 55)
    {
      analogWrite(enable_low, 255-motor_strength);
    }
    else
    { // no point sending lower values, just wastes power
      analogWrite(enable_low, 255);
    }
    Serial.print("Activity Level: ");
    Serial.print(activity);
    Serial.print("  Counter: ");
    Serial.print(counter);
    Serial.print("  min/Motor Strength/max: ");	
    Serial.print(min_motor_strength);
    Serial.print("/");
    Serial.print(motor_strength);
    Serial.print("/");
    Serial.println(max_motor_strength);
    prev_motor = curr_motor;
  }
  else
  {
    analogWrite(enable_low, 255);  // leave motor off
    delay(1000); // delay really unnecessary, but prevents too much blabber on serial
    if (voltage_level < 530)  PowerDown = 0;  // turn back on if voltage somehow rising back to good again
  }
  
  /*Serial.print("ACT: ");
  Serial.print(activity);

  Serial.print(" CNT: ");
  Serial.print(counter);
  
  Serial.print(" MS: ");
  Serial.println(motor_strength);
  */

/*
// Code for debugging motor order (turn them on in numerical sequence)  
  analogWrite(enable_low, 0);
  count++;
  TurnOnMotor(count);
  Serial.print(count); // print angle
  Serial.println("  ");
  delay(3000);
  if (count >= 8)
  {
    count = 0;
    delay(1500);
  }
*/
}



//// FUNCTIONS

void TurnOnMotor(int which){
  // accept which from 1 to 8
  // send message to shift register as appropiate
  delayMicroseconds(100);  //slow and steady
  Serial.print("Motor  ");
  Serial.println(which); // print angle
  switch(which){
    case 1:
      shiftOut(serial_in, SRCK, LSBFIRST, B01000000);
      break;
    case 2:
      shiftOut(serial_in, SRCK, LSBFIRST, B00100000);
	  break;
    case 3:
      shiftOut(serial_in, SRCK, LSBFIRST, B00000010);
      break;
    case 4:
      shiftOut(serial_in, SRCK, LSBFIRST, B00010000);
	  break;
    case 5:
      shiftOut(serial_in, SRCK, LSBFIRST, B10000000);
      break;
    case 6:
      shiftOut(serial_in, SRCK, LSBFIRST, B00000100);
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




//HM55B Functions

void ShiftOut(int Value, int BitsCount) {
  for(int i = BitsCount; i >= 0; i--) {
    digitalWrite(CLK_pin, LOW);
    if ((Value & 1 << i) == ( 1 << i)) {
      digitalWrite(DIO_pin, HIGH);
      //Serial.print("1");
    }
    else {
      digitalWrite(DIO_pin, LOW);
      //Serial.print("0");
    }
    digitalWrite(CLK_pin, HIGH);
    delayMicroseconds(1);
  }
}

int ShiftIn(int BitsCount) {
  int ShiftIn_result;
    ShiftIn_result = 0;
    pinMode(DIO_pin, INPUT);
    for(int i = BitsCount; i >= 0; i--) {
      digitalWrite(CLK_pin, HIGH);
      delayMicroseconds(1);
      if (digitalRead(DIO_pin) == HIGH) {
        ShiftIn_result = (ShiftIn_result << 1) + 1; 
      }
      else {
        ShiftIn_result = (ShiftIn_result << 1) + 0;
      }
      digitalWrite(CLK_pin, LOW);
      delayMicroseconds(1);
    }
  //Serial.print(":");

// below is difficult to understand:
// if bit 11 is Set the value is negative
// the representation of negative values you
// have to add B11111000 in the upper Byte of
// the integer.
// see: http://en.wikipedia.org/wiki/Two%27s_complement
  if ((ShiftIn_result & 1 << 11) == 1 << 11) {
    ShiftIn_result = (B11111000 << 8) | ShiftIn_result; 
  }


  return ShiftIn_result;
}

void HM55B_Reset() {
  pinMode(DIO_pin, OUTPUT);
  digitalWrite(EN_pin, LOW);
  ShiftOut(B0000, 3);
  digitalWrite(EN_pin, HIGH);
}

void HM55B_StartMeasurementCommand() {
  pinMode(DIO_pin, OUTPUT);
  digitalWrite(EN_pin, LOW);
  ShiftOut(B1000, 3);
  digitalWrite(EN_pin, HIGH);
}

int HM55B_ReadCommand() {
  int result = 0;
  pinMode(DIO_pin, OUTPUT);
  digitalWrite(EN_pin, LOW);
  ShiftOut(B1100, 3);
  result = ShiftIn(3);
  return result;
}
