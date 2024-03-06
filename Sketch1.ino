/*
 Name:		Sketch1.ino
 Created:	02.03.2024 22:23:42
 Author:	Artem
*/

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(9600);
}

// the loop function runs over and over again until power down or reset
void loop() {
	Serial.println("tick");
}
