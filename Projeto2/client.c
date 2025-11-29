#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

void print_menu() {
    printf("\n=== MENU DE VOTAÇÃO ===\n");
    printf("LIST          - Listar opções disponíveis\n");
    printf("VOTE <numero> - Votar na opção (ex: VOTE 1)\n");
    printf("SCORE         - Ver placar parcial\n");
    printf("BYE           - Encerrar sessão\n");
    printf("========================\n\n");
}

void print_admin_menu() {
    printf("\n=== MENU ADMINISTRATIVO ===\n");
    printf("ADMIN CLOSE - Encerrar votação\n");
    printf("SCORE       - Ver placar\n");
    printf("BYE         - Encerrar sessão\n");
    printf("===========================\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <servidor> <porta> <VOTER_ID>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s localhost 8080 VOTER001\n", argv[0]);
        exit(1);
    }
    
    char *server_host = argv[1];
    int server_port = atoi(argv[2]);
    char *voter_id = argv[3];
    
    // Cria socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Erro ao criar socket");
        exit(1);
    }
    
    // Configura endereço do servidor
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_host, &server_addr.sin_addr) <= 0) {
        // Tenta resolver hostname
        if (strcmp(server_host, "localhost") == 0) {
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        } else {
            fprintf(stderr, "Endereço inválido: %s\n", server_host);
            exit(1);
        }
    }
    
    // Conecta ao servidor
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao conectar ao servidor");
        exit(1);
    }
    
    printf("Conectado ao servidor %s:%d\n", server_host, server_port);
    
    // Envia HELLO
    char buffer[MAX_BUFFER];
    sprintf(buffer, "HELLO %s\n", voter_id);
    send(sock, buffer, strlen(buffer), 0);
    
    // Recebe WELCOME
    memset(buffer, 0, MAX_BUFFER);
    recv(sock, buffer, MAX_BUFFER - 1, 0);
    printf("Servidor: %s", buffer);
    
    // Verifica se é ADMIN
    bool is_admin = (strcmp(voter_id, "ADMIN") == 0);
    
    if (is_admin) {
        printf("\n*** MODO ADMINISTRATIVO ***\n");
        print_admin_menu();
    } else {
        printf("\nBem-vindo, %s!\n", voter_id);
        print_menu();
    }
    
    // Loop de comandos
    while (1) {
        printf("> ");
        fflush(stdout);
        
        char command[MAX_BUFFER];
        if (fgets(command, MAX_BUFFER, stdin) == NULL) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = 0;
        
        // Comando vazio
        if (strlen(command) == 0) {
            continue;
        }
        
        // Adiciona newline para protocolo
        strcat(command, "\n");
        
        // Envia comando
        send(sock, command, strlen(command), 0);
        
        // Recebe resposta
        memset(buffer, 0, MAX_BUFFER);
        int bytes_received = recv(sock, buffer, MAX_BUFFER - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Servidor desconectado.\n");
            break;
        }
        
        // Remove newline da resposta
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Processa resposta
        if (strncmp(buffer, RESP_OPTIONS, strlen(RESP_OPTIONS)) == 0) {
            // Parse OPTIONS response
            int num_options;
            char *ptr = buffer + strlen(RESP_OPTIONS) + 1;
            sscanf(ptr, "%d", &num_options);
            
            printf("\n=== OPÇÕES DE VOTAÇÃO ===\n");
            
            // Avança para as opções (separadas por |)
            ptr = strchr(ptr, '|');
            if (ptr) {
                ptr++; // Pula o primeiro |
                int option_num = 1;
                char *option = strtok(ptr, "|");
                while (option != NULL && option_num <= num_options) {
                    // Remove possível \n no final
                    option[strcspn(option, "\n\r")] = 0;
                    printf("%d. %s\n", option_num++, option);
                    option = strtok(NULL, "|");
                }
            }
            printf("========================\n");
        }
        else if (strncmp(buffer, RESP_OK_VOTED, strlen(RESP_OK_VOTED)) == 0) {
            printf("✓ Voto registrado com sucesso!\n");
            printf("%s\n", buffer);
        }
        else if (strcmp(buffer, RESP_ERR_DUPLICATE) == 0) {
            printf("✗ Erro: Você já votou anteriormente!\n");
        }
        else if (strcmp(buffer, RESP_ERR_INVALID) == 0) {
            printf("✗ Erro: Opção inválida!\n");
        }
        else if (strcmp(buffer, RESP_ERR_CLOSED) == 0) {
            printf("✗ Erro: A votação foi encerrada!\n");
        }
        else if (strncmp(buffer, RESP_SCORE, strlen(RESP_SCORE)) == 0) {
            // Parse SCORE response
            int num_options;
            char *ptr = buffer + strlen(RESP_SCORE) + 1;
            sscanf(ptr, "%d", &num_options);
            
            printf("\n=== PLACAR PARCIAL ===\n");
            
            // Avança para os resultados (separados por |)
            ptr = strchr(ptr, '|');
            if (ptr) {
                ptr++; // Pula o primeiro |
                char *result = strtok(ptr, "|");
                while (result != NULL) {
                    char option[MAX_OPTION_NAME];
                    int votes;
                    // Remove possível \n
                    result[strcspn(result, "\n\r")] = 0;
                    if (sscanf(result, "%[^:]:%d", option, &votes) == 2) {
                        printf("%-40s: %d votos\n", option, votes);
                    }
                    result = strtok(NULL, "|");
                }
            }
            printf("======================\n");
        }
        else if (strncmp(buffer, RESP_CLOSED, strlen(RESP_CLOSED)) == 0) {
            // Parse CLOSED FINAL response
            int num_options;
            char *ptr = buffer + strlen(RESP_CLOSED) + 1;
            sscanf(ptr, "%d", &num_options);
            
            printf("\n=== RESULTADO FINAL ===\n");
            
            // Avança para os resultados (separados por |)
            ptr = strchr(ptr, '|');
            if (ptr) {
                ptr++; // Pula o primeiro |
                
                // Remove newline do buffer
                char temp_buffer[MAX_BUFFER];
                strncpy(temp_buffer, ptr, MAX_BUFFER - 1);
                temp_buffer[strcspn(temp_buffer, "\n\r")] = 0;
                
                // Primeiro, calcula total
                char results_copy[MAX_BUFFER];
                strcpy(results_copy, temp_buffer);
                int total_votes = 0;
                char *temp = strtok(results_copy, "|");
                while (temp != NULL) {
                    int votes;
                    char option[MAX_OPTION_NAME];
                    if (sscanf(temp, "%[^:]:%d", option, &votes) == 2) {
                        total_votes += votes;
                    }
                    temp = strtok(NULL, "|");
                }
                
                // Agora imprime com porcentagens
                char *result = strtok(temp_buffer, "|");
                while (result != NULL) {
                    char option[MAX_OPTION_NAME];
                    int votes;
                    if (sscanf(result, "%[^:]:%d", option, &votes) == 2) {
                        double percentage = total_votes > 0 ? (votes * 100.0 / total_votes) : 0.0;
                        printf("%-40s: %d votos (%.2f%%)\n", option, votes, percentage);
                    }
                    result = strtok(NULL, "|");
                }
                
                printf("\nTotal de votos: %d\n", total_votes);
            }
            printf("=======================\n");
        }
        else if (strcmp(buffer, RESP_BYE) == 0) {
            printf("Sessão encerrada. Até logo!\n");
            break;
        }
        else if (strncmp(buffer, "OK ELECTION_CLOSED", 18) == 0) {
            printf("✓ Votação encerrada com sucesso!\n");
        }
        else if (strncmp(buffer, "ERR NOT_AUTHORIZED", 18) == 0) {
            printf("✗ Erro: Você não tem permissão para executar este comando!\n");
        }
        else {
            printf("Servidor: %s\n", buffer);
        }
    }
    
    close(sock);
    return 0;
}
