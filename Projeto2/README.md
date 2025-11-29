# Sistema de Votação Distribuído em Tempo Real

Sistema distribuído de votação com múltiplos clientes conectados a um servidor via sockets TCP, com suporte a concorrência e sincronização usando threads.

## Disciplina
Computação Distribuída - 2025.2

## Tecnologias
- Linguagem: C
- Sockets TCP
- Threads (pthread)
- Sincronização com mutex

## Arquitetura

### Servidor
- Gerencia até 20+ conexões simultâneas via socket TCP
- Contabiliza votos com garantia de voto único por VOTER_ID
- Fornece placar parcial e final
- Registra log de eventos em `logs/eleicao.log`
- Gera `logs/resultado_final.txt` ao encerrar votação

### Cliente
- Conecta ao servidor via TCP
- Informa VOTER_ID
- Lista opções disponíveis
- Registra voto único
- Consulta placar em tempo real
- Encerra sessão

## Compilação

```bash
make
```

Isso irá compilar:
- `server` - Servidor de votação
- `client` - Cliente votante

Para limpar os binários:
```bash
make clean
```

## Execução

### 1. Configurar opções de votação
Edite o arquivo `opcoes.txt` com as opções desejadas (uma por linha):
```
Candidato A
Candidato B
Candidato C
```

### 2. Iniciar o servidor
```bash
./server [porta]
```
Exemplo:
```bash
./server 8080
```

### 3. Conectar clientes
Em outros terminais:
```bash
./client <servidor> <porta> <VOTER_ID>
```
Exemplo:
```bash
./client localhost 8080 VOTER001
```

### 4. Comandos do cliente
Após conectar, o cliente pode usar:
- `LIST` - Listar opções de votação
- `VOTE <numero>` - Votar na opção (1, 2, 3, etc.)
- `SCORE` - Ver placar parcial
- `BYE` - Encerrar conexão

### 5. Encerrar votação (Admin)
Conecte um cliente com VOTER_ID especial:
```bash
./client localhost 8080 ADMIN
```
E execute:
```
ADMIN CLOSE
```

## Protocolo de Comunicação

### Cliente → Servidor
- `HELLO <VOTER_ID>` - Identificação
- `LIST` - Listar opções
- `VOTE <OPCAO>` - Registrar voto
- `SCORE` - Consultar placar
- `BYE` - Encerrar conexão
- `ADMIN CLOSE` - Encerrar votação (apenas ADMIN)

### Servidor → Cliente
- `WELCOME <VOTER_ID>` - Confirmação de conexão
- `OPTIONS <k> <op1> ... <opk>` - Lista de opções
- `OK VOTED <OPCAO>` - Voto registrado
- `ERR DUPLICATE` - Voto duplicado
- `ERR INVALID_OPTION` - Opção inválida
- `SCORE <k> <op1>:<count1> ...` - Placar parcial
- `CLOSED FINAL <k> <op1>:<count1> ...` - Placar final
- `ERR CLOSED` - Votação encerrada
- `BYE` - Confirmação de desconexão

## Arquivos Gerados

- `logs/eleicao.log` - Log detalhado de todos os eventos
- `logs/resultado_final.txt` - Resultado final da votação

## Casos de Teste

1. **Voto único**: Segundo voto do mesmo VOTER_ID → `ERR DUPLICATE`
2. **Opção inválida**: Voto em opção inexistente → `ERR INVALID_OPTION`
3. **20 clientes simultâneos**: Sistema funciona sem travar
4. **Cliente cai antes de votar**: Voto não é contado
5. **Admin encerra votação**: Após `ADMIN CLOSE`, novos votos → `ERR CLOSED`
6. **Consulta após encerramento**: Retorna placar final

## Estrutura de Arquivos

```
Projeto/
├── server.c              # Implementação do servidor
├── client.c              # Implementação do cliente
├── server.h              # Headers do servidor
├── protocol.h            # Definições do protocolo
├── server                # Servidor compilado
├── client                # Cliente compilado
├── logs/
│   ├── eleicao.log       # Log de eventos (gerado)
│   └── resultado_final.txt # Resultado final (gerado)
├── opcoes.txt            # Opções de votação (configurável)
├── Makefile              # Compilação
├── README.md             # Este arquivo
└── relatorio.md          # Relatório técnico
```

## Autores
Francisco - 103646673
Pedro - 10441998

## Data
Novembro de 2025
