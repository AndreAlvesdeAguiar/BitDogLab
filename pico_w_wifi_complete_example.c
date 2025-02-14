#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>

#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h"


const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

#define LED_COUNT 25
#define LED_PIN2 7

#define LED_PIN 12         // Pino do LED
#define BUTTON1_PIN 5      // Pino do botão 1
#define BUTTON2_PIN 6      // Pino do botão 2
#define WIFI_SSID "AGUIA 2.4"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "Leticia150789" // Substitua pela senha da sua rede Wi-Fi

// Estado dos botões (inicialmente sem mensagens)
char button1_message[50] = "Nenhum evento no botão 1";
char button2_message[50] = "Nenhum evento no botão 2";

// Buffer para resposta HTTP
char http_response[1024];


struct pixel_t {
    uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
  };
  typedef struct pixel_t pixel_t;
  typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.
  
  // Declaração do buffer de pixels que formam a matriz.
  npLED_t leds[LED_COUNT];
  
  // Variáveis para uso da máquina PIO.
  PIO np_pio;
  uint sm;
  
  /**
   * Inicializa a máquina PIO para controle da matriz de LEDs.
   */
  void npInit(uint pin) {
  
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
  
    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
      np_pio = pio1;
      sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }
  
    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
  
    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i) {
      leds[i].R = 0;
      leds[i].G = 0;
      leds[i].B = 0;
    }
  }
  
  /**
   * Atribui uma cor RGB a um LED.
   */
  void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
  }
  
  /**
   * Limpa o buffer de pixels.
   */
  void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
      npSetLED(i, 0, 0, 0);
  }
  
  /**
   * Escreve os dados do buffer nos LEDs.
   */
  void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
      pio_sm_put_blocking(np_pio, sm, leds[i].G);
      pio_sm_put_blocking(np_pio, sm, leds[i].R);
      pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
  }

  
// Função para criar a resposta HTTP
void create_http_response() {
    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "  <meta charset=\"UTF-8\">"
             "  <title>Controle do LED e Botões</title>"
             "</head>"
             "<body>"
             "  <h1>Controle do LED e Botões</h1>"
             "  <p><a href=\"/led/on\">Ligar LED</a></p>"
             "  <p><a href=\"/led/off\">Desligar LED</a></p>"
             "  <p><a href=\"/update\">Atualizar Estado</a></p>"
             "  <h2>Estado dos Botões:</h2>"
             "  <p>Botão 1: %s</p>"
             "  <p>Botão 2: %s</p>"
             "</body>"
             "</html>\r\n",
             button1_message, button2_message);
}


// Função de callback para processar requisições HTTP
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Cliente fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Processa a requisição HTTP
    char *request = (char *)p->payload;

    if (strstr(request, "GET /led/on")) {
        gpio_put(LED_PIN, 1);  // Liga o LED
        printf("Ligando LEDs\n");

        // Topo
        npSetLED(2, 0, 255, 0);
        
        // Segunda linha
        npSetLED(6, 0, 255, 0);
        npSetLED(8, 0, 255, 0);
        
        // Terceira linha
        npSetLED(10, 0, 255, 0);
        npSetLED(11, 0, 255, 0);
        npSetLED(14, 0, 255, 0);
        
        // Quarta linha
        npSetLED(15, 0, 255, 0);
        npSetLED(17, 0, 255, 0);
        npSetLED(19, 0, 255, 0);
        
        // Caule
        npSetLED(22, 0, 255, 0);
        
        npWrite();
     
    } else if (strstr(request, "GET /led/off")) {
        gpio_put(LED_PIN, 0);  // Desliga o LED
        printf("Ligando LEDs\n");
        npSetLED(0, 0, 0, 0);
        npSetLED(1, 0, 0, 0);
        npSetLED(2, 0, 0, 0);
        npSetLED(3, 0, 0, 0);
        npSetLED(4, 0, 0, 0);
        npSetLED(5, 0, 0, 0);
        npSetLED(6, 0, 0, 0);
        npSetLED(7, 0, 0, 0);
        npSetLED(8, 0, 0, 0);
        npSetLED(9, 0, 0, 0);
        npSetLED(10, 0, 0, 0);
        npSetLED(11, 0, 0, 0);
        npSetLED(12, 0, 0, 0);
        npSetLED(13, 0, 0, 0);
        npSetLED(14, 0, 0, 0);
        npSetLED(15, 0, 0, 0);
        npSetLED(16, 0, 0, 0);
        npSetLED(17, 0, 0, 0);
        npSetLED(18, 0, 0, 0);
        npSetLED(19, 0, 0, 0);
        npSetLED(20, 0, 0, 0);
        npSetLED(21, 0, 0, 0);
        npSetLED(22, 0, 0, 0);
        npSetLED(23, 0, 0, 0);
        npSetLED(24, 0, 0, 0);
        npWrite();
    }

    // Atualiza o conteúdo da página com base no estado dos botões
    create_http_response();

    // Envia a resposta HTTP
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);  // Associa o callback HTTP
    return ERR_OK;
}

// Função de setup do servidor TCP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    // Liga o servidor na porta 80
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);  // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback);  // Associa o callback de conexão

    printf("Servidor HTTP rodando na porta 80...\n");
}

// Função para monitorar o estado dos botões
void monitor_buttons() {
    static bool button1_last_state = false;
    static bool button2_last_state = false;

    bool button1_state = !gpio_get(BUTTON1_PIN); // Botão pressionado = LOW
    bool button2_state = !gpio_get(BUTTON2_PIN);

    if (button1_state != button1_last_state) {
        button1_last_state = button1_state;
        if (button1_state) {
            snprintf(button1_message, sizeof(button1_message), "Botão 1 foi pressionado!");
            printf("Botão 1 pressionado\n");
        } else {
            snprintf(button1_message, sizeof(button1_message), "Botão 1 foi solto!");
            printf("Botão 1 solto\n");
        }
    }

    if (button2_state != button2_last_state) {
        button2_last_state = button2_state;
        if (button2_state) {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi pressionado!");
            printf("Botão 2 pressionado\n");
        } else {
            snprintf(button2_message, sizeof(button2_message), "Botão 2 foi solto!");
            printf("Botão 2 solto\n");
        }
    }
}

int main() {
    stdio_init_all();  // Inicializa a saída padrão

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    npInit(LED_PIN2);
    npClear();
  
    // Aqui, você desenha nos LEDs.
  
    npWrite(); // Escreve os dados nos LEDs.
    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    char *text[] = {
        "  Bem-vindos!   ",
        "  Embarcatech   ",
        "  Embarcatech   ",
        "  Embarcatech   ",
        "  Embarcatech   "};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);


    sleep_ms(10000);
    printf("Iniciando servidor HTTP\n");

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }else {
        printf("Connected.\n");
        // Read the ip address in a human readable way
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    printf("Wi-Fi conectado!\n");

    // Configura o LED e os botões
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    printf("Botões configurados com pull-up nos pinos %d e %d\n", BUTTON1_PIN, BUTTON2_PIN);

    // Inicia o servidor HTTP
    start_http_server();

    // Loop principal
    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo
        monitor_buttons();  // Atualiza o estado dos botões
        sleep_ms(100);      // Reduz o uso da CPU
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}
