#define LWIP_DEBUG 1

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include <string.h>
#include <stdio.h>

// Configurações WiFi 
#define WIFI_SSID "brisa-580702"
#define WIFI_PASSWORD "pi1jgg6t"

// Configurações de pinos
#define LED_PIN 12
#define MONITOR_PIN 18

// Configurações MQTT - HiveMQ público/Assinaturas dos topicos
#define MQTT_BROKER "broker.hivemq.com"
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#define MQTT_CLIENT_ID "pico_client_unique_123"
#define MQTT_TOPIC_LED "pico/led"
#define MQTT_TOPIC_STATUS "pico/input_status"

typedef struct {
    mqtt_client_t *mqtt_client;
    ip_addr_t remote_addr;
    bool connected;
} MQTT_STATE_T;

static MQTT_STATE_T mqtt_state = {0};

// Função para ler o pino monitorado
int read_monitor_pin() {
    return gpio_get(MONITOR_PIN);
}

// Função para controlar o LED
void led_set(bool on) {
    gpio_put(LED_PIN, on ? 1 : 0);
    printf("LED %s\n", on ? "ON" : "OFF");
}

// Função para publicar mensagens
void mqtt_publish_message(mqtt_client_t *client, const char* topic, const char* message) {
    if (!client || !mqtt_client_is_connected(client)) {
        printf("Cliente MQTT não conectado\n");
        return;
    }
    
    err_t err = mqtt_publish(client, topic, message, strlen(message), 0, 0, NULL, NULL);
    if(err != ERR_OK) {
        printf("Erro ao publicar: %d\n", err);
    } else {
        printf("Publicado em %s: %s\n", topic, message);
    }
}

// Callback de dados recebidos
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    // Proteção contra dados nulos ou tamanho inválido
    if (!data || len == 0) {
        return;
    }
    
    char msg[len + 1];
    memcpy(msg, data, len);
    msg[len] = '\0';

    printf("Mensagem recebida: %s\n", msg);

    // Controle do LED via mensagem (case insensitive)
    if(strcasecmp(msg, "on") == 0 || strcasecmp(msg, "1") == 0) {
        led_set(true);
        mqtt_publish_message(mqtt_state.mqtt_client, "pico/led_status", "on");
    } else if(strcasecmp(msg, "off") == 0 || strcasecmp(msg, "0") == 0) {
        led_set(false);
        mqtt_publish_message(mqtt_state.mqtt_client, "pico/led_status", "off");
    }
}

// Callback de novo publish
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    printf("Mensagem recebida no tópico: %s, tamanho: %lu\n", topic, (unsigned long)tot_len);
}

// Callback após subscribe
static void mqtt_sub_request_cb(void *arg, err_t err) {
    if (err == ERR_OK) {
        printf("Subscribe realizado com sucesso\n");
    } else {
        printf("Erro no subscribe: %d\n", err);
    }
}

// Callback de conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    printf("MQTT Connection Status: %d\n", status);
    
    switch(status) {
        case MQTT_CONNECT_ACCEPTED:
            printf("MQTT conectado com sucesso!\n");
            mqtt_state.connected = true;
            
            // Configurar callback para mensagens recebidas
            mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);
            
            // Subscrição ao tópico de controle do LED
            err_t sub_err = mqtt_subscribe(client, MQTT_TOPIC_LED, 0, mqtt_sub_request_cb, arg);
            if (sub_err != ERR_OK) {
                printf("Erro ao subscrever: %d\n", sub_err);
            }
            
            // Publicar mensagem de conexão
            mqtt_publish_message(client, "pico/status", "connected");
            break;
            
     case MQTT_CONNECT_DISCONNECTED:
        printf("MQTT desconectado\n");
        mqtt_state.connected = false;
        break;

    case MQTT_CONNECT_TIMEOUT:
        printf("MQTT timeout na conexão\n");
        mqtt_state.connected = false;
        break;

    case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
        printf("MQTT versão de protocolo recusada\n");
        mqtt_state.connected = false;
        break;

    default:
        printf("MQTT status desconhecido: %d\n", status);
        mqtt_state.connected = false;
        break;
    }
}

// Callback DNS resolvido
void dns_resolved_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    if(ipaddr) {
        printf("DNS resolvido: %s -> %s\n", name, ipaddr_ntoa(ipaddr));
        
        // Copiar endereço IP
        ip_addr_copy(mqtt_state.remote_addr, *ipaddr);

        // Criar cliente MQTT
        mqtt_state.mqtt_client = mqtt_client_new();
        if (!mqtt_state.mqtt_client) {
            printf("Falha ao criar cliente MQTT\n");
            return;
        }

        // Configurar informações do cliente
        struct mqtt_connect_client_info_t ci;
        memset(&ci, 0, sizeof(ci));

        // Necessário:
        ci.client_id = MQTT_CLIENT_ID;  // ID único do cliente MQTT
        ci.keep_alive = 60;             // Intervalo de keep-alive (em segundos)

        /* 
        ci.client_id = MQTT_CLIENT_ID;
        ci.keep_alive = 60;
        ci.will_topic = NULL;
        ci.will_msg = NULL;
        ci.will_qos = 0;
        ci.will_retain = 0;
        ci.client_user = NULL;
        ci.client_pass = NULL;
        */

        // Tentar conectar ao broker
        err_t err = mqtt_client_connect(mqtt_state.mqtt_client, ipaddr, MQTT_PORT, mqtt_connection_cb, NULL, &ci);
        if (err != ERR_OK) {
            printf("Erro ao conectar ao broker MQTT (%s:%d), erro: %d\n", ipaddr_ntoa(ipaddr), MQTT_PORT, err);
        } else {
            printf("Conectando ao broker MQTT (%s:%d)...\n", ipaddr_ntoa(ipaddr), MQTT_PORT);
        }
    } else {
        printf("Falha ao resolver DNS para %s\n", name);
    }
}

void mqtt_run() {
    printf("Iniciando aplicação MQTT...\n");
    
    // Inicialização do WiFi
    if (cyw43_arch_init()) {
        printf("Erro na inicialização do WiFi\n");
        return;
    }
    
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao WiFi %s...\n", WIFI_SSID);
    
    int wifi_status = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (wifi_status != 0) {
        printf("Erro ao conectar WiFi, status: %d\n", wifi_status);
        cyw43_arch_deinit();
        return;
    }
    printf("Conectado ao WiFi com sucesso!\n");

    /* Mostrar IP local
    extern struct netif *netif_default;
    if (netif_default && netif_is_up(netif_default)) {
        printf("IP local: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    }
     */
    
    // Inicializar pinos GPIO
    gpio_init(MONITOR_PIN);
    gpio_set_dir(MONITOR_PIN, GPIO_IN);
    gpio_pull_up(MONITOR_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0); // LED inicialmente desligado

    printf("Pinos GPIO inicializados\n");

    // Resolver DNS do broker HiveMQ
    printf("Resolvendo DNS para %s...\n", MQTT_BROKER);
    err_t dns_err = dns_gethostbyname(MQTT_BROKER, &mqtt_state.remote_addr, dns_resolved_cb, NULL);
    
    if(dns_err == ERR_OK) {
        // DNS já está em cache
        dns_resolved_cb(MQTT_BROKER, &mqtt_state.remote_addr, NULL);
    } else if(dns_err == ERR_INPROGRESS) {
        printf("Resolução DNS em progresso.\n");
    } else {
        printf("Erro ao iniciar resolução DNS: %d\n", dns_err);
        cyw43_arch_deinit();
        return;
    }

    // Variáveis para controle de tempo
    uint32_t last_publish = 0;
    int last_pin_status = -1;
    
    printf("Entrando no loop principal...\n");

    // Loop principal
    while (1) {
        // Processar eventos de rede
        cyw43_arch_poll();
        
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Verificar se o cliente MQTT está conectado
        if (mqtt_state.mqtt_client && mqtt_state.connected && mqtt_client_is_connected(mqtt_state.mqtt_client)) {
            
            // Ler status do pino monitorado
            int pin_status = read_monitor_pin();
            
            // Publicar status do pino se mudou ou a cada 10 segundos
            if (pin_status != last_pin_status || (now - last_publish) >= 10000) {
                char msg[16];
                snprintf(msg, sizeof(msg), "%d", pin_status);
                printf("[DEBUG] Enviando estado do pino 18: %s\n", msg);  //test    
                mqtt_publish_message(mqtt_state.mqtt_client, MQTT_TOPIC_STATUS, msg);
                
                last_pin_status = pin_status;
                last_publish = now;
            }
        }
        
        sleep_ms(100); // Reduzir frequência do loop
    }
    
    
    if (mqtt_state.mqtt_client) {
        mqtt_disconnect(mqtt_state.mqtt_client);
        mqtt_client_free(mqtt_state.mqtt_client);
    }
    cyw43_arch_deinit();
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Aguardar inicialização do USB
    printf("=== Pico W MQTT Client com HiveMQ ===\n");
    mqtt_run();
    return 0;
} 
