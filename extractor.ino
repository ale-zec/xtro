#include <LiquidCrystal_I2C.h>
#include <CountDown.h>

// Button and interrupt
#define BUTTON_PIN 2 //D2 so interrupt vetor 0
volatile int buttonState;
volatile int lastButtonState = HIGH;
int state = 0; // 0: setting time, 1: filling, 2: extracting, 3: stopped

// Potetiometers 
int potp1 = 0;    
float potv1 = 0;  
int potp2 = 1;    
float potv2 = 0;  

// Time management
CountDown CD(CountDown::MINUTES);
int hours = 0;
int minutes = 0;
char temp[80];
unsigned long lhours = 0;
unsigned long lminutes = 0;
unsigned long lseconds = 0;

// Lcd
LiquidCrystal_I2C lcd(0x3F, 20, 4);

// Linear actuator
byte speed = 0; // Intialize Varaible for the speed of the motor (0-255);
int RPWM = 10;  //connect Arduino pin 10 to IBT-2 pin RPWM
int LPWM = 11;  //connect Arduino pin 11 to IBT-2 pin LPWM

void setup() {
	Serial.begin(9600);

	// Actuator setup
	pinMode(10, OUTPUT);
	pinMode(11, OUTPUT);
	
	// Button setup
	pinMode(BUTTON_PIN, INPUT_PULLUP);	// enable the internal pull-up resistor: reads high when not pressed
	attachInterrupt(0, pin_ISR, CHANGE);	// (interrupt vector, interrupt function, interrupt trigger), gets pin from vector
	
	// LCD setup
	lcd.init();
	lcd.backlight();
}

void loop() {
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Welcome!");
	
	delay(2000);

	setTime();
	
	insert();
	
	extract();
	
	stop();
}

void setTime()
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Input time:");
	while(true)
	{
		potv1 = analogRead(potp1);
		potv2 = analogRead(potp2);
		
		hours = (potv1/1023)*10; // normalize [0-y]: y*(x-max(x))/(max(x)-min(x)); 
		minutes = (potv2/1023)*14; // normalizing between 0 and 14
		minutes = minutes*4; // normalizing between 0 and 56 in increments of 4
		
		sprintf(temp, "%02d:%02d", hours, minutes);
		lcd.setCursor(0, 1);
		lcd.print(temp);

		if(state!=0) //check if button has been pressed
			break;
	}
}

void insert() // piston starts from fully retracted, extend enough to insert piston, needs plunge test, NO INTERRUPTS
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Inserting...");
	
	//extend at half speed
	speed = 127;
	analogWrite(RPWM, 0);
	analogWrite(LPWM, speed);
	
	delay(7000); //plunge test
	
	//stop actuator
	analogWrite(RPWM, 0);
	analogWrite(LPWM, 0);

  lcd.clear();

  while(true) //wait for filling, interrupt won't trigger from empty loop so we print inside
	{
		lcd.setCursor(0, 0);
		lcd.print("Fill up");
		lcd.setCursor(0, 1);
		lcd.print("Press to start");
		
		if(state==2)
			break;
	}
}

void extract()
{

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Extracting:");

	unsigned long duration = (long)hours*60*60*1000+(long)minutes*60*1000; //converting input time to millis
	unsigned long remaining = 0;
	unsigned long b = millis(); // tracks progress of current phase
	unsigned long current = 0;
	
	int phase = 0; // 0: active, 1: passive
	int active_phase = 0; // 0: inactive, 1: extending, 2: retracting
	int passive_phase = 0; // 0: inactive, 1: extending, 2: waiting, 3: retracting
	
	CD.start(0,0,0,duration/1000);
	remaining = CD.remaining();
	
	while(remaining>0)
	{	
		current = millis();
		
		if(phase==0) // active phase
		{
			if(active_phase==0) // if inactive, start extending at half speed
			{
				active_phase = 1; //state to extending
				b = millis(); //update beginning
				
				//extend at half speed
				speed = 127;
				analogWrite(RPWM, 0);
				analogWrite(LPWM, speed);
			}
			else if((active_phase==1) && ((current-b)>=2000)) // if finised extending, stop and start retracting
			{
				//stop actuator
				analogWrite(RPWM, 0);
				analogWrite(LPWM, 0);
				
				active_phase = 2; //state to retracting
				b = millis(); //update beginning
				
				//retract at half speed
				speed = 127;
				analogWrite(RPWM, speed);
				analogWrite(LPWM, 0);			
			}
			else if((active_phase==2) && ((current-b)>=2000)) // if finished retracting, stop, reset active phase and switch to passive
			{
				//stop actuator
				analogWrite(RPWM, 0);
				analogWrite(LPWM, 0);
				
				active_phase = 0; // reset active phase
				phase = 1; // switch to passive
			}
		}
		else if(phase==1) // passive phase
		{
			if(passive_phase==0) // if inactive, start extending at quarter speed for 4s
			{
				passive_phase = 1; //state to extending
				b = millis(); //update beginning
				
				//extend at quarter speed
				speed = 64;
				analogWrite(RPWM, 0);
				analogWrite(LPWM, speed);
			}
			else if((passive_phase==1) && ((current-b)>=4000)) // if finised extending, stop and wait 50s
			{
				//stop actuator
				analogWrite(RPWM, 0);
				analogWrite(LPWM, 0);
				
				passive_phase = 2; //state to waiting
				b = millis(); //update beginning	
			}
			else if((passive_phase==2) && ((current-b)>=50000)) // if finished waiting, start retracting for 4s
			{
				
				passive_phase = 3; //state to retracting
				b = millis(); //update beginning
				
				//retract at quarter speed
				speed = 64;
				analogWrite(RPWM, speed);
				analogWrite(LPWM, 0);			
			}
			else if((passive_phase==3) && ((current-b)>=4000)) // if finished retracting, stop , reset passive phase and switch to active
			{
				//stop actuator
				analogWrite(RPWM, 0);
				analogWrite(LPWM, 0);
				
				passive_phase = 0; // reset passive phase
				phase = 0; // switch to active
			}
		}
		
		//countdown
		lseconds = remaining;
		lminutes = lseconds / 60;
		lhours = lminutes / 60;
		lseconds %= 60;
		lminutes %= 60;
		lhours %= 24;
		sprintf(temp, "%02ld:%02ld:%02ld", lhours, lminutes, lseconds);
		lcd.setCursor(0, 1);
		lcd.print(temp);

		remaining = CD.remaining();
			
		if(state==3)
			break;
	}
	
	CD.stop();	
	state = 3;
}

void stop()
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Stopping...");
	
	//stop actuator
	analogWrite(RPWM, 0);
	analogWrite(LPWM, 0);
	
	// fully retract piston
	speed = 127;
	analogWrite(RPWM, speed);
	analogWrite(LPWM, 0);
	
	delay(10000); // plunge test + some sec
	
	//stop actuator
	analogWrite(RPWM, 0);
	analogWrite(LPWM, 0);
	
	lcd.clear();
	
	while(true) //wait for restart, interrupt won't trigger from empty loop so we print inside
	{
		lcd.setCursor(0, 0);
		lcd.print("Press to restart");
		
		if(state==0)
			break;
	}
}

// INTERRUPT FUNCTION
void pin_ISR() 
{
	static unsigned long last_interrupt_time = 0;
	unsigned long interrupt_time = millis();

	// if interrupts come faster than 500ms, assume it's a bounce and ignore
	if (interrupt_time - last_interrupt_time > 500) 
	{
		buttonState = digitalRead(BUTTON_PIN); 

		if(buttonState==HIGH && (interrupt_time - last_interrupt_time >=1000) ) // if interrupts last less than 1s assume it's noise
		{
		  if(state==0) // if we are setting time, we switch to filling 
			{
				state = 1;
			}
			else if(state==1) // if we are filling, switch to extracting
			{
				state = 2;
			}
			else if(state==2) // if we are extracting, we get out of extract so piston stops and retracts, and loop restarts
			{
				state = 3;
			}
			else if(state==3) // if we are stopped, restart
			{
				state = 0;
			}
		}

	}
	last_interrupt_time = interrupt_time;
}

