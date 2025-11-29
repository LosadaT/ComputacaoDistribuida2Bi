#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include "protocol.h"

// Inicializa o servidor
void init_server(ElectionServer *server) {
    server->num_options = 0;
    server->num_voters = 0;
    server->election_closed = false;
    pthread_mutex_init(&server->mutex, NULL);
    
    // Abre arquivo de log
    server->log_file = fopen("logs/eleicao.log", "a");
    if (server->log_file == NULL) {
        perror("Erro ao abrir arquivo de log");
        exit(1);
    }
    
    write_log(server, "=== Servidor iniciado ===");
}

// Carrega opções de votação do arquivo
void load_options(ElectionServer *server, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir %s\n", filename);
        exit(1);
    }
    
    char line[MAX_OPTION_NAME];
    while (fgets(line, sizeof(line), file) && server->num_options < MAX_OPTIONS) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) > 0) {
            strncpy(server->options[server->num_options].name, line, MAX_OPTION_NAME - 1);
            server->options[server->num_options].votes = 0;
            server->num_options++;
        }
    }
    
    fclose(file);
    write_log(server, "Carregadas %d opções de votação", server->num_options);
    
    if (server->num_options < 3) {
        fprintf(stderr, "Erro: é necessário ter pelo menos 3 opções de votação\n");
        exit(1);
    }
}

// Escreve no log com timestamp
void write_log(ElectionServer *server, const char *format, ...) {
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    pthread_mutex_lock(&server->mutex);
    
    fprintf(server->log_file, "[%s] ", timestamp);
    
    va_list args;
    va_start(args, format);
    vfprintf(server->log_file, format, args);
    va_end(args);
    
    fprintf(server->log_file, "\n");
    fflush(server->log_file);
    
    pthread_mutex_unlock(&server->mutex);
}

// Busca votante pelo ID
int find_voter(ElectionServer *server, const char *voter_id) {
    for (int i = 0; i < server->num_voters; i++) {
        if (strcmp(server->voters[i].voter_id, voter_id) == 0) {
            return i;
        }
    }
    return -1;
}

// Adiciona novo votante
int add_voter(ElectionServer *server, const char *voter_id) {
    if (server->num_voters >= MAX_CLIENTS) {
        return -1;
    }
    
    int index = server->num_voters;
    strncpy(server->voters[index].voter_id, voter_id, MAX_VOTER_ID - 1);
    server->voters[index].has_voted = false;
    server->voters[index].voted_option[0] = '\0';
    server->num_voters++;
    
    return index;
}

// Registra voto
bool record_vote(ElectionServer *server, const char *voter_id, int option_index) {
    pthread_mutex_lock(&server->mutex);
    
    int voter_index = find_voter(server, voter_id);
    if (voter_index == -1) {
        if (server->num_voters >= MAX_CLIENTS) {
            pthread_mutex_unlock(&server->mutex);
            return false;
        }
        voter_index = server->num_voters;
        strncpy(server->voters[voter_index].voter_id, voter_id, MAX_VOTER_ID - 1);
        server->voters[voter_index].has_voted = false;
        server->voters[voter_index].voted_option[0] = '\0';
        server->num_voters++;
    }
    
    if (server->voters[voter_index].has_voted) {
        pthread_mutex_unlock(&server->mutex);
        return false;
    }
    
    if (option_index < 0 || option_index >= server->num_options) {
        pthread_mutex_unlock(&server->mutex);
        return false;
    }
    
    server->voters[voter_index].has_voted = true;
    strncpy(server->voters[voter_index].voted_option, 
            server->options[option_index].name, MAX_OPTION_NAME - 1);
    server->options[option_index].votes++;
    
    char option_name[MAX_OPTION_NAME];
    strncpy(option_name, server->options[option_index].name, MAX_OPTION_NAME - 1);
    int total_votes = server->options[option_index].votes;
    
    pthread_mutex_unlock(&server->mutex);
    
    write_log(server, "Voto registrado: %s -> %s (total: %d votos)", voter_id, option_name, total_votes);
    return true;
}

// Obtém placar atual
void get_score(ElectionServer *server, char *buffer, bool final) {
    pthread_mutex_lock(&server->mutex);
    
    if (final) {
        sprintf(buffer, "%s %d", RESP_CLOSED, server->num_options);
    } else {
        sprintf(buffer, "%s %d", RESP_SCORE, server->num_options);
    }
    
    for (int i = 0; i < server->num_options; i++) {
        char temp[256];
        sprintf(temp, "|%s:%d", server->options[i].name, server->options[i].votes);
        strcat(buffer, temp);
    }
    
    pthread_mutex_unlock(&server->mutex);
}

// Encerra eleição
void close_election(ElectionServer *server) {
    pthread_mutex_lock(&server->mutex);
    server->election_closed = true;
    pthread_mutex_unlock(&server->mutex);
    
    write_log(server, "Eleição encerrada por comando administrativo");
    save_final_results(server);
}

// Salva resultados finais
void save_final_results(ElectionServer *server) {
    FILE *file = fopen("logs/resultado_final.txt", "w");
    if (file == NULL) {
        return;
    }
    
    pthread_mutex_lock(&server->mutex);
    
    fprintf(file, "===========================================\n");
    fprintf(file, "    RESULTADO FINAL DA VOTAÇÃO\n");
    fprintf(file, "===========================================\n\n");
    
    time_t now;
    time(&now);
    fprintf(file, "Data: %s\n", ctime(&now));
    
    int total_votes = 0;
    for (int i = 0; i < server->num_options; i++) {
        total_votes += server->options[i].votes;
    }
    
    fprintf(file, "Total de votos: %d\n", total_votes);
    fprintf(file, "Total de votantes registrados: %d\n\n", server->num_voters);
    
    fprintf(file, "-------------------------------------------\n");
    fprintf(file, "Opção                              Votos  %%\n");
    fprintf(file, "-------------------------------------------\n");
    
    for (int i = 0; i < server->num_options; i++) {
        double percentage = total_votes > 0 ? 
            (server->options[i].votes * 100.0 / total_votes) : 0.0;
        fprintf(file, "%-35s %5d %6.2f%%\n", 
                server->options[i].name, 
                server->options[i].votes,
                percentage);
    }
    
    fprintf(file, "-------------------------------------------\n");
    
    pthread_mutex_unlock(&server->mutex);
    
    fclose(file);
    write_log(server, "Resultado final salvo em logs/resultado_final.txt");
}

// Manipula conexão do cliente
void *handle_client(void *arg) {
    ClientData *client_data = (ClientData *)arg;
    int client_socket = client_data->socket;
    ElectionServer *server = client_data->server;
    char buffer[MAX_BUFFER];
    char voter_id[MAX_VOTER_ID] = {0};
    bool authenticated = false;
    
    write_log(server, "Nova conexão estabelecida (socket %d)", client_socket);
    
    while (1) {
        memset(buffer, 0, MAX_BUFFER);
        int bytes_read = recv(client_socket, buffer, MAX_BUFFER - 1, 0);
        
        if (bytes_read <= 0) {
            write_log(server, "Cliente %s desconectado (socket %d)", 
                     authenticated ? voter_id : "não autenticado", client_socket);
            break;
        }
        
        // Remove newline
        buffer[strcspn(buffer, "\n\r")] = 0;
        
        write_log(server, "Recebido de %s: %s", 
                 authenticated ? voter_id : "não autenticado", buffer);
        
        // HELLO <VOTER_ID>
        if (strncmp(buffer, CMD_HELLO, strlen(CMD_HELLO)) == 0) {
            sscanf(buffer, "HELLO %s", voter_id);
            
            pthread_mutex_lock(&server->mutex);
            int voter_index = find_voter(server, voter_id);
            if (voter_index == -1) {
                add_voter(server, voter_id);
            }
            pthread_mutex_unlock(&server->mutex);
            
            authenticated = true;
            sprintf(buffer, "%s %s\n", RESP_WELCOME, voter_id);
            send(client_socket, buffer, strlen(buffer), 0);
            write_log(server, "Cliente autenticado: %s", voter_id);
        }
        // LIST
        else if (strcmp(buffer, CMD_LIST) == 0) {
            if (!authenticated) {
                sprintf(buffer, "ERR NOT_AUTHENTICATED\n");
                send(client_socket, buffer, strlen(buffer), 0);
                continue;
            }
            
            pthread_mutex_lock(&server->mutex);
            sprintf(buffer, "%s %d", RESP_OPTIONS, server->num_options);
            for (int i = 0; i < server->num_options; i++) {
                char temp[256];
                sprintf(temp, "|%s", server->options[i].name);
                strcat(buffer, temp);
            }
            strcat(buffer, "\n");
            pthread_mutex_unlock(&server->mutex);
            
            int sent = send(client_socket, buffer, strlen(buffer), 0);
            write_log(server, "Lista enviada (%d bytes)", sent);
        }
        // VOTE <OPTION>
        else if (strncmp(buffer, CMD_VOTE, strlen(CMD_VOTE)) == 0) {
            write_log(server, "Processando comando VOTE");
            
            if (!authenticated) {
                sprintf(buffer, "ERR NOT_AUTHENTICATED\n");
                send(client_socket, buffer, strlen(buffer), 0);
                continue;
            }
            
            write_log(server, "Cliente autenticado, verificando eleição");
            
            pthread_mutex_lock(&server->mutex);
            bool closed = server->election_closed;
            pthread_mutex_unlock(&server->mutex);
            
            write_log(server, "Eleição fechada? %d", closed);
            
            if (closed) {
                sprintf(buffer, "%s\n", RESP_ERR_CLOSED);
                send(client_socket, buffer, strlen(buffer), 0);
                continue;
            }
            
            int option_num;
            sscanf(buffer, "VOTE %d", &option_num);
            int option_index = option_num - 1;
            
            write_log(server, "Opção escolhida: %d (index %d)", option_num, option_index);
            
            // Verifica se já votou
            pthread_mutex_lock(&server->mutex);
            int voter_index = find_voter(server, voter_id);
            bool has_voted = (voter_index != -1 && server->voters[voter_index].has_voted);
            pthread_mutex_unlock(&server->mutex);
            
            write_log(server, "Já votou? %d", has_voted);
            
            if (has_voted) {
                sprintf(buffer, "%s\n", RESP_ERR_DUPLICATE);
                send(client_socket, buffer, strlen(buffer), 0);
                continue;
            }
            
            write_log(server, "Chamando record_vote");
            
            // Registra o voto
            bool vote_recorded = record_vote(server, voter_id, option_index);
            
            write_log(server, "Voto registrado? %d", vote_recorded);
            
            if (vote_recorded) {
                pthread_mutex_lock(&server->mutex);
                char option_name[MAX_OPTION_NAME];
                strncpy(option_name, server->options[option_index].name, MAX_OPTION_NAME - 1);
                option_name[MAX_OPTION_NAME - 1] = '\0';
                pthread_mutex_unlock(&server->mutex);
                
                sprintf(buffer, "%s %s\n", RESP_OK_VOTED, option_name);
            } else {
                sprintf(buffer, "%s\n", RESP_ERR_INVALID);
            }
            
            write_log(server, "Enviando resposta: %s", buffer);
            int sent = send(client_socket, buffer, strlen(buffer), 0);
            write_log(server, "Resposta enviada (%d bytes)", sent);
        }
        // SCORE
        else if (strcmp(buffer, CMD_SCORE) == 0) {
            if (!authenticated) {
                sprintf(buffer, "ERR NOT_AUTHENTICATED\n");
                send(client_socket, buffer, strlen(buffer), 0);
                continue;
            }
            
            pthread_mutex_lock(&server->mutex);
            bool closed = server->election_closed;
            pthread_mutex_unlock(&server->mutex);
            
            get_score(server, buffer, closed);
            strcat(buffer, "\n");
            send(client_socket, buffer, strlen(buffer), 0);
        }
        // ADMIN CLOSE
        else if (strncmp(buffer, CMD_ADMIN_CLOSE, strlen(CMD_ADMIN_CLOSE)) == 0) {
            write_log(server, "Comando ADMIN CLOSE reconhecido");
            
            if (!authenticated || strcmp(voter_id, "ADMIN") != 0) {
                write_log(server, "Acesso negado: authenticated=%d, voter_id=%s", authenticated, voter_id);
                sprintf(buffer, "ERR NOT_AUTHORIZED\n");
                send(client_socket, buffer, strlen(buffer), 0);
                continue;
            }
            
            write_log(server, "Encerrando eleição...");
            close_election(server);
            sprintf(buffer, "OK ELECTION_CLOSED\n");
            send(client_socket, buffer, strlen(buffer), 0);
            write_log(server, "Resposta enviada: OK ELECTION_CLOSED");
        }
        // BYE
        else if (strcmp(buffer, CMD_BYE) == 0) {
            sprintf(buffer, "%s\n", RESP_BYE);
            send(client_socket, buffer, strlen(buffer), 0);
            write_log(server, "Cliente %s encerrou sessão", voter_id);
            break;
        }
        else {
            sprintf(buffer, "ERR UNKNOWN_COMMAND\n");
            send(client_socket, buffer, strlen(buffer), 0);
        }
    }
    
    close(client_socket);
    free(client_data);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(1);
    }
    
    int port = atoi(argv[1]);
    
    ElectionServer server;
    init_server(&server);
    load_options(&server, "opcoes.txt");
    
    // Cria socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Erro ao criar socket");
        exit(1);
    }
    
    // Permite reutilizar porta
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configura endereço
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no bind");
        exit(1);
    }
    
    // Listen
    if (listen(server_socket, 20) < 0) {
        perror("Erro no listen");
        exit(1);
    }
    
    printf("Servidor de votação iniciado na porta %d\n", port);
    printf("Aguardando conexões...\n");
    write_log(&server, "Servidor aguardando conexões na porta %d", port);
    
    // Loop principal
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Erro no accept");
            continue;
        }
        
        // Cria thread para cliente
        ClientData *client_data = malloc(sizeof(ClientData));
        client_data->socket = client_socket;
        client_data->server = &server;
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_data) != 0) {
            perror("Erro ao criar thread");
            close(client_socket);
            free(client_data);
            continue;
        }
        
        pthread_detach(thread_id);
    }
    
    close(server_socket);
    fclose(server.log_file);
    pthread_mutex_destroy(&server.mutex);
    
    return 0;
}
