// --- CONFIGURACIÓN DE PINES ---
const int ENA = 3;  const int IN1 = 5;  const int IN2 = 6;
const int ENB = 11; const int IN3 = 9;  const int IN4 = 10;
const int TRIG = 13; const int ECHO = 12;
const int IR_PINS[] = {A0, A1, A2, A3, A4}; // TCRT5000 x5 (seguimiento de línea)

// FC-51 Obstáculos laterales/traseros
// FC-51 da LOW (0) cuando detecta obstáculo, HIGH (1) cuando está libre
const int FC51_LEFT  = 4;  // Sensor izquierdo
const int FC51_RIGHT = 2;  // Sensor derecho

char currentMode = 'M';
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

  // FC-51 con INPUT_PULLUP para evitar lecturas flotantes
  pinMode(FC51_LEFT,  INPUT_PULLUP);
  pinMode(FC51_RIGHT, INPUT_PULLUP);

  stopMotors();
  Serial.println("Neural Chassis Ready!");
}

long getDistance() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 25000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

// FC-51: LOW=obstáculo, HIGH=libre
bool obstacleLeft()  { return digitalRead(FC51_LEFT)  == LOW; }
bool obstacleRight() { return digitalRead(FC51_RIGHT) == LOW; }
bool isObstacleBehind() { return obstacleLeft() || obstacleRight(); }

void loop() {
  long currentDist = getDistance();
  bool bLeft  = obstacleLeft();
  bool bRight = obstacleRight();
  bool objBehind = bLeft || bRight;

  // Frenado de emergencia SOLO en modo Avoidance automatico.
  // En modo Manual, la seguridad se gestiona en processCommand (bloquea el comando, no para el motor).
  // Esto evita que el loop() cancele a cada ciclo lo que el joystick está pidiendo.

  // Lectura Bluetooth
  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd, currentDist, objBehind);
  }

  // Ejecución de Modos Automáticos
  if (currentMode == 'A') modeAvoidance(currentDist, bLeft, bRight);
  else if (currentMode == 'X') modeLineFollower();

  // Telemetría
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    sendTelemetry(currentDist, bLeft, bRight);
    lastUpdate = millis();
  }
}

void processCommand(char c, long currentDist, bool objBehind) {
  // Bloqueos de seguridad
  if (currentDist < 15 && c == 'F') return;  // No ir adelante si hay pared
  if (objBehind      && c == 'B') return;     // No ir atrás si hay obstáculo

  if      (c == 'F') moveForward();
  else if (c == 'B') moveBackward();
  else if (c == 'L') turnLeft();
  else if (c == 'R') turnRight();
  else if (c == 'S') { currentMode = 'M'; stopMotors(); }
  else if (c == 'A') { currentMode = 'A'; }
  else if (c == 'X') { currentMode = 'X'; }
}

// --- FUNCIONES DE MOTOR MODULARES ---
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

// --- MODO EVITAR OBSTÁCULOS (con FC-51 laterales) ---
void modeAvoidance(long d, bool bLeft, bool bRight) {
  // 1. Obstáculo delante (ultrasonidos)
  if (d < 25 && d > 0) {
    stopMotors();
    delay(100);

    // Retroceder si FC-51 traseros libres
    if (!bLeft && !bRight) {
      moveBackward();
      delay(350);
    }

    // Elegir giro inteligente según qué lateral está libre
    if (!bLeft && bRight) {
      turnLeft(); delay(450);   // Derecha bloqueada → girar izquierda
    } else {
      turnRight(); delay(450);  // Por defecto girar derecha
    }

    stopMotors();
    delay(100);
    return;
  }

  // 2. Obstáculo lateral izquierdo: desviar a la derecha
  if (bLeft && !bRight) {
    turnRight();
    delay(200);
    return;
  }

  // 3. Obstáculo lateral derecho: desviar a la izquierda
  if (bRight && !bLeft) {
    turnLeft();
    delay(200);
    return;
  }

  // 4. Ambos bloqueados: parar
  if (bLeft && bRight) {
    stopMotors();
    return;
  }

  // 5. Camino libre -> avanzar
  moveForward();
}

// --- MODO SEGUIMIENTO DE LÍNEA (TCRT5000 x5, sensor trasero) ---
void modeLineFollower() {
  int s[5];
  for(int i=0; i<5; i++) s[i] = digitalRead(IR_PINS[i]);

  // TCRT5000: 0=línea negra, 1=suelo blanco
  // Sensor trasero → coche va hacia ATRÁS, giros invertidos
  if (s[2] == 0) {
    moveBackward();     // Centro: ir atrás
  } else if (s[0] == 0 || s[1] == 0) {
    turnRight();        // Línea a la izquierda del sensor trasero → girar derecha
  } else if (s[3] == 0 || s[4] == 0) {
    turnLeft();         // Línea a la derecha del sensor trasero → girar izquierda
  } else {
    stopMotors();       // Sin línea: parar
  }
}

// --- TELEMETRÍA para la App ---
void sendTelemetry(long d, bool bLeft, bool bRight) {
  // Formato: P:123|L:10101|O:LD (O=Obstacle: L=Left, R=Right, D=clear)
  Serial.print("P:"); Serial.print(d); Serial.print("|");
  Serial.print("L:");
  for(int i=0; i<5; i++) Serial.print(digitalRead(IR_PINS[i]));
  Serial.print("|O:");
  if (bLeft)  Serial.print("L");
  if (bRight) Serial.print("R");
  if (!bLeft && !bRight) Serial.print("D"); // D = despejado
  Serial.println();
}
