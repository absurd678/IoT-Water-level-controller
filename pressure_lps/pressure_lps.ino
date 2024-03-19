#include <Wire.h> 
#include <LPS.h> 
#include <SoftwareSerial.h>
 
 // КОНСТАНТЫ
 // LPS
 #define sensorUpdateTime 40 // 40 ms corresponds to the maximum update rate of the sensor (25 Hz)
#define pauseBtSensors 5
#define cycleTime 1000 - sensorUpdateTime - pauseBtSensors
// Другое
#define L 1.5 // Длина трубы 
#define Phi 1000 // Плотность воды 
#define g 9.8 // Ускорение свобод падения 
#define TransimissPin 8 // RS485 pin

// ПЕРЕМЕННЫЕ
LPS ps1; // Это нижний датчик - снаружи 
LPS ps2; // Это верхний - в трубе
SoftwareSerial Master(10, 11);
int time_now; 
float P_atm; // атмосферное давление  
float P_tube; // давление в трубе, Па 
float T_atm; // температура во время измерения снаружи, К
float T_tube; // температура во время измерения в трубе, К 
float T_0; // температура во время установки устройства, К 
int h; // высота столба жидкости в резервуаре, cm
char request;

// ПРОТОТИПЫ ФУНКЦИЙ
float calch(float P_out, float P_in, float T_out, float T_in, float T_install); // Вычисление высоты
void getValues(float& P_out, float& T_out, float& P_in, float& T_in); // Измерения
int temp_constrain(int temp);
int press_constrain(int press, int press_min);

// ГЛАВНАЯ ПРОГРАММА 
void setup() 
{ 
  Serial.begin(9600);
  Master.begin(9600);
  // Настройка датчиков
  Wire.begin(); 
  ps1.init(LPS::device_25H, LPS::sa0_low); // пин наружного датчика к земле
  ps2.init(LPS::device_25H, LPS::sa0_high);  // пин внутреннего датчика к питанию
  ps1.enableDefault(); 
  ps2.enableDefault(); 
  T_0 = ps1.readTemperatureC() + 273.15; 
  for (int i=0; i<3; i++) 
    getValues(P_atm, T_atm, P_tube, T_tube); // Прогреваем lps
  
  // Настройка пина для передачи по RS485
  pinMode(TransimissPin, OUTPUT); 
  delay(10); 
  digitalWrite(TransimissPin, HIGH);  

} // setup
 
void loop() 
{ 
  if(ps1.init() && ps2.init()) 
  { 
    getValues(P_atm, T_atm, P_tube, T_tube);
    h = round(calch(P_atm, P_tube, T_atm, T_tube, T_0)); 

    digitalWrite(TransimissPin, LOW); // Делаем нано слейвом
    if (Master.available()) 
    {
        request = char(Master.read());
        digitalWrite(TransimissPin, HIGH); // Делаем нано боссом
        switch (request)
        {
        case 'h': 
            Master.write(h);
            break;
        case 't':
            Master.write(temp_constrain(T_tube));
            break;
        case 'p':
            Master.write(press_constrain(P_tube, P_atm));
            break;
        }
    } // Отправка на вемос 
  } 
} // loop


// РЕАЛИЗАЦИЯ ФУНКЦИЙ

float calch(float P_out, float P_in, float T_out, float T_in, float T_install) 
{
  float res = -1.0;
  float P_in2 = pow(P_in, 2);
  if(P_in >= P_out) {
    res = ( ( Phi*g*L*P_in*T_install - 
              Phi*g*L*P_out*( T_in ) + // (T_out+T_in)/(2)
              P_in2*T_install - 
              P_out*P_in*T_install ) / 
            ( Phi*g*P_in*T_install ) );
    res = res*100;
    float res_cal = res*(res*(-0.000005*res+0.0002)+1.0316);
    return res_cal;
  }
  else {
    res = 0;
    return res;
  }
}

void getValues(float& P_out, float& T_out, float& P_in, float& T_in) 
{
  delay(sensorUpdateTime); 
  P_out = ps1.readPressureMillibars()*100;
  T_out = ps1.readTemperatureC() + 273.15;
  delay(pauseBtSensors);
  P_in = ps2.readPressureMillibars()*100;
  T_in = ps1.readTemperatureC() + 273.15;
}

int temp_constrain(int temp)
{
    return map(temp, 0, 296, 0, 255);
}
int press_constrain(int press, int press_min)
{
    return map(press, press_min, 100450, 0, 255);
}
