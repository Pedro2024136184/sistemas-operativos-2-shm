/**
 * Protocolo de Comunicação - SO2 Placares e Memória Partilhada (Versão Melhorada)
 * Unidade Curricular: Sistemas Operativos 2 (SO2) - Ano 2026
 * Autor: Pedro Costa (2024136184)
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <windows.h>
#include <tchar.h>

#define MAX_MSG 140
#define MAX_PLACARES 20

#define TIPO_LIGAR          1
#define TIPO_DESLIGAR       2
#define TIPO_FIM_ALERTA     3
#define TIPO_NOVO_ALERTA    4
#define TIPO_CANCELAR       5
#define TIPO_ENCERRAR       6
#define TIPO_ATRIBUIR_ID    7

// Nomes predefinidos para os mecanismos de sincronização e SHM no Windows
#define NOME_PIPE_BASE     TEXT("\\\\.\\pipe\\SO2_PIPE_PLACAR_")
#define NOME_PIPE_CENTRAL  TEXT("\\\\.\\pipe\\SO2_PIPE_CENTRAL")
#define NOME_SHM           TEXT("SO2_SHM_ALERTAS")
#define NOME_EVENTO        TEXT("SO2_EVENTO_ALERTAS")
#define NOME_MUTEX_SHM     TEXT("SO2_MUTEX_SHM")

typedef struct {
    BYTE tipo;
} MSG_CMD;

typedef struct {
    BYTE tipo;
    TCHAR msg[MAX_MSG];
    DWORD duracao;
} MSG_ALERTA;

typedef struct {
    BYTE tipo;
    DWORD identificador;
} MSG_ID;

// Estrutura para a Memória Partilhada (Shared Memory)
typedef struct {
    DWORD versao; // Contador de versão incremental para deteção confiável de alterações (Zero-Polling)
    struct {
        DWORD identificador;
        DWORD duracao;
        TCHAR msg[MAX_MSG];
    } placar[MAX_PLACARES];
    BOOL desligar;
} SHM_ALERTA;

#endif // PROTOCOLO_H
