# 🤖 Configuración de Hardware: Neural Chassis

Este documento guarda el estado exacto "dorado" de la configuración física y lógica del coche, para no perder nunca el norte cuando añadamos más módulos (sensores, brazos, luces, etc.).

## ⚙️ Motores (L298N)
Hemos descubierto que el cableado físico de los motores requería invertir la lógica por software para que "Adelante" fuera adelante y no diera trompos.

**Configuración perfecta (Guardada en `NeuralChassis.ino`):**
```cpp
const bool INVERTIR_MOTOR_A = true;  // Lado izquierdo 
const bool INVERTIR_MOTOR_B = false; // Lado derecho 
```
*Si en algún momento el coche vuelve a volverse loco al avanzar, el primer paso será revisar que estas dos variables estén exactamente así.*

## 🎙️ Módulo de Voz (VC-02)
*   **Conexiones:** 
    *   Arduino RX (Pin 4) recibe desde TX del módulo.
    *   Arduino TX (Pin 2) envía al RX del módulo.
*   **Velocidad Serie:** 9600 baudios.
*   **Envío de Comandos:** El módulo envía números (del 1 al 7) según la frase en inglés detectada.

## 📱 App Móvil (HTML + Capacitor)
*   Incluye un control por voz de la propia App que usa `cordova-plugin-speechrecognition`.
*   Para que funcione, hemos insertado un bloque especial `<queries>` en el `AndroidManifest.xml` (para evitar el bloqueo en Android 11+).

## 📡 Matriz Infrarroja (TCRT5000)
Actualmente apunta **al suelo** para seguir la línea. Por este motivo lógico, la función `isObstacleBehind()` la hemos desactivado por defecto en la comprobación (`objBehind = false;`). Si algún día usamos este sensor apuntando al aire para detectar paredes dando marcha atrás, volveremos a activar la comprobación.
