#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

#define NOME_REDE_WIFI "brisa-580702"
#define SENHA_WIFI "pi1jgg6t"
#define PINO_BOTAO 6

// Função para calcular a direção do joystick
const char* obter_direcao_simples(uint16_t x, uint16_t y) {
    const int centro = 2048;
    const int margem = 700;

    if (x > centro + margem && y > centro + margem) return "Nordeste";
    if (x > centro + margem && y < centro - margem) return "Sudeste";
    if (x < centro - margem && y > centro + margem) return "Noroeste";
    if (x < centro - margem && y < centro - margem) return "Sudoeste";
    if (x > centro + margem) return "Leste";
    if (x < centro - margem) return "Oeste";
    if (y > centro + margem) return "Norte";
    if (y < centro - margem) return "Sul";
    return "Centro";
}

// Processa a requisição HTTP
static err_t servidor_tcp_receber(void *arg, struct tcp_pcb *conexao, struct pbuf *pacote, err_t err) {
    if (!pacote) {
        tcp_close(conexao);
        tcp_recv(conexao, NULL);
        return ERR_OK;
    }

    char *requisicao = (char *)malloc(pacote->len + 1);
    if (!requisicao) {
        pbuf_free(pacote);
        return ERR_MEM;
    }

    memcpy(requisicao, pacote->payload, pacote->len);
    requisicao[pacote->len] = '\0';

    printf("Requisição: %s\n", requisicao);

    // Leitura do joystick
    adc_select_input(0);
    uint16_t x_atual = adc_read();
    adc_select_input(1);
    uint16_t y_atual = adc_read();

    // Calcula direção
    const char* direcao = obter_direcao_simples(x_atual, y_atual);

    // Leitura direta do botão 
    bool pressionado = gpio_get(PINO_BOTAO) == 0;
    const char* estado_botao = pressionado ? "Pressionado" : "Solto";
    printf("Estado do botão: %s\n", estado_botao);

    // HTML para acompanhar os estados do sistema
    char html[1024];
    snprintf(
        html,
        sizeof(html),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html lang=\"pt-BR\">\n"
        "<head>\n"
        "    <title>Status do Dispositivo</title>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta http-equiv=\"refresh\" content=\"1\">\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Status Atual do Dispositivo</h1>\n"
        "    <p><strong>Botão:</strong> %s</p>\n"
        "    <p><strong>Joystick:</strong> X = %d, Y = %d</p>\n"
        "    <p><strong>Direção:</strong> %s</p>\n"
        "    <p><a href=\"/\">Atualizar agora</a></p>\n"
        "</body>\n"
        "</html>",
        estado_botao,
        x_atual,
        y_atual,
        direcao
    );

    tcp_write(conexao, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(conexao);

    free(requisicao);
    pbuf_free(pacote);
    return ERR_OK;
}

static err_t servidor_tcp_aceitar(void *arg, struct tcp_pcb *nova_conexao, err_t err) {
    tcp_recv(nova_conexao, servidor_tcp_receber);
    return ERR_OK;
}

int main() {
    stdio_init_all();

    // Configura botão pull-up interno
    gpio_init(PINO_BOTAO);
    gpio_set_dir(PINO_BOTAO, GPIO_IN);
    gpio_pull_up(PINO_BOTAO);

    // Inicia Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao iniciar Wi-Fi\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(NOME_REDE_WIFI, SENHA_WIFI, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Erro ao conectar no Wi-Fi\n");
        return -1;
    }

    printf("Conectado! IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr));

    // Configura ADC para o joystick
    adc_init();
    adc_gpio_init(26); // ADC0
    adc_gpio_init(27); // ADC1

    // Configura servidor TCP
    struct tcp_pcb *servidor = tcp_new();
    if (!servidor || tcp_bind(servidor, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao iniciar servidor\n");
        return -1;
    }

    servidor = tcp_listen(servidor);
    tcp_accept(servidor, servidor_tcp_aceitar);
    printf("Servidor ouvindo na porta 80\n");

    while (true) {
        cyw43_arch_poll();
        sleep_ms(10);
    }

    cyw43_arch_deinit();
    return 0;
}
