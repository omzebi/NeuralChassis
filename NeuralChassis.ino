// --- CONFIGURACIÓN DE PINES ---
const int ENA = 3;  const int IN1 = 5;  const int IN2 = 6;
const int ENB = 11; const int IN3 = 9;  const int IN4 = 10;
const int TRIG = 13; const int ECHO = 12;

// TCRT5000 x5 (seguimiento de línea, sensores centrales)
const int IR_PINS[] = {A0, A1, A2, A3, A4};

// FC-51 x2 (sensores extremos izquierdo/derecho para seguimiento de línea)
// ¡YA NO SE USAN PARA OBSTÁCULOS! Estaban demasiado cerca del suelo y bloqueaban la marcha atrás.
const int FC51_LEFT  = 4;
const int FC51_RIGHT = 2;

char currentMode = 'M';
char lastDir = 'S';
int baseSpeed = 170;

// --- CONFIGURACIÓN DE DIRECCIÓN DE MOTORES ---
// ¡NO CAMBIAR! Configuración validada y guardada en HARDWARE_CONFIG.md
const bool INVERTIR_MOTOR_A = true;
const bool INVERTIR_MOTOR_B = false;

void setup() {
  Serial.begin(9600);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  for(int i=0; i<5; i++) pinMode(IR_PINS[i], INPUT);

  // FC-51 como sensores de línea (extremos izquierdo y derecho)
  pinMode(FC51_LEFT,  INPUT_PULLUP);
  pinMode(FC51_RIGHT, INPUT_PULLUP);

  stopMotors();
  Serial.println("Neural Chassis Ready!");
}

long getDistance() {
  digitalWrite(TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 25000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void loop() {
  long currentDist = getDistance();

  // SEGURIDAD: solo sensor ultrasónico delante.
  // FC-51 ya no se usan para obstáculos (estaban detectando el suelo).
  if (currentMode == 'M') {
    if (lastDir == 'F' && currentDist > 0 && currentDist < 12) {
      stopMotors(); lastDir = 'S'; // Para si va adelante y hay pared
    }
  }

  // Lectura Bluetooth
  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd, currentDist);
  }

  // Ejecución de Modos Automáticos
  if      (currentMode == 'A') modeAvoidance(currentDist);
  else if (currentMode == 'X') modeLineFollower();

  // Telemetría
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    sendTelemetry(currentDist);
    lastUpdate = millis();
  }
}

void processCommand(char c, long currentDist) {
  // Solo bloqueamos avanzar si hay pared delante (ultrasónico)
  if (currentDist > 0 && currentDist < 12 && c == 'F') return;

  if      (c == 'F') { moveForward();  lastDir = 'F'; }
  else if (c == 'B') { moveBackward(); lastDir = 'B'; }
  else if (c == 'L') { turnLeft();     lastDir = 'L'; }
  else if (c == 'R') { turnRight();    lastDir = 'R'; }
  else if (c == 'S') { stopMotors();   lastDir = 'S'; currentMode = 'M'; }
  else if (c == 'A') { currentMode = 'A'; }
  else if (c == 'X') { currentMode = 'X'; }
}

// --- FUNCIONES DE MOTOR ---
void moveMotorA(int speed, bool forward) {
  if (INVERTIR_MOTOR_A) forward = !forward;
  analogWrite(ENA, speed);
  digitalWrite(IN1, forward ? HIGH : LOW);
  digitalWrite(IN2, forward ? LOW  : HIGH);
}
void moveMotorB(int speed, bool forward) {
  if (INVERTIR_MOTOR_B) forward = !forward;
  analogWrite(ENB, speed);
  digitalWrite(IN3, forward ? HIGH : LOW);
  digitalWrite(IN4, forward ? LOW  : HIGH);
}

void moveForward()  { moveMotorA(baseSpeed, true);  moveMotorB(baseSpeed, true);  }
void moveBackward() { moveMotorA(baseSpeed, false); moveMotorB(baseSpeed, false); }
void turnLeft()     { moveMotorA(baseSpeed, false); moveMotorB(baseSpeed, true);  }
void turnRight()    { moveMotorA(baseSpeed, true);  moveMotorB(baseSpeed, false); }
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);    analogWrite(ENB, 0);
}

// --- MODO EVITAR OBSTÁCULOS (solo ultrasónico delante) ---
void modeAvoidance(long d) {
  if (d < 25 && d > 0) {
    stopMotors(); delay(100);
    moveBackward(); delay(350);
    turnRight();    delay(450);
    stopMotors(); delay(100);
  } else {
    moveForward();
  }
}

// --- MODO SEGUIMIENTO DE LÍNEA ---
// Usa 7 sensores en total: FC51_LEFT | TCRT[0..4] | FC51_RIGHT
// TCRT5000 y FC-51: LOW (0) = línea negra, HIGH (1) = suelo claro
// Sensor montado detrás → coche va hacia ATRÁS, giros invertidos
void modeLineFollower() {
  int s[5];
  for(int i=0; i<5; i++) s[i] = digitalRead(IR_PINS[i]);
  bool fLeft  = (digitalRead(FC51_LEFT)  == LOW); // FC-51 extremo izquierdo
  bool fRight = (digitalRead(FC51_RIGHT) == LOW); // FC-51 extremo derecho

  if (s[2] == 0) {
    // Centro: ir atrás recto
    moveBackward();
  } else if (fLeft || s[0] == 0 || s[1] == 0) {
    // Línea a la izquierda (incluyendo FC-51 extremo) → girar derecha
    turnRight();
  } else if (fRight || s[3] == 0 || s[4] == 0) {
    // Línea a la derecha (incluyendo FC-51 extremo) → girar izquierda
    turnLeft();
  } else {
    stopMotors(); // Línea perdida
  }
}

// --- TELEMETRÍA ---
void sendTelemetry(long d) {
  Serial.print("P:"); Serial.print(d); Serial.print("|");
  Serial.print("L:");
  for(int i=0; i<5; i++) Serial.print(digitalRead(IR_PINS[i]));
  Serial.println();
}
