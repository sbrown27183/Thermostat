#define CoolRelay   D4
#define HeatRelay   D3
#define FanRelay    D2

double celsius;
double fahrenheit;
static int tmpAddress = 0b1001000;
static int ResolutionBits = 12;
int TempSum = 1;
int i = 0;  //loop incrementer for actions that happen less frequently than once per loop

struct statstruct
{
  float setpoint = 79; 
  float hysteresis = 1.5;
  //float offset; //for calibration if necessary
  float currentTemp;
  int mode;//mode variable 0 = off, 1 = cool, 2 = heat, 3 = fan only
  int lastmode;
float lastSetpoint;
};
int needsrunout;
int relays = 0;  //sum of : 0 = off, 1 = fan, 2 = cool 4 = heat  Valid states of 1, 3, 5

char status [60];
String modestring;
statstruct state;


Timer runout(20000,runout_done); //software timer to add fan runout after heating or cooling (efficiency!)



void setup()
{
   
   Particle.variable("fahrenheit",&fahrenheit, DOUBLE);
   Particle.variable("getstatus",status, STRING);
   Particle.variable("relaystatus",&relays, INT); //relay status
   Particle.function("setmode",setMode);
   Particle.function("settemp",setTemp);
   
   EEPROM.get(0,state);
   
   RGB.control(true);
   Wire.begin();
   SetResolution();
   pinMode(A0, INPUT);
   pinMode(CoolRelay, OUTPUT);
   pinMode(HeatRelay, OUTPUT);
   pinMode(FanRelay, OUTPUT);
   digitalWrite(FanRelay, HIGH); //turns off fan relay
   digitalWrite(CoolRelay, HIGH);
   digitalWrite(HeatRelay, HIGH);
   
}

void loop()
{
    
    getTemperature();
    state.currentTemp = fahrenheit;
    if(state.mode != state.lastmode)
    {
        relays = setrelay(0);  //no conflicting relay states
    }
    switch (state.mode)
    {
        
        case 0:
            RGB.color(255,255,255);
            RGB.brightness(16);
            relays = setrelay(0);
            break;
        case 1: //cooling mode
            RGB.color(0,0,255);
           if (state.currentTemp > (state.setpoint + state.hysteresis)) //hotter than the setpoint + hysteresis
            {
                needsrunout = 1;
                RGB.brightness(128);
                relays = setrelay(3);
            }
            else if(state.currentTemp < (state.setpoint - state.hysteresis)) //cooler than the setpoint - hysteresis
            {
                
                RGB.brightness(25);
                if(needsrunout)
                {
                    runout_start();
                }
                //relays = setrelay(0); //fan only
            }
            break;
            
        case 2: //heating mode
            RGB.color(255,0,0);
            if (state.currentTemp > (state.setpoint + state.hysteresis))//hotter than the setpoint + hysteresis
            {
                RGB.brightness(25);
                //relays = setrelay(0);
                if(needsrunout)
                {
                    runout_start();
                }
            }
            else if(state.currentTemp < (state.setpoint - state.hysteresis))//cooler than the setpoint - hysteresis
            {
                needsrunout= 1;
                RGB.brightness(128);
                relays = setrelay(5);
            }
            break;
        case 3: //fan mode only
            RGB.color(0,255,0);
            RGB.brightness(128);
            relays = setrelay(1);
            break;
           
    }
    
    
    if(state.lastmode != state.mode || state.setpoint != state.lastSetpoint)
    {
        RGB.color(255,255,0);
        RGB.brightness(255);
        delay(100);
       EEPROM.put(0,state);
       state.lastmode = state.mode;//remembers what mode it was in during the last loop (to prevent unpredictable behavior when stuck between hysteresis points.
       state.lastSetpoint = state.setpoint;
    }
    //analogValue = analogRead(A0); //reading AD value on Pin A0 this is vestigal debug could possibly be an input for stuff
    
    
   
    putstatus();
    Particle.publish("Statechange",status);
    delay(250);
   
        
        
}


int setrelay(int relaystate)
{
switch(relaystate){
    case 0:
        digitalWrite(FanRelay, HIGH); //turns off everything
        digitalWrite(HeatRelay, HIGH); 
        digitalWrite(CoolRelay, HIGH);
        return 0;
        break;
    case 1:
        digitalWrite(FanRelay, LOW); //turn on fan
        digitalWrite(CoolRelay, HIGH);
        digitalWrite(HeatRelay, HIGH);
        return 1;
        break;
    case 3:
        digitalWrite(FanRelay, LOW); //turns on Fan Relay
        digitalWrite(CoolRelay, LOW); //turns on Cool Relay
        digitalWrite(HeatRelay, HIGH);
        return 3;
        break;
    case 5:
        digitalWrite(FanRelay, LOW); //turns on Fan Relay
        digitalWrite(CoolRelay, HIGH); 
        digitalWrite(HeatRelay, LOW);//turns on Heat Relay
        return 5;
        break;
    default:
        return -1; //invalid argument
    }
}

void runout_start()
{
    relays = setrelay(1);
    runout.start();
}

void runout_done()
{
    needsrunout = 0;
    relays = setrelay(0);
    runout.stop();
}

void putstatus()
{
    
   sprintf(status,"%i Relay:%i Setpoint:%.2f Temp:%.4f",state.mode, relays, state.setpoint, state.currentTemp); 
   
}


int setTemp(String command){
    state.setpoint = command.toFloat(); //accepts string command and converts to float
    return 0;
}



int setMode(String command)
{
    if (command == "HEAT")
    {
        state.mode = 2;
    }
    else if (command == "COOL")
    {
        state.mode = 1;
    }
    else if (command == "FAN")
    {
        state.mode = 3;
    }
    else if(command == "OFF")
    {
        state.mode = 0;
    }
    return 0;
}


void getTemperature(){
    int TempSum = 1;
  Wire.requestFrom(tmpAddress,2);
  byte MSB = Wire.read();
  byte LSB = Wire.read();
 
  TempSum = ((MSB << 8) | LSB) >> 4;
 
  celsius = TempSum*0.0625;
  fahrenheit = (1.8 * celsius) + 32;
}
 
  
void SetResolution(){
  //if (ResolutionBits < 9 || ResolutionBits > 12) exit;
  Wire.beginTransmission(tmpAddress);
  Wire.write(0x01); //addresses the configuration register
  Wire.write((ResolutionBits-9) << 5); //writes the resolution bits
  Wire.endTransmission();
 
  Wire.beginTransmission(tmpAddress); //resets to reading the temperature
  Wire.write((byte)0x00);
  Wire.endTransmission();
}