#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "protocolo.h"

typedef struct {
    HANDLE hPipe;
    HANDLE hThread;
    DWORD id;
    BOOL ativo;
    BOOL temAlerta;
    DWORD duracaoAtual; // Novo campo adicionado para a M3
    TCHAR msgAtual[MAX_MSG];
} PLACAR_INFO;

PLACAR_INFO placares[MAX_PLACARES];
HANDLE hMutexPlacares;
DWORD nextId = 1;
BOOL servidorA_Correr = TRUE;

// Novas variáveis globais para a M3
HANDLE hMapFile = NULL;
SHM_ALERTA* pBuf = NULL;
HANDLE hEventMonitor = NULL;
HANDLE hMutexSHM = NULL;

// Função para espelhar o estado atual na Memória Partilhada
void AtualizarSHM() {
    if (pBuf == NULL) return;

    WaitForSingleObject(hMutexSHM, INFINITE);
    WaitForSingleObject(hMutexPlacares, INFINITE);

    BOOL flagDesligar = pBuf->desligar;
    DWORD novaVersao = pBuf->versao + 1;
    ZeroMemory(pBuf, sizeof(SHM_ALERTA));
    pBuf->desligar = flagDesligar;
    pBuf->versao = novaVersao;

    for (int i = 0; i < MAX_PLACARES; i++) {
        if (placares[i].ativo && placares[i].id != 0) {
            pBuf->placar[i].identificador = placares[i].id;
            if (placares[i].temAlerta) {
                _tcscpy_s(pBuf->placar[i].msg, MAX_MSG, placares[i].msgAtual);
                pBuf->placar[i].duracao = placares[i].duracaoAtual;
            }
            else {
                pBuf->placar[i].duracao = 0;
            }
        }
    }

    ReleaseMutex(hMutexPlacares);
    ReleaseMutex(hMutexSHM);

    SetEvent(hEventMonitor);
    Sleep(50);
    ResetEvent(hEventMonitor);
}

DWORD WINAPI ThreadAtendePlacar(LPVOID param) {
    int slot = (int)(INT_PTR)param;
    HANDLE hPipe = placares[slot].hPipe;
    BYTE buffer[sizeof(MSG_ALERTA)];
    DWORD bytesLidos, bytesDisponiveis;

    while (servidorA_Correr && placares[slot].ativo) {
        if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesDisponiveis, NULL)) {
            if (bytesDisponiveis > 0) {
                if (ReadFile(hPipe, buffer, sizeof(buffer), &bytesLidos, NULL)) {
                    if (bytesLidos == 0) continue;

                    BYTE tipo = buffer[0];

                    if (tipo == TIPO_LIGAR) {
                        WaitForSingleObject(hMutexPlacares, INFINITE);
                        placares[slot].id = nextId++;
                        DWORD assignedId = placares[slot].id;
                        ReleaseMutex(hMutexPlacares);

                        MSG_ID resp;
                        resp.tipo = TIPO_ATRIBUIR_ID;
                        resp.identificador = assignedId;
                        DWORD bytesEscritos;
                        WriteFile(hPipe, &resp, sizeof(MSG_ID), &bytesEscritos, NULL);
                        _tprintf_s(TEXT("\n[Info] Novo placar ligado! ID atribuido: %d\nCMD> "), assignedId);

                        AtualizarSHM(); // M3: Placar novo entrou
                    }
                    else if (tipo == TIPO_DESLIGAR) {
                        MSG_CMD resp;
                        resp.tipo = TIPO_DESLIGAR;
                        DWORD bw;
                        WriteFile(hPipe, &resp, sizeof(MSG_CMD), &bw, NULL);
                        _tprintf_s(TEXT("\n[Info] Placar %d desligou-se.\nCMD> "), placares[slot].id);
                        break;
                    }
                    else if (tipo == TIPO_FIM_ALERTA) {
                        WaitForSingleObject(hMutexPlacares, INFINITE);
                        placares[slot].temAlerta = FALSE;
                        ReleaseMutex(hMutexPlacares);

                        AtualizarSHM(); // M3: Alerta terminou o tempo
                    }
                    else if (tipo == TIPO_NOVO_ALERTA) {
                        WaitForSingleObject(hMutexPlacares, INFINITE);
                        placares[slot].temAlerta = TRUE;
                        ReleaseMutex(hMutexPlacares);
                        _tprintf_s(TEXT("\n[Info] Placar %d confirmou rececao do alerta.\nCMD> "), placares[slot].id);

                        AtualizarSHM(); // M3: Alerta confirmado e ativo no placar
                    }
                    else if (tipo == TIPO_CANCELAR) {
                        WaitForSingleObject(hMutexPlacares, INFINITE);
                        placares[slot].temAlerta = FALSE;
                        ReleaseMutex(hMutexPlacares);
                        _tprintf_s(TEXT("\n[Info] Placar %d confirmou o cancelamento.\nCMD> "), placares[slot].id);

                        AtualizarSHM(); // M3: Alerta removido do placar
                    }
                }
            }
            else {
                Sleep(50);
            }
        }
        else {
            break;
        }
    }

    WaitForSingleObject(hMutexPlacares, INFINITE);
    placares[slot].ativo = FALSE;
    placares[slot].temAlerta = FALSE;
    placares[slot].id = 0;
    ReleaseMutex(hMutexPlacares);

    AtualizarSHM(); // M3: O placar já não está ativo

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    return 0;
}

DWORD WINAPI ThreadEsperaLigacoes(LPVOID param) {
    TCHAR* nomePipe = (TCHAR*)param;
    TCHAR pipeFullAddress[256];
    _stprintf_s(pipeFullAddress, 256, TEXT("\\\\.\\pipe\\%s"), nomePipe);

    while (servidorA_Correr) {
        HANDLE hPipe = CreateNamedPipe(
            pipeFullAddress,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            MAX_PLACARES,
            4096,
            4096,
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected) {
            WaitForSingleObject(hMutexPlacares, INFINITE);
            int slotLivre = -1;
            for (int i = 0; i < MAX_PLACARES; i++) {
                if (!placares[i].ativo) {
                    slotLivre = i;
                    break;
                }
            }

            if (slotLivre != -1) {
                placares[slotLivre].hPipe = hPipe;
                placares[slotLivre].ativo = TRUE;
                placares[slotLivre].id = 0;
                placares[slotLivre].temAlerta = FALSE;
                placares[slotLivre].duracaoAtual = 0;
                placares[slotLivre].hThread = CreateThread(NULL, 0, ThreadAtendePlacar, (LPVOID)(INT_PTR)slotLivre, 0, NULL);
            }
            else {
                CloseHandle(hPipe);
            }
            ReleaseMutex(hMutexPlacares);
        }
        else {
            CloseHandle(hPipe);
        }
    }
    return 0;
}

int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
#endif

    if (argc < 2) {
        _tprintf_s(TEXT("[Erro] Uso: central.exe <nome_do_pipe>\n"));
        return 1;
    }

    TCHAR* nomePipe = argv[1];

    hMutexPlacares = CreateMutex(NULL, FALSE, NULL);
    for (int i = 0; i < MAX_PLACARES; i++) {
        placares[i].ativo = FALSE;
        placares[i].hThread = NULL;
    }

    // M3: Inicialização de recursos de partilha com o Monitor
    hEventMonitor = CreateEvent(NULL, TRUE, FALSE, NOME_EVENTO);
    hMutexSHM = CreateMutex(NULL, FALSE, NOME_MUTEX_SHM);
    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SHM_ALERTA), NOME_SHM);

    if (hMapFile != NULL) {
        pBuf = (SHM_ALERTA*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHM_ALERTA));
        if (pBuf != NULL) {
            ZeroMemory(pBuf, sizeof(SHM_ALERTA));
            pBuf->desligar = FALSE;
        }
    }

    HANDLE hDispatcher = CreateThread(NULL, 0, ThreadEsperaLigacoes, nomePipe, 0, NULL);
    _tprintf_s(TEXT("[Central] A escuta no Named Pipe '%s'...\n"), nomePipe);

    TCHAR linhaComando[256];
    while (servidorA_Correr) {
        _tprintf_s(TEXT("CMD> "));
        if (_fgetts(linhaComando, 256, stdin) == NULL) {
            break;
        }

        size_t len = _tcslen(linhaComando);
        if (len > 0 && linhaComando[len - 1] == '\n') linhaComando[len - 1] = '\0';
        if (_tcslen(linhaComando) == 0) continue;

        TCHAR cmd[30];
        _stscanf_s(linhaComando, TEXT("%29s"), cmd, 30);

        if (_tcsicmp(cmd, TEXT("listar")) == 0) {
            WaitForSingleObject(hMutexPlacares, INFINITE);
            _tprintf_s(TEXT("\n--- PLACARES REGISTADOS ---\n"));
            int contagem = 0;
            for (int i = 0; i < MAX_PLACARES; i++) {
                if (placares[i].ativo && placares[i].id != 0) {
                    contagem++;
                    _tprintf_s(TEXT("ID: %d | Estado: %s"), placares[i].id, placares[i].temAlerta ? TEXT("COM ALERTA") : TEXT("Livre"));
                    if (placares[i].temAlerta) _tprintf_s(TEXT(" ('%s' - %d seg)"), placares[i].msgAtual, placares[i].duracaoAtual);
                    _tprintf_s(TEXT("\n"));
                }
            }
            if (contagem == 0) _tprintf_s(TEXT("Nenhum placar ligado.\n"));
            _tprintf_s(TEXT("---------------------------\n"));
            ReleaseMutex(hMutexPlacares);
        }

        else if (_tcsicmp(cmd, TEXT("encerrar")) == 0) {
            _tprintf_s(TEXT("A encerrar plataforma...\n"));
            MSG_CMD msgFim;
            msgFim.tipo = TIPO_ENCERRAR;

            HANDLE pipesParaFechar[MAX_PLACARES];
            int numPipes = 0;

            WaitForSingleObject(hMutexPlacares, INFINITE);
            servidorA_Correr = FALSE;
            for (int i = 0; i < MAX_PLACARES; i++) {
                if (placares[i].ativo) {
                    pipesParaFechar[numPipes++] = placares[i].hPipe;
                }
            }
            ReleaseMutex(hMutexPlacares);

            for (int i = 0; i < numPipes; i++) {
                DWORD bw;
                WriteFile(pipesParaFechar[i], &msgFim, sizeof(MSG_CMD), &bw, NULL);
            }

            // M3: Avisar o Monitor que a plataforma vai desligar
            if (pBuf != NULL) {
                WaitForSingleObject(hMutexSHM, INFINITE);
                pBuf->desligar = TRUE;
                ReleaseMutex(hMutexSHM);
                SetEvent(hEventMonitor);
                Sleep(50);
            }
            break;
        }

        else if (_tcsicmp(cmd, TEXT("alerta")) == 0) {
            TCHAR msg[MAX_MSG];
            DWORD duracao = 0, idAlvo = 0;

            TCHAR* inicioAspas = _tcschr(linhaComando, '\"');
            TCHAR* fimAspas = inicioAspas ? _tcschr(inicioAspas + 1, '\"') : NULL;

            if (inicioAspas && fimAspas) {
                int tamanhoMsg = (int)(fimAspas - inicioAspas - 1);
                if (tamanhoMsg >= MAX_MSG) tamanhoMsg = MAX_MSG - 1;

                ZeroMemory(msg, sizeof(msg));
                _tcsncpy_s(msg, MAX_MSG, inicioAspas + 1, tamanhoMsg);
                msg[tamanhoMsg] = TEXT('\0');

                if (_stscanf_s(fimAspas + 1, TEXT("%u %u"), &duracao, &idAlvo) == 2) {

                    MSG_ALERTA alertaReq;
                    ZeroMemory(&alertaReq, sizeof(MSG_ALERTA));
                    alertaReq.tipo = TIPO_NOVO_ALERTA;
                    alertaReq.duracao = duracao;
                    _tcscpy_s(alertaReq.msg, MAX_MSG, msg);

                    HANDLE pipesAlvo[MAX_PLACARES];
                    int enviados = 0;

                    WaitForSingleObject(hMutexPlacares, INFINITE);
                    for (int i = 0; i < MAX_PLACARES; i++) {
                        if (placares[i].ativo && (idAlvo == 0 || placares[i].id == idAlvo)) {
                            pipesAlvo[enviados++] = placares[i].hPipe;
                            _tcscpy_s(placares[i].msgAtual, MAX_MSG, msg);
                            placares[i].duracaoAtual = duracao; // M3: Guardar a duração atual
                        }
                    }
                    ReleaseMutex(hMutexPlacares);

                    for (int i = 0; i < enviados; i++) {
                        DWORD bw;
                        if (!WriteFile(pipesAlvo[i], &alertaReq, sizeof(MSG_ALERTA), &bw, NULL)) {
                            _tprintf_s(TEXT("[Erro] Falha ao comunicar com um placar.\n"));
                        }
                    }

                    if (enviados == 0) _tprintf_s(TEXT("[Aviso] Nenhum placar ativo encontrado com o ID %d.\n"), idAlvo);
                }
                else {
                    _tprintf_s(TEXT("[Erro] Valores de duracao ou ID invalidos.\n"));
                }
            }
            else {
                _tprintf_s(TEXT("[Erro] Formato: alerta \"mensagem\" <duracao> <id_placar>\n"));
            }
        }

        else if (_tcsicmp(cmd, TEXT("cancelar")) == 0) {
            DWORD idAlvo = 0;
            if (_stscanf_s(linhaComando, TEXT("%*s %u"), &idAlvo) == 1) {
                MSG_CMD cancelarReq;
                cancelarReq.tipo = TIPO_CANCELAR;

                HANDLE pipeAlvo = NULL;

                WaitForSingleObject(hMutexPlacares, INFINITE);
                for (int i = 0; i < MAX_PLACARES; i++) {
                    if (placares[i].ativo && placares[i].id == idAlvo) {
                        pipeAlvo = placares[i].hPipe;
                        break;
                    }
                }
                ReleaseMutex(hMutexPlacares);

                if (pipeAlvo != NULL) {
                    DWORD bw;
                    WriteFile(pipeAlvo, &cancelarReq, sizeof(MSG_CMD), &bw, NULL);
                }
                else {
                    _tprintf_s(TEXT("[Aviso] Placar com ID %d nao encontrado.\n"), idAlvo);
                }
            }
        }
        else {
            _tprintf_s(TEXT("Comando desconhecido.\n"));
        }
    }

    for (int i = 0; i < MAX_PLACARES; i++) {
        if (placares[i].hThread != NULL) {
            CloseHandle(placares[i].hThread);
        }
    }

    CloseHandle(hDispatcher);
    CloseHandle(hMutexPlacares);

    // M3: Limpeza de recursos
    if (pBuf != NULL) UnmapViewOfFile(pBuf);
    if (hMapFile != NULL) CloseHandle(hMapFile);
    if (hEventMonitor != NULL) CloseHandle(hEventMonitor);
    if (hMutexSHM != NULL) CloseHandle(hMutexSHM);

    return 0;
}