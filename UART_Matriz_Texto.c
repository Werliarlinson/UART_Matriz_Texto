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


#define I2C_PORT i2c1
#define I2C_SDA 14                          // Pinos GPIO para comunicação I2C
#define I2C_SCL 15                          // Pinos GPIO para comunicação I2C
#define endereco 0x3C                       // Endereço I2C do display OLED
#define LED_PIN 7                           // Pino GPIO conectado a matriz de LEDs
#define LED_COUNT 25                        // Número de LEDs na matriz


const uint LED_VERDE = 11;                  // Define o pino GPIO 11 para controlar o LED.
const uint LED_AZUL = 12;                   // Define o pino GPIO 12 para controlar o LED.
const uint LED_VERMELHO = 13;               // Define o pino GPIO 13 para controlar o LED.
const uint button_A = 5;                    // Define o pino GPIO 5 para ler o estado do botão.
const uint button_B = 6;                    // GPIO do botão B


bool led_on = false;                        // Variável global para armazenar o estado do LED (não utilizada neste código).
bool led_active = false;                    // Indica se o LED está atualmente aceso (para evitar múltiplas ativações).
absolute_time_t turn_off_time;              // Variável para armazenar o tempo em que o LED deve ser desligado (não utilizada neste código).
static volatile uint32_t last_time = 0;     // Armazena o tempo do último evento (em microssegundos)
int led_state = 0;                          // Armazena o estado atual do LED
uint32_t elapsed_time = 0;                  // Armazena o tempo decorrido em segundos
int tempo = 0;                              // Armazena o tempo programado para desligar o LED
static int32_t set_button = 0;              // Controlador de seleção das animações



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
    leds[index].R = r;                                    // Definir componente vermelho
    leds[index].G = g;                                    // Definir componente verde
    leds[index].B = b;                                    // Definir componente azul
}

// Função para limpar (apagar) todos os LEDs
void npClear() 
{
    for (uint i = 0; i < LED_COUNT; ++i)                  // Iterar sobre todos os LEDs
        npSetLED(i, 0, 0, 0);                             // Definir cor como preta (apagado)
}

// Função para atualizar os LEDs no hardware
void npWrite() 
{
    for (uint i = 0; i < LED_COUNT; ++i)                      // Iterar sobre todos os LEDs
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G<<24);       // Enviar componente verde
        pio_sm_put_blocking(np_pio, sm, leds[i].R<<24);       // Enviar componente vermelho
        pio_sm_put_blocking(np_pio, sm, leds[i].B<<24);       // Enviar componente azul
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

// Função de callback para desligar o LED após o tempo programado.
int64_t turn_off_callback(alarm_id_t id, void *user_data) {

    // Desliga o LED com base no estado atual.
    switch (led_state)
    {
    case 2:
        gpio_put(LED_VERDE, false);
        led_state--;
        printf("LED Verde desligado\n");
        break;
    case 1:
        gpio_put(LED_AZUL, false);
        led_state--;
        printf("LED Azul desligado\n");
        break;
    case 0:
        gpio_put(LED_VERMELHO, false);
        led_active = false;
        printf("LED Vermelho desligado\n");
        printf("Fim do processo de contagem\n");
        return 0;
    }

    tempo = 0;       // Reseta o tempo decorrido
    // Agenda um novo alarme para desligar o próximo LED após 3 segundo (3000 ms).
    add_alarm_in_ms(3000, turn_off_callback, NULL, false);
    // Retorna 0 para indicar que o alarme não deve se repetir.
    return 0;
}

// Função para lidar com a interrupção dos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    
    uint32_t current_time = to_us_since_boot(get_absolute_time());      // Obter o tempo atual em microssegundos

    if(current_time - last_time > 300000) {                             // Ignorar eventos muito próximos
       
        last_time = current_time;
        
        if (gpio == button_B) {                                         // Avança para a próxima combinação
            set_button++;
            if(set_button > 9){
                set_button = 9;
            } 
        } 
        if (gpio == button_A) {                                         // Retrocede para a combinação anterior
            set_button--;
            if(set_button < 0){
                set_button = 0;
            }
        }
    
    }

}

int main() {
    // Inicializa a comunicação serial para permitir o uso de printf.
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

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

    npInit(LED_PIN);                                      // Inicializar os LEDs
    npClear();                                            // Apagar todos os LEDs
    npWrite();                                            // Atualizar os LEDs no hardware

    bool cor = true;

    // Loop principal do programa que verifica continuamente o estado do botão.
    while (true) {
        
        uint32_t current_time = to_us_since_boot(get_absolute_time());      // Obter o tempo atual em microssegundos

        cor = !cor;
        // Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
        ssd1306_draw_string(&ssd, "Cepedi   TIC37", 8, 10); // Desenha uma string
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 30); // Desenha uma string
        ssd1306_draw_string(&ssd, "Prof Wilton", 15, 48); // Desenha uma string      
        ssd1306_send_data(&ssd); // Atualiza o display

        // Verifica se o botão foi pressionado (nível baixo no pino) e se o LED não está ativo.
        if (gpio_get(button_A) == 0 && !led_active) {
            
            if(current_time - last_time > 300000) {                             // Evento para ignorar o debounce do botão
       
                last_time = current_time;
                elapsed_time = current_time;

                // Liga os LEDs configurando os pinos para nível alto.
                gpio_put(LED_AZUL, true);
                gpio_put(LED_VERDE, true);
                gpio_put(LED_VERMELHO, true);

                // Define 'led_active' como true para indicar que o LED está aceso.
                led_active = true;
                led_state = 2;
                tempo = 0;
                // Agenda um alarme para desligar o LED após 3 segundos (3000 ms).
                // A função 'turn_off_callback' será chamada após esse tempo.
                add_alarm_in_ms(3000, turn_off_callback, NULL, false);
                printf("Botão pressionado\n");
                printf("Começa o processo de contagem\n");
                //Implementação do temporizador dentro da função pois ele só iria reagir no proximo segundo apenas
                tempo++;
                printf("Tempo decorrido: %d segundos\n", tempo);
            }

        }
        
        if (current_time - elapsed_time >= 1000000 && led_active) {             // Evento para contar o tempo decorrido
            elapsed_time += 1000000;                                            //Atualiza o tempo do último segundo contado
            tempo++;
            printf("Tempo decorrido: %d segundos\n", tempo);
        }
        // Introduz uma pequena pausa de 10 ms para reduzir o uso da CPU.
        // Isso evita que o loop seja executado muito rapidamente e consuma recursos desnecessários.
        sleep_ms(10);
    }

    // Retorno de 0, que nunca será alcançado devido ao loop infinito.
    // Isso é apenas uma boa prática em programas com um ponto de entrada main().
    return 0;
}
