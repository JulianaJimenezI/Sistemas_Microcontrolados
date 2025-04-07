/*Integrantes:
  Cely Juliana
  Jiménez Juliana
  Mora Zharick

Objetivo:
  Describir una solución para el microcontrolador que calcule el cuadrado
  de un número recibido por puerto serial e imprima el resultado por
  serial.
  Si se recibe algo diferente a un número entero se debe ignorar.*/

  
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#define UART_PORT UART_NUM_0
#define BUF_SIZE (1024)

// Configuración del UART
static void uart_init(int BAUD_RATE){
  // Definir la estructura de configuración del UART
  uart_config_t uart_config = {
    .baud_rate  = BAUD_RATE,                // Velocidad de transmisión en baudios
    .data_bits  = UART_DATA_8_BITS,         // Número de bits de datos por paquete de transmisión
    .parity     = UART_PARITY_DISABLE,      // Configuración de paridad (desactivado)
    .stop_bits  = UART_STOP_BITS_1,         // Número de bits de parada por paquete de transmisión
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, // Control de flujo del hardware (desactivado)
    .source_clk = UART_SCLK_DEFAULT,        // Configuración de la fuente de reloj (predeterminado)
  };
  // Configuración de los parámetros del UART
  ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

  // Configuración de los pines físicos de UART. Indicar los pines Tx, Rx, RTS y CTS.
  // RX: pin de recepción de datos desde otro dispositivo a través del UART.
  // TX: pin de transmisión de datos a otro dispositivo a través del UART.
  // RTS (Request to Send) y CTS (Clear to Send): pines OPCIONALES para controlar el flujo de datos en la comunicación UART.
  // Evitan la pérdida de datos debido a la sobrecarga de datos en el buffer.
  ESP_ERROR_CHECK(uart_set_pin(UART_PORT, 1, 3, 22, 19));

  // Instalación del controlador del UART
  ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, ESP_INTR_FLAG_IRAM));
}

void replicar_string() 
{
  char data[BUF_SIZE + 1] = {0};  // +1 para el carácter nulo
  int len = uart_read_bytes(UART_PORT, (uint8_t*)data, BUF_SIZE, pdMS_TO_TICKS(20));

  if (len > 0) 
  {
      data[len] = '\0';  // Asegurar terminación de string
      
      // Primero verificar si el dato se puede convertir a entero y si es mayor a 0
      // Si es así, entonces buscar el cuadrado usando los primero N números impares
      // Sino, solo enviar un mensaje de "Entrada inválida"
      int numero;
      if (sscanf(data, "%d", &numero) == 1 && numero > 0) 
      {
          
          // Calcular suma de primeros 'n' impares (n²)
          int sumatoria = 0;
          for(int i = 0, impar = 1; i < numero; ++i, impar += 2) 
          {
              sumatoria += impar;
          }
          
          // Convertir resultado a string
          char resultado[20];

          // Determina la longitud del mensaje a enviar
          int largo = snprintf(resultado, sizeof(resultado), "%d\n", sumatoria);
          
          // Enviar respuesta
          uart_write_bytes(UART_PORT, resultado, largo);
        } 
    }
}

void app_main() {
  // Iniciar el puerto serial.
  // La tasa de baudios se pasa como argumento.
  uart_init(9600);

  // Mostrar mensaje por serial (OPCIONAL)
  printf("Iniciando...\n");

  // En loop, de lo que reciba verificar si es un entero positivo.
  // Si lo es, cualcular el cuadrado
  // Si no, no hacer nada
  while(1) {
    replicar_string();
  }
}