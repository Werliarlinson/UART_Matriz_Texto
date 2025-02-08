#include <stdio.h>                          // Biblioteca padrão do C.
#include <string.h>                         // Biblioteca padrão do C para manipulação de strings.
#include "pico/stdlib.h"                    // Biblioteca padrão do Raspberry Pi Pico para controle de GPIO, temporização e comunicação serial.
#include "pico/time.h"                      // Biblioteca para gerenciamento de temporizadores e alarmes.
#include "hardware/i2c.h"                   // Biblioteca para comunicação I2C.
#include "inc/ssd1306.h"                    // Biblioteca para controle do display OLED SSD1306.
#include "inc/font.h"                       // Biblioteca para uso de fontes personalizadas.
#include "hardware/pio.h"                   // Biblioteca para manipulação de periféricos PIO
#include "ws2818b.pio.h"                    // Programa para controle de LEDs WS2812B
#include "hardware/clocks.h"                // Biblioteca para controle de relógios do hardware
#include "ctype.h"                          // Biblioteca para manipulação de caracteres


#define I2C_PORT i2c1
#define I2C_SDA 14                          // Pinos GPIO para comunicação I2C
#define I2C_SCL 15                          // Pinos GPIO para comunicação I2C
#define endereco 0x3C                       // Endereço I2C do display OLED
#define LED_PIN 7                           // Pino GPIO conectado a matriz de LEDs
#define LED_COUNT 25                        // Número de LEDs na matriz
#define UART_ID uart0                       // Seleciona a UART0
#define BAUD_RATE 115200                    // Define a taxa de transmissão
#define UART_TX_PIN 0                       // Pino GPIO usado para TX
#define UART_RX_PIN 1                       // Pino GPIO usado para RX

const uint LED_VERDE = 11;                  // Define o pino GPIO 11 para controlar a cor verde do LED RGB.
const uint LED_AZUL = 12;                   // Define o pino GPIO 12 para controlar a cor azul do LED RGB.
const uint LED_VERMELHO = 13;               // Define o pino GPIO 13 para controlar a cor vermelha do LED RGB.
const uint button_A = 5;                    // GPIO do botão A.
const uint button_B = 6;                    // GPIO do botão B

static volatile uint32_t last_time = 0;     // Armazena o tempo do último evento (em microssegundos)
static volatile bool flag_button = 0;       // Armazena o estado do botão
uint32_t elapsed_time = 10000;              // Armazena o tempo decorrido em microsegundos (Padrão: 10s)
static int32_t set_button = 0;              // Controlador de seleção das frases
alarm_id_t alarm_id = 0;                    // Variável global para armazenar o ID do alarme

typedef struct pixel_t pixel_t;             // Alias para a estrutura pixel_t
typedef pixel_t npLED_t;                    // Alias para facilitar o uso no contexto de LEDs

struct pixel_t { 
    uint32_t G, R, B;                       // Componentes de cor: Verde, Vermelho e Azul
};

npLED_t leds[LED_COUNT];                    // Array para armazenar o estado de cada LED
PIO np_pio;                                 // Variável para referenciar a instância PIO usada
uint sm;                                    // Variável para armazenar o número do state machine usado

// Função para obter o índice de um LED na matriz
int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24-(y * 5 + x);              // Linha par (esquerda para direita).
    } else {
        return 24-(y * 5 + (4 - x));        // Linha ímpar (direita para esquerda).
    }
}

// Função para inicializar o PIO para controle dos LEDs
void npInit(uint pin) 
{
    uint offset = pio_add_program(pio0, &ws2818b_program);      // Carregar o programa PIO
    np_pio = pio0;                                              // Usar o primeiro bloco PIO

    sm = pio_claim_unused_sm(np_pio, false);                    // Tentar usar uma state machine do pio0
    if (sm < 0)                                                 // Se não houver disponível no pio0
    {
        np_pio = pio1;                                          // Mudar para o pio1
        sm = pio_claim_unused_sm(np_pio, true);                 // Usar uma state machine do pio1
    }

    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);    // Inicializar state machine para LEDs

    for (uint i = 0; i < LED_COUNT; ++i)                        // Inicializar todos os LEDs como apagados
    {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

// Função para definir a cor de um LED específico
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) 
{
    leds[index].R = r;                                          // Definir componente vermelho
    leds[index].G = g;                                          // Definir componente verde
    leds[index].B = b;                                          // Definir componente azul
}

// Função para limpar (apagar) todos os LEDs
void npClear() 
{
    for (uint i = 0; i < LED_COUNT; ++i)                        // Iterar sobre todos os LEDs
        npSetLED(i, 0, 0, 0);                                   // Definir cor como preta (apagado)
}

// Função para atualizar os LEDs no hardware
void npWrite() 
{
    for (uint i = 0; i < LED_COUNT; ++i)                        // Iterar sobre todos os LEDs
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G<<24);         // Enviar componente verde
        pio_sm_put_blocking(np_pio, sm, leds[i].R<<24);         // Enviar componente vermelho
        pio_sm_put_blocking(np_pio, sm, leds[i].B<<24);         // Enviar componente azul
    }
}

// Função para imprimir um frame na matriz de LEDs de maneira padronizada e sem dificuldades
void print_frame(int frame[5][5][3])
{
    for(int linha = 0; linha < 5; linha++){
        for(int coluna = 0; coluna < 5; coluna++){
            int posicao = getIndex(linha, coluna);
            npSetLED(posicao, frame[coluna][linha][0], frame[coluna][linha][1], frame[coluna][linha][2]);
        }
    }
    npWrite();
    
}

// Função para imprimir a animação dos números árabicos
void animation_number_ara(int number){

    //Lista dos numeros arabicos
    int number_1[5][5][3] = { //1
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int number_2[5][5][3] = { //2
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_3[5][5][3] = { //3
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_4[5][5][3] = { //4
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_5[5][5][3] = { //5
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_6[5][5][3] = { //6
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_7[5][5][3] = { //7
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_8[5][5][3] = { //8
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_9[5][5][3] = { //9
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int number_0[5][5][3] = { //0
                {{0, 0, 0}, {55, 0, 0}, {255, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };

    switch (number) {
        case 0: print_frame(number_0); break;
        case 1: print_frame(number_1); break;
        case 2: print_frame(number_2); break;
        case 3: print_frame(number_3); break;
        case 4: print_frame(number_4); break;
        case 5: print_frame(number_5); break;
        case 6: print_frame(number_6); break;
        case 7: print_frame(number_7); break;
        case 8: print_frame(number_8); break;
        case 9: print_frame(number_9); break;
    }          

}

// Função para imprimir a animação das letras maiúsculas
void animation_letter(char letter) {
    // Lista das letras maiúsculas
    int letter_A[5][5][3] = { // A
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_B[5][5][3] = { // B
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_C[5][5][3] = { // C
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_D[5][5][3] = { // D
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_E[5][5][3] = { // E
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_F[5][5][3] = { // F
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_G[5][5][3] = { // G
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_H[5][5][3] = { // H
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_I[5][5][3] = { // I
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_J[5][5][3] = { // J
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_K[5][5][3] = { // K
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_L[5][5][3] = { // L
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_M[5][5][3] = { // M
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {55, 0, 0},},
                {{55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},}
                };
    int letter_N[5][5][3] = { // N
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {55, 0, 0}},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},}
                };
    int letter_O[5][5][3] = { // O
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_P[5][5][3] = { // P
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_Q[5][5][3] = { // Q
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0},}
                };
    int letter_R[5][5][3] = { // R
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_S[5][5][3] = { // S
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_T[5][5][3] = { // T
                {{55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_U[5][5][3] = { // U
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {0, 0, 0},}
                };
    int letter_V[5][5][3] = { // V
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_W[5][5][3] = { // W
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}},
                {{55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}},
                {{55, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {55, 0, 0},}
                };
    int letter_X[5][5][3] = { // X
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}},
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},}
                };
    int letter_Y[5][5][3] = { // Y
                {{55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0},},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0},}
                };
    int letter_Z[5][5][3] = { // Z
                {{55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0},},
                {{0, 0, 0}, {0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{0, 0, 0}, {55, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                {{55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0}, {55, 0, 0},}
                };

    // Seleciona a letra correta com base no caractere recebido
    switch(letter) {
        case 'A': case 'a': print_frame(letter_A); break;
        case 'B': case 'b': print_frame(letter_B); break;
        case 'C': case 'c': print_frame(letter_C); break;
        case 'D': case 'd': print_frame(letter_D); break;
        case 'E': case 'e': print_frame(letter_E); break;
        case 'F': case 'f': print_frame(letter_F); break;
        case 'G': case 'g': print_frame(letter_G); break;
        case 'H': case 'h': print_frame(letter_H); break;
        case 'I': case 'i': print_frame(letter_I); break;
        case 'J': case 'j': print_frame(letter_J); break;
        case 'K': case 'k': print_frame(letter_K); break;
        case 'L': case 'l': print_frame(letter_L); break;
        case 'M': case 'm': print_frame(letter_M); break;
        case 'N': case 'n': print_frame(letter_N); break;
        case 'O': case 'o': print_frame(letter_O); break;
        case 'P': case 'p': print_frame(letter_P); break;
        case 'Q': case 'q': print_frame(letter_Q); break;
        case 'R': case 'r': print_frame(letter_R); break;
        case 'S': case 's': print_frame(letter_S); break;
        case 'T': case 't': print_frame(letter_T); break;
        case 'U': case 'u': print_frame(letter_U); break;
        case 'V': case 'v': print_frame(letter_V); break;
        case 'W': case 'w': print_frame(letter_W); break;
        case 'X': case 'x': print_frame(letter_X); break;
        case 'Y': case 'y': print_frame(letter_Y); break;
        case 'Z': case 'z': print_frame(letter_Z); break;    
    }
}

// Função para lidar com a interrupção dos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    
    flag_button = true;                                                 // Ativa a flag para ignorar o botão

    uint32_t current_time = to_us_since_boot(get_absolute_time());      // Obter o tempo atual em microssegundos

    if(current_time - last_time > 300000) {                             // Ignorar eventos muito próximos
       
        last_time = current_time;
        
        if (gpio == button_B) {                                         // Avança para a próxima combinação
            gpio_put(LED_AZUL, !gpio_get(LED_AZUL));                    // Inverte o estado do LED azul
            set_button = 2;
        } 
        if (gpio == button_A) {                                         // Retrocede para a combinação anterior
            gpio_put(LED_VERDE, !gpio_get(LED_VERDE));                  // Inverte o estado do LED verde
            set_button = 1;
        }
    }
}

// Função de resetar as mensagens já escritas em tela para a configuração padrão.
int64_t turn_off_callback(alarm_id_t id, void *user_data) {
    
    bool cor = true;
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);       // Inicializa o display
    ssd1306_config(&ssd);                                               // Configura o display
    ssd1306_send_data(&ssd);                                            // Envia os dados para o display

    // Sequência de escape ANSI para limpar a tela do terminal
    const char *clear_screen = "\033[2J\033[H";
    uart_puts(UART_ID, clear_screen);

    // Mensagem inicial
    const char *init_message = "Digite algo e veja o que acontece:\r\n";
    uart_puts(UART_ID, init_message);
    ssd1306_fill(&ssd, !cor);                                           // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);                       // Desenha um retângulo
    ssd1306_draw_string(&ssd, "Tarefa \t\t U4C6", 8, 10);               // Desenha uma string
    ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 30);                   // Desenha uma string
    ssd1306_draw_string(&ssd, "Werliarlinson", 14, 48);                 // Desenha uma string      
    ssd1306_send_data(&ssd);                                            // Atualiza o display
    
    // Retorna 0 para indicar que o alarme não deve se repetir.
    return 0;
}

int main() {
    
    // Inicializa a comunicação serial para permitir o uso de printf.
    stdio_init_all();

    uart_init(UART_ID, BAUD_RATE);                                      // Inicializa a UART
    
    i2c_init(I2C_PORT, 400 * 1000);                                     // Inicializa o display OLED

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                          // Seta a função do pino GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                          
    gpio_pull_up(I2C_SDA);                                              // Estabelece o pull-up na linha de dados
    gpio_pull_up(I2C_SCL);                                              // Estabelece o pull-up na linha de clock
    ssd1306_t ssd;                                                      // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);       // Inicializa o display
    ssd1306_config(&ssd);                                               // Configura o display
    ssd1306_send_data(&ssd);                                            // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Configura os pinos GPIO para a UART
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);                     // Configura o pino 0 para TX
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);                     // Configura o pino 1 para RX

    // Configura os pinos para o LED RGB (11, 12 e 13) como saída digital.
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    // Configura o pino dos butões como entrada digital.
    gpio_init(button_A);
    gpio_set_dir(button_A, GPIO_IN);
    gpio_init(button_B);                                  
    gpio_set_dir(button_B, GPIO_IN);
    
    // Habilita o resistor pull-up interno para o pino do botão.
    // Isso garante que o pino seja lido como alto (3,3 V) quando o botão não está pressionado.
    gpio_pull_up(button_A);
    gpio_pull_up(button_B);

    npInit(LED_PIN);                                                    // Inicializar os LEDs
    npClear();                                                          // Apagar todos os LEDs
    npWrite();                                                          // Atualizar os LEDs no hardware

    //Configuração da interrupção do botão A
    gpio_set_irq_enabled_with_callback(button_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);   // Habilitar interrupção no botão A
    //Configuração da interrupção do botão B
    gpio_set_irq_enabled_with_callback(button_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);   // Habilitar interrupção no botão B
    
    bool cor = true;

    add_alarm_in_ms(10, turn_off_callback, NULL, false);                // Aciona para começar a mensagem padrão

    // Loop principal do programa que verifica continuamente o estado do botão.
    while (true) {
        
        cor = !cor;
        
        if(flag_button) {                                                                    // Evento para verificar o botão foi acionado
            flag_button = false;                                                             // Reseta a flag
            // Verifica se o botão foi pressionado (nível baixo no pino) para emissão da mensagem.
            if (set_button == 1) {
                uart_puts(UART_ID,"Estado do LED Verde alterado!\r\n");                      // Imprime uma mensagem no terminal
                ssd1306_fill(&ssd, !cor);                                                    // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);                                // Desenha um retângulo
                ssd1306_draw_string(&ssd, "Estado do LED", 12, 25);                          // Desenha uma string
                ssd1306_draw_string(&ssd, "Verde alterado!", 5, 35);                         // Desenha uma string
                ssd1306_send_data(&ssd);                                                     // Atualiza o display
            }
            if (set_button == 2) {
                uart_puts(UART_ID,"Estado do LED Azul alterado!\r\n");                       // Imprime uma mensagem no terminal
                ssd1306_fill(&ssd, !cor);                                                    // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);                                // Desenha um retângulo
                ssd1306_draw_string(&ssd, "Estado do LED", 12, 25);                          // Desenha uma string
                ssd1306_draw_string(&ssd, "Azul alterado!", 10, 35);                         // Desenha uma string
                ssd1306_send_data(&ssd);                                                     // Atualiza o display 
            }
            set_button = 0;                                                                  // Reseta o valor do botão
            // Reseta o tempo de espera para a mensagem padrão
            cancel_alarm(alarm_id);
            // Timeout atingido, reseta a mensagem padrão
            alarm_id = add_alarm_in_ms(elapsed_time, turn_off_callback, NULL, false);
        }    
        
        // Verifica se há dados disponíveis para leitura
        if (uart_is_readable(UART_ID)) {
            // Lê um caractere da UART
            int c = uart_getc(UART_ID);                                         // Lê um caractere da UART
            // Verifica se o caractere é um número
            if (isdigit(c)) {
                int number = c - '0';                                           // Converte o caractere para um número inteiro
                animation_number_ara(number);                                   // Chama a animação do número
                ssd1306_fill(&ssd, !cor);                                       // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);                   // Desenha um retângulo
                ssd1306_draw_string(&ssd, "Numero ", 20, 30);                   // Desenha uma string
                ssd1306_draw_char(&ssd, c, 100, 30);                            // Desenha o caractere
                ssd1306_send_data(&ssd);                                        // Atualiza o display
            } else if (isalpha(c)) {                                            // Verifica se o caractere é uma letra
                animation_letter(c);                                            // Chama a animação da letra
                ssd1306_fill(&ssd, !cor);                                       // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);                   // Desenha um retângulo
                ssd1306_draw_string(&ssd, "Caractere ", 20, 30);                // Desenha uma string
                ssd1306_draw_char(&ssd, c, 100, 30);                            // Desenha o caractere
                ssd1306_send_data(&ssd);                                        // Atualiza o display
            } else {
                uart_puts(UART_ID, "Caractere não suportado: ");                // Envia uma mensagem de erro
                ssd1306_fill(&ssd, !cor);                                       // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);                   // Desenha um retângulo
                ssd1306_draw_string(&ssd, "Caractere nao", 12, 25);  // Desenha uma string
                ssd1306_draw_string(&ssd, "suportado!", 25, 35);  // Desenha uma string

                ssd1306_send_data(&ssd);                                        // Atualiza o display
            }
            // Envia de volta o caractere lido (eco)
            uart_putc(UART_ID, c);
            // Envia uma mensagem adicional para cada caractere recebido
            uart_puts(UART_ID, " <- Eco do RP2\r\n");
            // Reseta o tempo de espera para a mensagem padrão
            cancel_alarm(alarm_id);
            // Timeout atingido, reseta a mensagem padrão
            alarm_id = add_alarm_in_ms(elapsed_time, turn_off_callback, NULL, false);
        }
        
        // Introduz uma pequena pausa de 10 ms para reduzir o uso da CPU.
        // Isso evita que o loop seja executado muito rapidamente e consuma recursos desnecessários.
        sleep_ms(10);
    }

    // Retorno de 0, que nunca será alcançado devido ao loop infinito.
    // Isso é apenas uma boa prática em programas com um ponto de entrada main().
    return 0;
}
