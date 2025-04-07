
/*Integrantes:
  Cely Juliana
  Jiménez Juliana
  Mora Zharick

Objetivo:
En un proyecto de telemetría se tiene un caudalímetro que envía por puerto
serial cada periodo de tiempo la cantidad de caudal detecta, el cual es un
número entero no mayor de 2 dígitos (es decir, se enviará un número entre
00 y 99)
Se recibirá el número por puerto serial, se debe imprimir por serial la
siguiente información:
1. El número mínimo recibido.
2. El número mayor recibido.
3. El último número recibido.
4. El promedio de todos los números recibidos.
Si se recibe algo diferente entre 00 y 99 (sea número o letra) se debe
ignorar.*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

// Configuraciones y constantes
#define UART_PORT UART_NUM_0  // Usamos el puerto UART0
#define BUF_SIZE 128           // Tamaño del buffer de recepción
#define MAX_NUM 99             // Máximo valor permitido
#define MIN_NUM 0              // Mínimo valor permitido

// Variables globales para llevar estadísticas
static int min_num = MAX_NUM;
static int max_num = MIN_NUM;
static int last_num = 0;
static int total = 0;
static int count = 0;

// ----------------------------------------------------
// Función que actualiza estadísticas con el nuevo número recibido
// ----------------------------------------------------
void process_number(int num) {
    last_num = num;

    if (num < min_num) min_num = num;
    if (num > max_num) max_num = num;

    total += num;
    count++;

    float average = (float)total / count;

    // Imprimir todos los valores relevantes por UART
    printf("Último: %d. Mínimo: %d. Máximo: %d. Promedio: %.2f.\n",
           last_num, min_num, max_num, average);
}

// ----------------------------------------------------
// Función que valida si la cadena ingresada es un número válido
// ----------------------------------------------------
int validate_number(const char *str) {
    size_t len = strlen(str);

    // Verificar longitud (debe tener 1 o 2 caracteres)
    if (len == 0 || len > 2) {
        printf("Error: Debe ser 1-2 dígitos (ingresó %zu caracteres)\n", len);
        return -1;
    }

    // Verificar que todos los caracteres sean dígitos
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)str[i])) {
            printf("Error: Solo se permiten números (carácter inválido: '%c')\n", str[i]);
            return -1;
        }
    }

    // Convertir a entero y validar rango
    int num = atoi(str);
    if (num < MIN_NUM || num > MAX_NUM) {
        printf("Error: El número debe estar entre %d y %d\n", MIN_NUM, MAX_NUM);
        return -1;
    }

    return num;
}

// ----------------------------------------------------
// Configura UART con parámetros estándar (115200 baudios, 8N1)
// ----------------------------------------------------
void init_uart() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(UART_PORT, &uart_config);               // Aplica configuración
    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0); // Instala el driver de UART
}

// ----------------------------------------------------
// Tarea principal que lee continuamente desde UART
// ----------------------------------------------------
void uart_read_task(void *arg) {
    char line[BUF_SIZE];  // Buffer donde se almacenan los caracteres recibidos
    int index = 0;        // Índice actual dentro del buffer

    while (1) {
        uint8_t byte;  // Variable para leer byte por byte
        // Leer un byte desde UART (espera hasta 100 ms)
        int len = uart_read_bytes(UART_PORT, &byte, 1, pdMS_TO_TICKS(100));

        if (len > 0) {
            // Si el carácter es Enter (\n o \r), procesamos el número recibido
            if (byte == '\n' || byte == '\r') {
                if (index > 0) {
                    line[index] = '\0';  // Finaliza la cadena con null terminator
                    printf("Procesando: '%s'\n", line);  // Debug

                    int num = validate_number(line);     // Validar el número
                    if (num != -1) {
                        process_number(num);            // Si es válido, procesarlo
                    }
                    index = 0;  // Reiniciar buffer para la próxima entrada
                }
            } else if (index < BUF_SIZE - 1) {
                // Si no es Enter, almacenar el byte en el buffer
                line[index++] = byte;
            }
        }
    }
}

// ----------------------------------------------------
// Función principal del programa (punto de entrada)
// ----------------------------------------------------
void app_main() {
    init_uart();  // Configurar UART al iniciar

    // Mostrar instrucciones al usuario por consola
    printf("\n=== Sistema de Telemetría ===\n");
    printf("Instrucciones:\n");
    printf("1. Ingrese números entre %d y %d\n", MIN_NUM, MAX_NUM);
    printf("2. Presione Enter después de cada número\n");
    printf("3. Ejemplos válidos: 5, 05, 99\n");
    printf("4. Ejemplos inválidos: 100, abc, -1\n\n");

    // Crear la tarea de lectura por UART (bucle principal)
    xTaskCreate(uart_read_task, "uart_reader", 4096, NULL, 10, NULL);
}