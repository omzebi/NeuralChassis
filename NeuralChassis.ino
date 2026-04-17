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

// --- CONFIGURACIÓN DE DIRECCIÓN DE MOTORES ---
// Si al intentar ir recto el coche gira sobre sí mismo, uno de los lados está cambiado.
// Hemos sincronizado los motores, pero estaban yendo hacia atrás. Ahora invertimos ambos.
const bool INVERTIR_MOTOR_A = true;  // Estaba en false -> cambiado a true
const bool INVERTIR_MOTOR_B = false; // Estaba en true -> cambiado a false

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
  // ATENCIÓN FÍSICA:
  // Si usas el sensor TCRT5000 apuntando hacia ABAJO (al suelo) para seguir líneas,
  // el sensor va a creer que el suelo es un "obstáculo" constantemente (estado 0).
  // Por lo tanto, nunca te dejaría dar marcha atrás. 
  // Usa esta función SOLO si pusiste el sensor mirando al aire en la parte trasera.
  
  for(int i=0; i<5; i++) {
    // Algunos sensores dan 1 al detectar pared negra o vacio, y 0 al detectar pared blanca o suelo
    if(digitalRead(IR_PINS[i]) == 0) {
      return true; // Hay algo pegado
    }
  }
  return false;
}

void loop() {
  long currentDist = getDistance();
  
  // HE DESACTIVADO EL BLOQUEO TRASERO TEMPORALMENTE (Cambiado a false en vez de isObstacleBehind)
  // ¿Por qué? Porque si el sensor IR está apuntando al suelo, te bloqueaba todo el coche y Avoidance.
  bool objBehind = false; // <-- Pon esto a isObstacleBehind() SOLO SI APUNTAN HACIA ATRÁS (y no al suelo)

  // Seguridad en modo Manual
  if (currentMode == 'M') {
    if (currentDist < 15) {
      stopMotors();
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

// --- FUNCIONES DE MOTOR MODULARES (Permiten invertir fácilmente) ---
void moveMotorA(int speed, bool forward) {
  if (INVERTIR_MOTOR_A) forward = !forward;
  analogWrite(ENA, speed);
  digitalWrite(IN1, forward ? HIGH : LOW);
  digitalWrite(IN2, forward ? LOW : HIGH);
}

void moveMotorB(int speed, bool forward) {
  if (INVERTIR_MOTOR_B) forward = !forward;
  analogWrite(ENB, speed);
  digitalWrite(IN3, forward ? HIGH : LOW);
  digitalWrite(IN4, forward ? LOW : HIGH);
}

void moveForward() { 
  moveMotorA(baseSpeed, true); 
  moveMotorB(baseSpeed, true); 
}
void moveBackward() { 
  moveMotorA(baseSpeed, false); 
  moveMotorB(baseSpeed, false); 
}
void turnLeft() { 
  moveMotorA(baseSpeed, false); 
  moveMotorB(baseSpeed, true); 
}
void turnRight() { 
  moveMotorA(baseSpeed, true); 
  moveMotorB(baseSpeed, false); 
}
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0); analogWrite(ENB, 0);
}

void modeAvoidance(long d, bool objBehind) {
  if (d < 25 && d > 0) {
    stopMotors();
    delay(100);
    
    // Si no hay obstáculo físico atrás, retrocede
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
  
  // TCRT5000 da 0 (LOW) al detectar linea negra, y 1 (HIGH) al ver suelo blanco.
  // Orden de pines de IZQUIERDA a DERECHA: A0=s[0], A1=s[1], A2=s[2](centro), A3=s[3], A4=s[4]
  // Si girar izquierda/derecha está al revés, intercambia los bloques de s[0]/s[1] con s[3]/s[4].
  
  if (s[2] == 0) {
    // Linea en el centro -> ir recto
    moveForward();
  } else if (s[0] == 0 || s[1] == 0) {
    // Linea a la izquierda -> girar izquierda
    turnLeft();
  } else if (s[3] == 0 || s[4] == 0) {
    // Linea a la derecha -> girar derecha
    turnRight();
  } else {
    // Sin linea: parar (puedes cambiar a moveForward() si prefieres que siga recto cuando pierde)
    stopMotors();
  }
}

void sendTelemetry(long d) {
  // Formato App: P:123|L:10101
  Serial.print("P:"); Serial.print(d); Serial.print("|");
  Serial.print("L:");
  for(int i=0; i<5; i++) Serial.print(digitalRead(IR_PINS[i]));
  Serial.println();
}
