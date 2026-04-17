#include <SoftwareSerial.h>

// --- CONFIGURACIÓN DE PINES ---
const int ENA = 3;  const int IN1 = 5;  const int IN2 = 6;
const int ENB = 11; const int IN3 = 9;  const int IN4 = 10;
const int TRIG = 13; const int ECHO = 12;
const int IR_PINS[] = {A0, A1, A2, A3, A4};

// Módulo de Voz VC-02
SoftwareSerial voiceModule(4, 2); 

char currentMode = 'M'; 
int baseSpeed = 170; // Velocidad ajustada para mejor control

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
  long duration = pulseIn(ECHO, HIGH, 25000); // Timeout 25ms
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

// Función auxiliar para saber si hay obstáculo detrás (usando matriz IR)
bool isObstacleBehind() {
  // OJO: Los TCRT5000 suelen dar LOW (0) al detectar obstáculo.
  // Si tu sensor funciona al revés (detecta con 1), cambia esto a "== 1"
  for(int i=0; i<5; i++) {
    if(digitalRead(IR_PINS[i]) == 0) {
      return true; // Hay algo pegado atrás
    }
  }
  return false;
}

void loop() {
  long currentDist = getDistance();
  bool objBehind = isObstacleBehind();

  // Seguridad en modo Manual
  if (currentMode == 'M') {
    // Si frena de frente
    if (currentDist < 15) {
      digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
    }
  }

  // Lectura Bluetooth
  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd, currentDist, objBehind);
  }
  
  // Lectura Voz
  if (voiceModule.available()) {
    char vCmd = voiceModule.read();
    Serial.print("BT:VOICE_INDEX:");
    Serial.println((int)vCmd);
    processCommand(vCmd, currentDist, objBehind);
  }

  // Ejecución de Modos
  if (currentMode == 'A') modeAvoidance(currentDist, objBehind);
  else if (currentMode == 'X') modeLineFollower();

  // Telemetría
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    sendTelemetry(currentDist);
    lastUpdate = millis();
  }
}

void processCommand(char c, long currentDist, bool objBehind) {
  // BLOQUEO DE SEGURIDAD
  if (currentDist < 15 && (c == 'F' || c == 1)) return; // No ir adelante si hay pared
  if (objBehind && (c == 'B' || c == 2)) return;      // No ir atrás si hay pared (IR)

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

void modeAvoidance(long d, bool objBehind) {
  if (d < 25 && d > 0) {
    stopMotors();
    delay(100);
    
    // Si hay que esquivar un obstáculo de frente, 
    // solo retrocede si NO hay un obstáculo detrás (IR libres)
    if (!objBehind) {
      moveBackward(); 
      delay(300);
    }
    
    turnRight();    
    delay(450);
    stopMotors();
    delay(100);
  } else {
    moveForward();
  }
}

void modeLineFollower() {
  int s[5];
  for(int i=0; i<5; i++) s[i] = digitalRead(IR_PINS[i]);
  if (s[2] == 1) moveForward();
  else if (s[0] == 1 || s[1] == 1) turnLeft();
  else if (s[3] == 1 || s[4] == 1) turnRight();
  else stopMotors();
}

void sendTelemetry(long d) {
  // Formato App: P:123|L:10101
  Serial.print("P:"); Serial.print(d); Serial.print("|");
  Serial.print("L:");
  for(int i=0; i<5; i++) Serial.print(digitalRead(IR_PINS[i]));
  Serial.println();
}
