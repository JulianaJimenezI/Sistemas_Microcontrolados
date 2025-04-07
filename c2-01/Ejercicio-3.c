/*Integrantes:
  Cely Juliana
  Jiménez Juliana
  Mora Zharick

1. Desarrolla un sistema de autenticación basado en un patrón táctil.
2. El usuario debe tocar la secuencia en un pin táctil:
   (a) 3 toques largos.
   (b) 3 toques cortos.
   (c) 3 toques largos.
3. Luego de hacer esa secuencia, se debe tocar otro pin táctil para validar la secuencia.
4. Imprimir por serial "APROBADO" o "NO APROBADO" si la secuencia ingresada es correcta o no.
5. Por grupo definir el tiempo a su criterio para determinar que es "toque largo" y por "toque corto".*/

// Librerías estándar y específicas del ESP32
#include <stdio.h>                  // Para funciones estándar de entrada/salida (como printf)
#include <inttypes.h>               // Para tipos de enteros con tamaño fijo (ej: uint8_t, uint32_t)
#include "freertos/FreeRTOS.h"      // Base del sistema operativo en tiempo real FreeRTOS
#include "freertos/task.h"          // Para manejo de tareas en FreeRTOS
#include "driver/touch_pad.h"       // Para configurar y usar el sistema de pads táctiles del ESP32
#include "esp_log.h"                // Para mostrar mensajes en consola con etiquetas y niveles

// ----------------------
// Configuración de pines táctiles
// ----------------------
#define TOUCH_PAD_SEQUENCE TOUCH_PAD_NUM8  // Pad táctil GPIO 33, usado para ingresar la secuencia
#define TOUCH_PAD_VALIDATE TOUCH_PAD_NUM7  // Pad táctil GPIO 27, usado para validar la secuencia ingresada

// ----------------------
// Definición de tiempos límite en milisegundos
// ----------------------
#define SHORT_TOUCH_MAX     2000   // Si el toque dura menos de 2 segundos, se considera "corto"
#define LONG_TOUCH_MIN      3000   // Si el toque dura 3 segundos o más, se considera "largo"
#define MAX_BETWEEN_TOUCHES 10000  // Tiempo máximo permitido entre toques (10 segundos). Si se excede, se reinicia la secuencia.
#define VALIDATION_TIMEOUT  15000  // Tiempo máximo para validar la secuencia después de ingresarla (15 segundos)

// ----------------------
// Secuencia correcta esperada (3 largos, 3 cortos, 3 largos → 1=largo, 0=corto)
// ----------------------
static const uint8_t expected_sequence[9] = {1, 1, 1, 0, 0, 0, 1, 1, 1};

// ----------------------
// Variables de estado del sistema
// ----------------------
static uint8_t entered_sequence[9];     // Arreglo para guardar los toques del usuario (0 o 1)
static uint8_t sequence_index = 0;      // Índice actual dentro de la secuencia
static uint32_t last_touch_time = 0;    // Marca de tiempo del último toque válido
static bool waiting_validation = false; // Bandera: indica si ya se completó la secuencia y se está esperando validación
static bool current_touch = false;      // Bandera: indica si el dedo está actualmente tocando el pad
static uint32_t touch_start_time = 0;   // Momento en que inició el toque actual
static uint16_t baseline_seq = 0;       // Valor de referencia para el pad táctil de entrada
static uint16_t baseline_val = 0;       // Valor de referencia para el pad táctil de validación

static const char *TAG = "TouchAuth";   // Etiqueta para los mensajes del sistema de autenticación táctil

// ----------------------
// Prototipos de funciones
// ----------------------
void reset_sequence();                        // Reinicia todo el estado de la secuencia
void validate_sequence();                     // Compara la secuencia ingresada con la esperada
void calibrate_touch_pad(touch_pad_t, uint16_t *); // Calibra un pad táctil, ajustando su umbral de activación
void init_touch_system();                     // Inicializa los pads táctiles y el sistema general

// ----------------------
// Calibración de un pad táctil
// ----------------------
void calibrate_touch_pad(touch_pad_t touch_num, uint16_t *baseline) {
    uint16_t touch_value;
    touch_pad_read(touch_num, &touch_value);              // Lee el valor actual sin filtrar del pad táctil
    *baseline = touch_value;                              // Guarda ese valor como línea base (sin tocar)
    touch_pad_set_thresh(touch_num, touch_value * 0.5);   // Establece un umbral de detección al 50% del valor base
    ESP_LOGI(TAG, "Pad %d calibrado - Base: %d, Umbral: %d", 
             touch_num, touch_value, (uint16_t)(touch_value * 0.5));
}

// ----------------------
// Inicializa el sistema táctil completo
// ----------------------
void init_touch_system() {
    touch_pad_init();                             // Inicializa el sistema de pads táctiles del ESP32
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER); // Usa el temporizador interno para muestreo automático
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V); // Ajuste de voltajes internos

    touch_pad_config(TOUCH_PAD_SEQUENCE, 0);      // Configura el pad táctil de ingreso (GPIO33)
    touch_pad_config(TOUCH_PAD_VALIDATE, 0);      // Configura el pad táctil de validación (GPIO27)

    touch_pad_filter_start(30);                   // Aplica un filtro digital para estabilidad
    vTaskDelay(200 / portTICK_PERIOD_MS);         // Espera breve para asegurar que los pads estén listos

    calibrate_touch_pad(TOUCH_PAD_SEQUENCE, &baseline_seq); // Calibra pad de ingreso
    calibrate_touch_pad(TOUCH_PAD_VALIDATE, &baseline_val); // Calibra pad de validación

    // Mensajes de inicio del sistema
    ESP_LOGI(TAG, "-------------------------------------------");
    ESP_LOGI(TAG, " SISTEMA DE AUTENTICACIÓN DE UN PATRÓN TÁCTIL");
    ESP_LOGI(TAG, "-------------------------------------------");
    ESP_LOGI(TAG, "Configuración de tiempos:");
    ESP_LOGI(TAG, "- Toque corto: <2 segundos");
    ESP_LOGI(TAG, "- Toque largo: ≥3 segundos");
    ESP_LOGI(TAG, "- Máximo entre toques: 10 segundos");
    ESP_LOGI(TAG, "- Tiempo para validar: 15 segundos");
    ESP_LOGI(TAG, "Instrucciones:");
    ESP_LOGI(TAG, "1. Toque GPIO33 (D33 - Touch8)");
    ESP_LOGI(TAG, "   Secuencia: 3 largos, 3 cortos, 3 largos");
    ESP_LOGI(TAG, "2. Luego toque GPIO27 (D27 - Touch7) para validar");
    ESP_LOGI(TAG, "====================================\n");

    reset_sequence(); // Inicia en estado limpio
}

// ----------------------
// Reinicia los valores para comenzar una nueva secuencia
// ----------------------
void reset_sequence() {
    sequence_index = 0;             // Reinicia el índice de toques
    waiting_validation = false;     // Se sale del modo de validación
    last_touch_time = 0;            // Borra el último tiempo registrado
    ESP_LOGI(TAG, "Esperando secuencia en el pin táctil...");
}

// ----------------------
// Verifica si la secuencia ingresada es válida
// ----------------------
void validate_sequence() {
    bool approved = true; // Bandera para saber si la secuencia fue correcta

    // Compara uno por uno los valores de la secuencia ingresada con la esperada
    for(int i = 0; i < 9; i++) {
        if(entered_sequence[i] != expected_sequence[i]) {
            approved = false;
            break;
        }
    }

    // Muestra la secuencia ingresada con detalle
    ESP_LOGI(TAG, "\n=== RESULTADO ===");
    for(int i = 0; i < 9; i++) {
        ESP_LOGI(TAG, "Toque %d: %s %s", i+1,
                 entered_sequence[i] ? "LARGO" : "CORTO",
                 entered_sequence[i] == expected_sequence[i] ? "[OK]" : "[X]");
    }

    // Resultado final
    if(approved) {
        ESP_LOGI(TAG, "APROBADO");
    } else {
        ESP_LOGI(TAG, "NO APROBADO");
    }
    ESP_LOGI(TAG, "==================\n");

    reset_sequence(); // Vuelve a estado inicial tras validación
}

// ----------------------
// Tarea principal que corre en segundo plano en FreeRTOS
// ----------------------
void touch_auth_task(void *pvParameter) {
    uint16_t touch_seq_value, touch_val_value;

    init_touch_system(); // Inicializa todo el sistema táctil

    while(1) {
        // Lee valores filtrados actuales de ambos pads
        touch_pad_read_filtered(TOUCH_PAD_SEQUENCE, &touch_seq_value);
        touch_pad_read_filtered(TOUCH_PAD_VALIDATE, &touch_val_value);

        // Tiempo actual en milisegundos desde el inicio
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // ----------------------
        // DETECCIÓN DE TOQUE EN EL PAD DE INGRESO
        // ----------------------
        if(touch_seq_value < baseline_seq * 0.5) {
            // Se ha detectado un nuevo toque
            if(!current_touch) {
                current_touch = true;
                touch_start_time = current_time; // Guarda cuándo comenzó el toque
            }
        } else {
            // Se soltó el toque (finaliza un toque)
            if(current_touch) {
                current_touch = false;
                uint32_t touch_duration = current_time - touch_start_time; // Calcula duración

                if(!waiting_validation && sequence_index < 9) {
                    // Determina tipo de toque (largo o corto)
                    uint8_t touch_type = (touch_duration >= LONG_TOUCH_MIN) ? 1 : 0;
                    entered_sequence[sequence_index++] = touch_type;

                    ESP_LOGI(TAG, "Toque %d/%d: %s (%.1f segundos)",
                             sequence_index, 9,
                             touch_type ? "TOQUE LARGO" : "TOQUE CORTO",
                             touch_duration/1000.0);

                    last_touch_time = current_time;

                    // Si ya se ingresaron los 9 toques
                    if(sequence_index >= 9) {
                        waiting_validation = true;
                        ESP_LOGI(TAG, "\n SECUENCIA COMPLETADA");
                        ESP_LOGI(TAG, "Toque GPIO27 (D27-Touch7) para validar la secuencia (tiene 15 segundos)");
                    }
                }
            }
        }

        // ----------------------
        // PROCESO DE VALIDACIÓN
        // ----------------------
        if(waiting_validation) {
            // Error: el usuario volvió a tocar el pad de ingreso en vez del de validación
            if(touch_seq_value < baseline_seq * 0.5) {
                ESP_LOGW(TAG, "Error: Toque el pin GPIO27 (D27-Touch7) para validar, no GPIO33 (D33-Touch8)");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            // Validación correcta
            else if(touch_val_value < baseline_val * 0.5) {
                validate_sequence();
            }
            // Si pasó el tiempo de espera de validación
            else if((current_time - last_touch_time) > VALIDATION_TIMEOUT) {
                ESP_LOGW(TAG, "Tiempo de validación agotado (15 segundos)");
                reset_sequence();
            }
        }
        // ----------------------
        // TIEMPO EXCEDIDO ENTRE TOQUES
        // ----------------------
        else if(sequence_index > 0 && (current_time - last_touch_time) > MAX_BETWEEN_TOUCHES) {
            ESP_LOGW(TAG, "El tiempo entre toques se ha excedido (10 segundos)");
            reset_sequence();
        }

        vTaskDelay(50 / portTICK_PERIOD_MS); // Espera 50 ms antes de revisar de nuevo (reduce carga de CPU)
    }
}

// ----------------------
// Punto de entrada del programa
// ----------------------
void app_main() {
    // Crea la tarea que manejará la lógica del sistema táctil
    xTaskCreate(&touch_auth_task, "touch_auth_task", 4096, NULL, 5, NULL);
}
