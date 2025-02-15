#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h"
#include "inc/ssd1306_i2c.h"
#include "inc/ssd1306.h"

// Biblioteca NeoPixel
#include "inc/neopixel.c"

// =====================
//      DEFINIÇÕES
// =====================
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

#define LED_COUNT 25
#define LED_PIN2 7       // Pino de dados NeoPixel
#define LED_PIN 12
#define BUTTON1_PIN 5
#define BUTTON2_PIN 6
#define WIFI_SSID "AGUIA 2.4"
#define WIFI_PASS "Leticia150789"

// Mensagens
char button1_message[50] = "Nenhum evento no botão 1";
char button2_message[50] = "Nenhum evento no botão 2";
char http_response[1024];

// --- Variáveis para dados remotos (JSON) ---
static float g_temperatura = 0.0f;
static float g_umidade     = 0.0f;
// Flag que indica se já estamos em processo de fetch
static bool  g_fetch_in_progress = false;

// ======================
//   PROTÓTIPOS FUNÇÕES
// ======================
int main(void);

// Display
static void display_lines(const char *lines[], int count);
static void display_ip_address(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);

// HTTP e Botões
void create_http_response(void);
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);
void monitor_buttons(void);

// Funções do “fetch” remoto
bool fetch_remote_data(void); // inicia a conexão/GET
static err_t fetch_connect_cb(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t fetch_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void  fetch_err_cb(void *arg, err_t err);

// Função auxiliar para parse de JSON
static bool parse_json(const char *json, float *temp_out, float *umi_out);

// =====================
//     FUNÇÃO  MAIN
// =====================
int main() {
    // 1) Inicializa stdio / debug
    stdio_init_all();

    // 2) Inicializa I2C p/ display
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // 3) Inicializa display SSD1306
    ssd1306_init();
    struct render_area frame_area = {
        .start_column = 0,
        .end_column   = ssd1306_width - 1,
        .start_page   = 0,
        .end_page     = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    // 4) Limpa tela e exibe "Inicializando"
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, sizeof(ssd));
    render_on_display(ssd, &frame_area);

    const char *inicializando[] = {
        " Inicializando ",
        "   Servidor    ",
        "      Web      "
    };
    display_lines(inicializando, 3);

    // 5) Inicializa Wi-Fi
    if (cyw43_arch_init()) {
        const char *erro_init[] = {
            " ERRO INICIAL  ",
            "    WIFI       "
        };
        display_lines(erro_init, 2);
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    // 6) Conecta ao Wi-Fi
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000) != 0) {
        const char *erro_conexao[] = {
            " ERRO CONEXAO  ",
            "    WIFI       "
        };
        display_lines(erro_conexao, 2);
        return 1;
    } else {
        // Exibir IP caso conectado
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        display_ip_address(ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    // 7) Inicializa NeoPixel
    npInit(LED_PIN2, LED_COUNT);
    npClear();
    npWrite();

    // 8) Configura LED e botões
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    // 9) Inicia servidor HTTP
    start_http_server();

    // 10) Loop principal com atualização automática a cada 5 segundos
    uint64_t last_fetch_ms = 0;

    while (true) {
        cyw43_arch_poll();
        monitor_buttons();

        // Se já se passaram 5 segundos, inicia fetch
        uint64_t now_ms = to_ms_since_boot(get_absolute_time());
        if (!g_fetch_in_progress && (now_ms - last_fetch_ms >= 5000)) {
            // Buscar dados se não estiver no meio de outro fetch
            bool ok = fetch_remote_data();
            if (ok) {
                printf("Auto-fetch remoto...\n");
            }
            last_fetch_ms = now_ms;
        }

        sleep_ms(100);
    }

    cyw43_arch_deinit();
    return 0;
}

// ~~~~~~~~~~~~~~~~~~~~~
//  Funções do DISPLAY
// ~~~~~~~~~~~~~~~~~~~~~
static void display_lines(const char *lines[], int count) {
    struct render_area frame_area = {
        .start_column = 0,
        .end_column   = ssd1306_width - 1,
        .start_page   = 0,
        .end_page     = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, sizeof(ssd));

    int y = 0;
    for (int i = 0; i < count; i++) {
        ssd1306_draw_string(ssd, 5, y, lines[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
    sleep_ms(2000);
}

static void display_ip_address(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3) {
    char ip_str[20];
    sprintf(ip_str, "%d.%d.%d.%d", ip0, ip1, ip2, ip3);

    const char *ip_lines[] = {
        " Conectado IP  ",
        ip_str
    };
    display_lines(ip_lines, 2);
}

// ~~~~~~~~~~~~~~~~~~~~~
//  HTTP / Botões
// ~~~~~~~~~~~~~~~~~~~~~
void create_http_response(void) {
    // Exibimos temperatura/umidade junto com os botões
    snprintf(http_response, sizeof(http_response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<meta charset=\"UTF-8\">"
             "<title>Controle do LED e Botões</title>"
             "</head>"
             "<body>"
             "  <h1>Controle do LED e Botões</h1>"
             "  <p><a href=\"/led/on\">Ligar LED</a></p>"
             "  <p><a href=\"/led/off\">Desligar LED</a></p>"
             "  <p><a href=\"/update\">Atualizar Dados Remotos</a></p>"
             "  <h2>Estado dos Botões:</h2>"
             "  <p>Botão 1: %s</p>"
             "  <p>Botão 2: %s</p>"

             "  <h2>Dados Remotos:</h2>"
             "  <p>Temperatura: %.2f °C</p>"
             "  <p>Umidade: %.2f %%</p>"

             "</body>"
             "</html>\r\n",
             button1_message, button2_message,
             g_temperatura, g_umidade);
}

static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }
    char *request = (char *)p->payload;

    if (strstr(request, "GET /led/on")) {
        gpio_put(LED_PIN, 1);
        npClear();
        // Exemplo: ligar alguns LEDs (verde)
        npSetLED(2, 0, 255, 0);
        npSetLED(6, 0, 255, 0);
        npSetLED(8, 0, 255, 0);
        npSetLED(10, 0, 255, 0);
        npSetLED(11, 0, 255, 0);
        npSetLED(14, 0, 255, 0);
        npSetLED(15, 0, 255, 0);
        npSetLED(17, 0, 255, 0);
        npSetLED(19, 0, 255, 0);
        npSetLED(22, 0, 255, 0);
        npWrite();
    }
    else if (strstr(request, "GET /led/off")) {
        gpio_put(LED_PIN, 0);
        npClear();
        npWrite();
    }
    else if (strstr(request, "GET /update")) {
        // Inicia a busca de dados se não estiver em progresso
        if (!g_fetch_in_progress) {
            bool ok = fetch_remote_data();
            if (ok) {
                printf("Fetch remoto via /update...\n");
            } else {
                printf("Falha ao iniciar fetch.\n");
            }
        }
    }

    create_http_response();
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

void monitor_buttons(void) {
    static bool button1_last_state = false;
    static bool button2_last_state = false;

    bool button1_state = !gpio_get(BUTTON1_PIN);
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

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  CLIENTE lwIP p/ fetch /dados
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
typedef struct {
    struct tcp_pcb *pcb;
    char buffer[512];
    int  bufpos;
} fetch_state_t;

bool fetch_remote_data(void) {
    if (g_fetch_in_progress) {
        printf("Fetch já está em andamento.\n");
        return false;
    }
    fetch_state_t *fs = (fetch_state_t*)calloc(1, sizeof(fetch_state_t));
    if (!fs) {
        printf("Sem memória p/ fetch_state.\n");
        return false;
    }

    fs->pcb = tcp_new();
    if (!fs->pcb) {
        printf("Erro ao criar pcb fetch.\n");
        free(fs);
        return false;
    }

    // Prepara callbacks
    tcp_arg(fs->pcb, fs);
    tcp_err(fs->pcb, fetch_err_cb);

    // IP do servidor remoto
    ip_addr_t remote_ip;
    ip4addr_aton("192.168.15.24", &remote_ip);

    err_t e = tcp_connect(fs->pcb, &remote_ip, 80, fetch_connect_cb);
    if (e != ERR_OK) {
        printf("Erro tcp_connect: %d\n", e);
        tcp_close(fs->pcb);
        free(fs);
        return false;
    }

    g_fetch_in_progress = true;
    return true;
}

static err_t fetch_connect_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Conexão fetch falhou.\n");
        return err;
    }

    tcp_recv(tpcb, fetch_recv_cb);

    const char *req = "GET /dados HTTP/1.0\r\nHost: 192.168.15.24\r\n\r\n";
    err_t werr = tcp_write(tpcb, req, strlen(req), TCP_WRITE_FLAG_COPY);
    if (werr != ERR_OK) {
        printf("Erro ao enviar GET: %d\n", werr);
        return werr;
    }
    tcp_output(tpcb);

    return ERR_OK;
}

static err_t fetch_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    fetch_state_t *fs = (fetch_state_t*)arg;
    if (!p) {
        printf("Fim da resposta, parse do buffer...\n");
        char *json_start = strstr(fs->buffer, "{");
        if (json_start) {
            float t=0, u=0;
            if (parse_json(json_start, &t, &u)) {
                g_temperatura = t;
                g_umidade     = u;
                printf("Dados ok: Temp=%.2f / Umid=%.2f\n", t, u);
            } else {
                printf("Falha parse JSON.\n");
            }
        } else {
            printf("Não encontrei '{' no buffer.\n");
        }
        tcp_close(fs->pcb);
        free(fs);
        g_fetch_in_progress = false;
        return ERR_OK;
    }

    // Copia para buffer
    struct pbuf *q = p;
    while (q && (fs->bufpos + q->len < (int)sizeof(fs->buffer))) {
        memcpy(fs->buffer + fs->bufpos, q->payload, q->len);
        fs->bufpos += q->len;
        q = q->next;
    }
    pbuf_free(p);

    return ERR_OK;
}

static void fetch_err_cb(void *arg, err_t err) {
    fetch_state_t *fs = (fetch_state_t*)arg;
    printf("fetch_err_cb: erro=%d\n", err);
    if (fs) {
        if (fs->pcb) {
            tcp_close(fs->pcb);
        }
        free(fs);
    }
    g_fetch_in_progress = false;
}

static bool parse_json(const char *json, float *temp_out, float *umi_out) {
    char *temp_ptr = strstr(json, "temperatura");
    char *umi_ptr  = strstr(json, "umidade");
    if (!temp_ptr || !umi_ptr) {
        return false;
    }

    temp_ptr = strchr(temp_ptr, ':');
    umi_ptr  = strchr(umi_ptr, ':');
    if (!temp_ptr || !umi_ptr) {
        return false;
    }

    temp_ptr++;
    umi_ptr++;

    float t, u;
    if (sscanf(temp_ptr, "%f", &t) != 1 ||
        sscanf(umi_ptr,  "%f", &u) != 1) {
        return false;
    }
    *temp_out = t;
    *umi_out  = u;
    return true;
}
