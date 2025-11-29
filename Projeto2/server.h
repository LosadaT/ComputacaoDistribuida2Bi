#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <stdbool.h>
#include "protocol.h"

// Estrutura para armazenar opções de votação
typedef struct {
    char name[MAX_OPTION_NAME];
    int votes;
} VoteOption;

// Estrutura para armazenar votantes
typedef struct {
    char voter_id[MAX_VOTER_ID];
    bool has_voted;
    char voted_option[MAX_OPTION_NAME];
} Voter;

// Estrutura global do servidor
typedef struct {
    VoteOption options[MAX_OPTIONS];
    int num_options;
    
    Voter voters[MAX_CLIENTS];
    int num_voters;
    
    bool election_closed;
    
    pthread_mutex_t mutex;
    
    FILE *log_file;
} ElectionServer;

// Estrutura para passar dados para threads de cliente
typedef struct {
    int socket;
    char voter_id[MAX_VOTER_ID];
    ElectionServer *server;
} ClientData;

// Funções principais
void init_server(ElectionServer *server);
void load_options(ElectionServer *server, const char *filename);
void write_log(ElectionServer *server, const char *format, ...);
void *handle_client(void *arg);
int find_voter(ElectionServer *server, const char *voter_id);
int add_voter(ElectionServer *server, const char *voter_id);
bool record_vote(ElectionServer *server, const char *voter_id, int option_index);
void get_score(ElectionServer *server, char *buffer, bool final);
void close_election(ElectionServer *server);
void save_final_results(ElectionServer *server);

#endif
