#include <SoftwareSerial.h>

// --- CONFIGURACIÓN DE PINES ---
const int ENA = 3;  const int IN1 = 5;  const int IN2 = 6;
const int ENB = 11; const int IN3 = 9;  const int IN4 = 10;
const int TRIG = 13; const int ECHO = 12;
const int IR_PINS[] = {A0, A1, A2, A3, A4};
SoftwareSerial voiceModule(4, 2); 

char currentMode = 'M'; 
int baseSpeed = 160; 

// --- VARIABLES PARA SEGUIDOR DE LÍNEA ---
// Cámbialo a 0 si tu pista negra no funciona con 1
const int LINE_VALUE = 1; 

void setup() {
  Serial.begin(9600);
  voiceModule.begin(9600);
  
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  for(int i=0; i<5; i++) pinMode(IR_PINS[i], INPUT);
  
  stopMotors();
}

long getDistance() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000); // 30ms timeout (aprox 5 metros)
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void loop() {
  long currentDist = getDistance();

  // Freno automático en Pilot
  if (currentMode == 'M' && currentDist < 15 && currentDist > 0) {
    stopMotors();
  }

  // Comandos Bluetooth y Voz
  if (Serial.available()) processCommand(Serial.read(), currentDist);
  if (voiceModule.available()) {
    char vCmd = voiceModule.read();
    Serial.print("BT:VOICE_INDEX:"); Serial.println((int)vCmd);
    processCommand(vCmd, currentDist);
  }

  if (currentMode == 'A') modeAvoidance(currentDist);
  else if (currentMode == 'X') modeLineFollower();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    sendTelemetry(currentDist);
    lastUpdate = millis();
  }
}

void processCommand(char c, long d) {
  if (d < 15 && (c == 'F' || c == 1)) return; // Freno seguridad adelante

  if (c == 'F' || c == 1) moveForward();
  else if (c == 'B' || c == 2) moveBackward();
  else if (c == 'L' || c == 3) turnLeft();
  else if (c == 'R' || c == 4) turnRight();
  else if (c == 'S' || c == 5) { currentMode = 'M'; stopMotors(); }
  else if (c == 6) { turnRight(); delay(600); stopMotors(); }
  else if (c == 7) { turnLeft(); delay(600); stopMotors(); }
  else if (c == 'A') { currentMode = 'A'; }
  else if (c == 'X') { currentMode = 'X'; }
}

void moveForward() {
  analogWrite(ENA, baseSpeed); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENB, baseSpeed); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void moveBackward() {
  analogWrite(ENA, baseSpeed); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  analogWrite(ENB, baseSpeed); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}
void turnLeft() {
  analogWrite(ENA, baseSpeed); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  analogWrite(ENB, baseSpeed); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void turnRight() {
  analogWrite(ENA, baseSpeed); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENB, baseSpeed); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void modeAvoidance(long d) {
  if (d < 25 && d > 0) {
    stopMotors(); delay(150);
    moveBackward(); delay(400);  // Retroceso vital para no quedarse atascado girando
    turnRight(); delay(500);     // Giro fuerte después de retroceder
    stopMotors(); delay(100);
  } else {
    moveForward();
  }
}

void modeLineFollower() {
  int s[5];
  for(int i=0; i<5; i++) s[i] = digitalRead(IR_PINS[i]);
  
  if (s[2] == LINE_VALUE) moveForward();
  else if (s[0] == LINE_VALUE || s[1] == LINE_VALUE) turnLeft(); // Corrección rápida izquierda
  else if (s[3] == LINE_VALUE || s[4] == LINE_VALUE) turnRight(); // Corrección rápida derecha
  else stopMotors();
}

void sendTelemetry(long d) {
  Serial.print("P:"); Serial.print(d); Serial.print("|");
  Serial.print("L:");
  for(int i=0; i<5; i++) Serial.print(digitalRead(IR_PINS[i]));
  Serial.println();
}
