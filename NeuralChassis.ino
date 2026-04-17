#include <SoftwareSerial.h>

// --- CONFIGURACIÓN DE PINES ---
const int ENA = 3;  const int IN1 = 5;  const int IN2 = 6;
const int ENB = 11; const int IN3 = 9;  const int IN4 = 10;
const int TRIG = 13; const int ECHO = 12;
const int IR_PINS[] = {A0, A1, A2, A3, A4};
SoftwareSerial voiceModule(4, 2); 

char currentMode = 'M'; 
char currentMoveState = 'S'; 
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

// Devuelve TRUE si cualquiera de los 5 sensores traseros ve un obstáculo
bool isObstacleBehind() {
  for(int i=0; i<5; i++) {
    // 0 = Hay obstáculo (luz infrarroja rebota)
    if(digitalRead(IR_PINS[i]) == 0) return true; 
  }
  return false;
}

void loop() {
  long currentDist = getDistance();
  bool rearObstacle = isObstacleBehind();

  // Freno dinámico de seguridad absoluta por lectura de Motor (Hardware check)
  if (currentMode == 'M') {
    // Si algún motor está yendo Hacia Adelante (IN1 o IN3 en HIGH) y hay muro delante:
    if ((digitalRead(IN1) == HIGH || digitalRead(IN3) == HIGH) && currentDist < 15 && currentDist > 0) {
      stopMotors();
      currentMoveState = 'S';
    }
    // Si algún motor está yendo Hacia Atrás (IN2 o IN4 en HIGH) y hay muro detrás:
    if ((digitalRead(IN2) == HIGH || digitalRead(IN4) == HIGH) && rearObstacle) {
      stopMotors();
      currentMoveState = 'S';
    }
  }

  // Comandos Bluetooth y Voz
  if (Serial.available()) processCommand(Serial.read(), currentDist, rearObstacle);
  if (voiceModule.available()) {
    char vCmd = voiceModule.read();
    Serial.print("BT:VOICE_INDEX:"); Serial.println((int)vCmd);
    processCommand(vCmd, currentDist, rearObstacle);
  }

  if (currentMode == 'A') modeAvoidance(currentDist);
  else if (currentMode == 'X') modeLineFollower();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    sendTelemetry(currentDist);
    lastUpdate = millis();
  }
}

void processCommand(char c, long d, bool rearObstacle) {
  if (d < 15 && (c == 'F' || c == 1)) return; // Freno seguridad adelante
  if (rearObstacle && (c == 'B' || c == 2)) return; // Freno seguridad ATRÁS (IR trasero)

  if (c == 'F' || c == 1) { moveForward(); currentMoveState = 'F'; }
  else if (c == 'B' || c == 2) { moveBackward(); currentMoveState = 'B'; }
  else if (c == 'L' || c == 3) { turnLeft(); currentMoveState = 'L'; }
  else if (c == 'R' || c == 4) { turnRight(); currentMoveState = 'R'; }
  else if (c == 'S' || c == 5) { currentMode = 'M'; stopMotors(); currentMoveState = 'S'; }
  else if (c == 6) { turnRight(); delay(600); stopMotors(); currentMoveState = 'S'; }
  else if (c == 7) { turnLeft(); delay(600); stopMotors(); currentMoveState = 'S'; }
  else if (c == 'A') { currentMode = 'A'; currentMoveState = 'S'; }
  else if (c == 'X') { currentMode = 'X'; currentMoveState = 'S'; }
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
