#include <SoftwareSerial.h>

// --- CONFIGURACIÓN DE PINES ---
const int ENA = 3;  const int IN1 = 5;  const int IN2 = 6;
const int ENB = 11; const int IN3 = 9;  const int IN4 = 10;
const int TRIG = 13; const int ECHO = 12;
const int IR_PINS[] = {A0, A1, A2, A3, A4};

// Módulo de Voz VC-02 (D2=RX, D3=TX)
SoftwareSerial voiceModule(2, 3); 

char currentMode = 'M'; 
int baseSpeed = 180;

void setup() {
  Serial.begin(9600); // Bluetooth
  voiceModule.begin(9600);
  
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  for(int i=0; i<5; i++) pinMode(IR_PINS[i], INPUT);
  
  stopMotors();
}

void loop() {
  // Bluetooth Command
  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd);
  }
  
  // Voice Command Sniffer
  if (voiceModule.available()) {
    char vCmd = voiceModule.read();
    
    // Enviar a la App para debug
    Serial.print("BT:VOICE_CMD:");
    Serial.println(vCmd);
    
    processCommand(vCmd);
  }

  if (currentMode == 'A') modeAvoidance();
  else if (currentMode == 'X') modeLineFollower();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    sendTelemetry();
    lastUpdate = millis();
  }
}

void processCommand(char c) {
  if (c == 'F') moveForward();
  else if (c == 'B') moveBackward();
  else if (c == 'L') turnLeft();
  else if (c == 'R') turnRight();
  else if (c == 'S') { currentMode = 'M'; stopMotors(); }
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

void modeAvoidance() {
  long duration, distance;
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;
  if (distance < 20 && distance > 0) { turnRight(); delay(300); stopMotors(); }
  else moveForward();
}

void modeLineFollower() {
  int s[5];
  for(int i=0; i<5; i++) s[i] = digitalRead(IR_PINS[i]);
  if (s[2] == 1) moveForward();
  else if (s[0] == 1 || s[1] == 1) turnLeft();
  else if (s[3] == 1 || s[4] == 1) turnRight();
  else stopMotors();
}

void sendTelemetry() {
  long duration, distance;
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;
  Serial.print("P:"); Serial.print(distance); Serial.print("|");
  Serial.print("L:");
  for(int i=0; i<5; i++) Serial.print(digitalRead(IR_PINS[i]));
  Serial.println();
}
