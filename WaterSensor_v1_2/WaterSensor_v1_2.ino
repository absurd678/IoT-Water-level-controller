// All
#include <splash.h>
#include <gfxfont.h>
#include <Adafruit_SPITFT_Macros.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_GrayOLED.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// SD1306
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// КОНСТАНТЫ
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define BUT D0
#define enablePin D5
#define cycleTime 1000

// ОБЪЕКТЫ

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClient espClient;
PubSubClient client(espClient);


//ПЕРЕМЕННЫЕ

int h; // [h] = cm
// Заряд накопителя, дисплей
uint8_t ERR = 0;
uint16_t bat_charge = 0;
uint8_t bat_charge_percent = 0;
uint16_t bat_max = 621;
uint16_t bat_min = 400;
uint16_t bat_range = 221;
uint8_t sens_mode = 0;
uint8_t is_display = 1;
uint16_t display_on_time = 30000;
uint32_t display_on_time_begin = 0;
uint32_t current_time = 0;
uint32_t second_counter = 0;
char data_mas[100]= {0};

// MQTT сервер
const char* ssid = "iPhone (Artyom)";                         // имя сети
const char* password = "8nphd2s4y2jjk";                      // пароль
const char* mqtt_server = "172.20.10.12";            // mqtt сервер
const int client_port = 1883;                           // номер порта
const char* s_client = "esp_client";                         // название клиента(должен отличаться)
const char* s_subscribe = "WS_IN";                       // Подписка модуля
const char* s_publish = "WS_OUT";                         // Публикация модуля
uint8_t WL_TIMEOUT = 30;
uint8_t WL_STATUS = 0;


// ПРОТОТИПЫ ФУНКЦИЙ

// MQTT
void setup_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
// SD1306
void display_wifi_disconnect();
void display_wifi_init();
void display_error(uint8_t ERR);
void display_height_value(float height);
void calc_charge(); // Уровень заряда
void display_charge();
void display_hello();


// ОСНОВНАЯ ПРОГРАММА

void setup()
{
  Serial.begin(9600);
  while (!Serial);  

  Wire.begin();
  Wire.setClock(400000);
  delay(10);

  // Настройка пина RS485
  pinMode(enablePin, OUTPUT);
  delay(10);
  digitalWrite(enablePin, LOW);        //  (Pin 8 always LOW to receive value from Master)
  
  // Настройка дисплея
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) // Address 0x3D or 0x3C for 128x64
    ERR |= (1<<0); // Если дисплей не подключен
  else
  {
    display.clearDisplay();
    display.display();
  }
  delay(250);
  
  if (ERR) // Обработка ошибок
  {
    Serial.println(ERR);
    if (!(ERR & 0x01))
      display_error(ERR);
    while(1);
  }  
  
  // Настройка соединения
  delay(cycleTime);
  display_wifi_init();
  setup_wifi();
  delay(10);
  if (WL_STATUS == 1)
  {
    client.setServer(mqtt_server, client_port); // Установка соединения с сервером
    client.setCallback(callback); // Установка функции обработки подписок  
  }             

}

//##########

void loop()
{
  h = Serial.parseInt(); // Читаем высоту с платы MAX485

  if (millis() - current_time > 1000)
  {
      sprintf(data_mas, "%d", h); // высоту делаем строкой
      client.publish(s_publish, data_mas); // отправка на сервер
    current_time = millis();
    if (is_display)
    {
      if (sens_mode == 0) 
        display_height_value(h);
      else if (sens_mode == 1)
        display_charge();
    }

  }

  if (digitalRead(BUT) && (is_display))
  {
    
    sens_mode ++;
    if (sens_mode > 1)
      sens_mode = 0;
    //display_on_time_begin = millis(); Я хз но так раз в 30 секунд экран меняется
  }
  else if (digitalRead(BUT) && (!is_display))
  {
    display_on_time_begin = millis();
    is_display = 1;
  }

  if (millis() - display_on_time_begin > display_on_time)
  {
    is_display = 0;
    display.clearDisplay();
    display.display();
  }
    
  if (WL_STATUS == 1)
  {
    if (!client.connected())
    { reconnect(); }
    else client.loop();
      
  }
      
}

//РЕАЛИЗАЦИЯ ФУНКЦИЙ

// Сервер

void setup_wifi() //Соединение по WiFi
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE); 
  display.setCursor(0,0);

  delay(10);
  uint8_t WL_CURRENT_TIMEOUT = 0;

  display.println();
  display.print("Connecting to ");
  display.setCursor(0,20); display.println(ssid);
  display.display();

  WiFi.begin(ssid, password);
  while ((WiFi.status() != WL_CONNECTED)&&(WL_CURRENT_TIMEOUT < WL_TIMEOUT)) {
    delay(500);
    WL_CURRENT_TIMEOUT++;
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    WL_STATUS = 0;
    WiFi.disconnect();
    display.setCursor(0,40);
    display.println("WiFi disconnected");
    display.display();
  }
  else
  {
    WL_STATUS = 1;
//
    display.setCursor(0,40);
    display.println("WiFi connected");
    display.setCursor(0,60); display.println("IP address: ");
    display.setCursor(0,80); display.println(WiFi.localIP());
    display.display();
  }
  delay(1500);
}

void reconnect() // Переподключение
{
  uint8_t WL_CURRENT_TIMEOUT = 0;
  display_on_time_begin = millis();
  is_display = 1;
  display_wifi_reconnect();
  while ((!client.connected())&&(WL_CURRENT_TIMEOUT < 20)) {
    // Attempt to connect
    if (client.connect(s_client)) {
      //Serial.println("connected");
      client.publish(s_publish, "connect\n");
      client.subscribe(s_subscribe);
    }
    else
    {
      //Serial.print("failed, rc=");
      //Serial.print(client.state());
      //Serial.println(" try again in 5 seconds");
      WL_CURRENT_TIMEOUT++;
      delay(500);
    }
  }
  if (!client.connected())
  {
    WL_STATUS = 0;
    WiFi.disconnect();
  }
}

// Отправка h на сервер
void callback(char* topic, byte* payload, unsigned int length) 
{

  if (payload[0] == '1')
  {
    snprintf (data_mas, 100, "%u,%f,%d\n", millis(), h, bat_charge_percent);
    client.publish(s_publish, data_mas);
  }
}

// Дисплей

void display_wifi_disconnect()
{
  display.fillRect(112,7, 7,7,  WHITE);
}

void display_wifi_init() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE); 
  display.setCursor(30,0);
  display.println("WIFI");
  display.setCursor(30,40);
  display.println("INIT");
  display.display();
  delay(100);
}

void display_wifi_reconnect() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE); 
  display.setCursor(30,0);
  display.println("MQTT");
  display.setCursor(12,40);
  display.println("RECONN");
  //display.drawLine (10,30, 117,30, WHITE);
  display.display();
  delay(100);
}

void display_error(uint8_t ERR) {
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE); 
  display.setCursor(20,0);
  display.println("ERROR");
  //display.drawLine (10,30, 117,30, WHITE);
  display.setCursor(20,40);
  display.print("0x");
  display.println(ERR, HEX);
  display.display();
  delay(100);
}

void display_height_value(float height) {
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE); 
  if (height < 10)
  {
    display.setCursor(40,0);
  }
  else if (height < 100)
  {
    display.setCursor(30,0);
  }
  else
  {
    display.setCursor(20,0);
  }
  if (height > 100)
  {
     height = 100;
     //display.fillRect(112,7, 7,7,  WHITE);
  }
  display.println(height, 1); 
  display.setTextSize(1);
  display.setCursor(5,30);
  display.println(0);
  display.setCursor(35,30);
  display.println("Height, cm");
  display.setCursor(110,30);
  display.println(100);
  //display.drawLine (10,30, 117,30, WHITE);
  display.drawRect(10,40, 110,20,  WHITE);
  display.fillRect(10,40, 10+(uint8_t)height,20,  WHITE);
  if(WL_STATUS == 0)
    display_wifi_disconnect();
  display.display();
}

void calc_charge()
{
   bat_charge = 0;
   for (uint8_t i =0; i <4; i++)
    bat_charge += analogRead(A0);
   bat_charge = bat_charge>>2;
   if (bat_charge > bat_max)
    bat_charge = bat_max;
   else if (bat_charge < bat_min)
    bat_charge = bat_min;
   bat_charge_percent = ((bat_charge -bat_min)*100)/bat_range;
}

void display_charge()
{
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(38,0);
  display.println("BAT");
  if (bat_charge_percent < 10)
  {
    display.setCursor(47,40);
  }
  else 
  {
    display.setCursor(38,40);
  }
  display.print(bat_charge_percent);
  display.println("%");
  if (WL_STATUS == 0)
    display_wifi_disconnect();
  display.display();
}

void display_hello()
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("HELLO I'M HERE");
  display.display();
}


