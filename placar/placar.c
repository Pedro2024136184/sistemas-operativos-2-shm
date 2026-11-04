#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "protocolo.h" 

#define MAX_PATH_LEN 256

// Variáveis Globais de Controlo
HANDLE hPipe = INVALID_HANDLE_VALUE;
HANDLE hMutexPipe;
HANDLE hEventoNotificar;
HANDLE hEventoCancelar;
HANDLE hEventoTerminar;
MSG_ALERTA alertaAtual;

// Função Auxiliar para imprimir com a hora
void ImprimirAlerta(const TCHAR* mensagem) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    _tprintf_s(TEXT("\r%02d/%02d/%04d (%02d:%02d:%02d): '%s'\nCMD> "),
        st.wDay, st.wMonth, st.wYear,
        st.wHour, st.wMinute, st.wSecond,
        mensagem);
}

// Thread que controla a apresentação do Alerta e o seu tempo
DWORD WINAPI ThreadTemporizador(LPVOID lpParam) {
    HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

    HANDLE eventos[4] = { hEventoTerminar, hEventoNotificar, hEventoCancelar, hTimer };

    while (TRUE) {
        DWORD dwWait = WaitForMultipleObjects(4, eventos, FALSE, INFINITE);

        if (dwWait == WAIT_OBJECT_0) {
            // [0] EVENTO TERMINAR: A aplicação vai fechar
            break;
        }
        else if (dwWait == WAIT_OBJECT_0 + 1) {
            // [1] EVENTO NOTIFICAR: Chegou um Novo Alerta
            ResetEvent(hEventoNotificar);

            // Confirma à Central que recebemos o alerta
            WaitForSingleObject(hMutexPipe, INFINITE);
            DWORD bw;
            WriteFile(hPipe, &alertaAtual, sizeof(MSG_ALERTA), &bw, NULL);
            ReleaseMutex(hMutexPipe);

            ImprimirAlerta(alertaAtual.msg);

            // Inicia a contagem decrescente
            LARGE_INTEGER TempoEspera;
            TempoEspera.QuadPart = -((LONGLONG)alertaAtual.duracao * 10000000LL);
            SetWaitableTimer(hTimer, &TempoEspera, 0, NULL, NULL, 0);
        }
        else if (dwWait == WAIT_OBJECT_0 + 2) {
            // [2] EVENTO CANCELAR: A Central mandou cancelar o alerta a meio
            ResetEvent(hEventoCancelar);
            CancelWaitableTimer(hTimer); // Para o relógio atual

            // Confirma à Central que cancelámos
            MSG_CMD cmdConf;
            cmdConf.tipo = TIPO_CANCELAR;
            WaitForSingleObject(hMutexPipe, INFINITE);
            DWORD bw;
            WriteFile(hPipe, &cmdConf, sizeof(MSG_CMD), &bw, NULL);
            ReleaseMutex(hMutexPipe);

            ImprimirAlerta(TEXT("---"));
        }
        else if (dwWait == WAIT_OBJECT_0 + 3) {
            // [3] TIMER DISPAROU: O tempo do alerta acabou naturalmente
            ImprimirAlerta(TEXT("---"));

            // Avisa a Central que o alerta terminou
            MSG_CMD cmdFim;
            cmdFim.tipo = TIPO_FIM_ALERTA;
            WaitForSingleObject(hMutexPipe, INFINITE);
            DWORD bw;
            WriteFile(hPipe, &cmdFim, sizeof(MSG_CMD), &bw, NULL);
            ReleaseMutex(hMutexPipe);
        }
    }

    CloseHandle(hTimer);
    return 0;
}

// Thread dedicada a ouvir o que vem do Named Pipe
DWORD WINAPI ThreadLeituraPipe(LPVOID lpParam) {
    BYTE buffer[sizeof(MSG_ALERTA)];
    DWORD bytesLidos, bytesDisponiveis;

    while (TRUE) {
        // Usa PeekNamedPipe para não bloquear a thread eternamente
        if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesDisponiveis, NULL)) {
            if (bytesDisponiveis > 0) {
                if (ReadFile(hPipe, buffer, sizeof(buffer), &bytesLidos, NULL)) {
                    if (bytesLidos == 0) continue;

                    BYTE tipo = buffer[0];

                    if (tipo == TIPO_NOVO_ALERTA) {
                        // Copia a estrutura toda da rede para a variável global
                        memcpy(&alertaAtual, buffer, sizeof(MSG_ALERTA));
                        SetEvent(hEventoNotificar);
                    }
                    else if (tipo == TIPO_CANCELAR) {
                        SetEvent(hEventoCancelar);
                    }
                    else if (tipo == TIPO_DESLIGAR) {
                        SetEvent(hEventoTerminar);
                        break;
                    }
                    else if (tipo == TIPO_ENCERRAR) {
                        _tprintf_s(TEXT("\n[Info] A plataforma foi encerrada pelo Administrador. A fechar a janela...\n"));
                        SetEvent(hEventoTerminar);

                        // Limpeza rápida de segurança antes do exit
                        CloseHandle(hEventoNotificar);
                        CloseHandle(hEventoCancelar);
                        CloseHandle(hEventoTerminar);
                        CloseHandle(hMutexPipe);
                        if (hPipe != INVALID_HANDLE_VALUE) {
                            CloseHandle(hPipe);
                        }

                        exit(0); // Força o encerramento da janela inteira

                        HWND hJanelaPlacar = GetConsoleWindow();
                        if (hJanelaPlacar != NULL) {
                            PostMessage(hJanelaPlacar, WM_CLOSE, 0, 0);
                        }
                    }
                }
            }
            else {
                Sleep(50); // Não há dados, descansa um pouco
            }
        }
        else {
            // O Peek falhou (ex: o Pipe foi cortado abruptamente)
            break;
        }
    }
    return 0;
}

// Função Principal (Interface e Registo)
int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
#endif

    HKEY hKey;
    TCHAR nomePipe[MAX_PATH_LEN];
    TCHAR comando[100];
    BOOL ligado = FALSE;
    HANDLE hThreadLeitura = NULL;
    HANDLE hThreadTempo = NULL;

    // 1. Tratamento do Registry para recuperar ou guardar o nome do Pipe
    RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\TrabSO2"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);

    if (argc > 1) {
        _tcscpy_s(nomePipe, MAX_PATH_LEN, argv[1]);
        RegSetValueEx(hKey, TEXT("NPIPE"), 0, REG_SZ, (const BYTE*)nomePipe, (DWORD)(_tcslen(nomePipe) + 1) * sizeof(TCHAR));
    }
    else {
        DWORD tipo = REG_SZ;
        DWORD tamanho = sizeof(nomePipe);
        if (RegQueryValueEx(hKey, TEXT("NPIPE"), NULL, &tipo, (LPBYTE)nomePipe, &tamanho) != ERROR_SUCCESS) {
            _tprintf_s(TEXT("[ERRO] O nome do 'NPIPE' nao foi especificado!\n"));
            RegCloseKey(hKey);
            return 1;
        }
    }
    RegCloseKey(hKey);

    _tprintf_s(TEXT("Named Pipe = '%s'\n"), nomePipe);

    // 2. Inicialização das ferramentas de Sincronização
    hEventoNotificar = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEventoCancelar = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEventoTerminar = CreateEvent(NULL, TRUE, FALSE, NULL);
    hMutexPipe = CreateMutex(NULL, FALSE, NULL);

    // 3. Ciclo Principal da Linha de Comandos
    while (TRUE) {
        _tprintf_s(TEXT("CMD> "));
        if (_fgetts(comando, 100, stdin) == NULL) {
            break;
        }

        size_t len = _tcslen(comando);
        if (len > 0 && comando[len - 1] == '\n') comando[len - 1] = '\0';

        if (_tcsicmp(comando, TEXT("liga")) == 0 || _tcsicmp(comando, TEXT("ligar")) == 0) {
            if (!ligado) {
                TCHAR pipeFullAddress[256];
                _stprintf_s(pipeFullAddress, 256, TEXT("\\\\.\\pipe\\%s"), nomePipe);

                // Tenta abrir o tubo que liga à Central
                hPipe = CreateFile(pipeFullAddress, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

                if (hPipe == INVALID_HANDLE_VALUE) {
                    _tprintf_s(TEXT("[Erro] Nao foi possivel contactar a Central.\n"));
                }
                else {
                    // Configura o tubo para ler blocos de mensagens inteiros em vez de letras soltas
                    DWORD dwMode = PIPE_READMODE_MESSAGE;
                    SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL);

                    // Manda pedido de ligação
                    MSG_CMD reqLigar;
                    reqLigar.tipo = TIPO_LIGAR;
                    DWORD bw;
                    WriteFile(hPipe, &reqLigar, sizeof(MSG_CMD), &bw, NULL);

                    // Espera pela resposta com o ID
                    MSG_ID respID;
                    DWORD br;
                    ReadFile(hPipe, &respID, sizeof(MSG_ID), &br, NULL);

                    if (respID.tipo == TIPO_ATRIBUIR_ID) {
                        _tprintf_s(TEXT("Identificador = %d\n"), respID.identificador);
                        ligado = TRUE;

                        // Se conseguiu ligar, arranca as threads
                        hThreadLeitura = CreateThread(NULL, 0, ThreadLeituraPipe, NULL, 0, NULL);
                        hThreadTempo = CreateThread(NULL, 0, ThreadTemporizador, NULL, 0, NULL);
                    }
                }
            }
            else {
                _tprintf_s(TEXT("O placar ja se encontra ligado.\n"));
            }
        }
        else if (_tcsicmp(comando, TEXT("desliga")) == 0 || _tcsicmp(comando, TEXT("desligar")) == 0) {
            _tprintf_s(TEXT("A terminar...\n"));
            if (ligado) {
                // Informa a Central que vai embora
                MSG_CMD reqDesligar;
                reqDesligar.tipo = TIPO_DESLIGAR;
                DWORD bw;
                WaitForSingleObject(hMutexPipe, INFINITE);
                WriteFile(hPipe, &reqDesligar, sizeof(MSG_CMD), &bw, NULL);
                ReleaseMutex(hMutexPipe);

                // Dispara o evento para matar a ThreadTemporizador localmente
                SetEvent(hEventoTerminar);
            }
            break;
        }
        else if (_tcslen(comando) > 0) {
            _tprintf_s(TEXT("Comando invalido.\n"));
        }
    }

    // 4. Limpeza Controlada e Saída
    if (ligado) {
        WaitForSingleObject(hThreadTempo, INFINITE);
        WaitForSingleObject(hThreadLeitura, INFINITE);
        CloseHandle(hThreadTempo);
        CloseHandle(hThreadLeitura);
        CloseHandle(hPipe);
    }

    CloseHandle(hEventoNotificar);
    CloseHandle(hEventoCancelar);
    CloseHandle(hEventoTerminar);
    CloseHandle(hMutexPipe);

    // Truque mestre para destruir fisicamente a janela da consola
    HWND hJanelaPlacar = GetConsoleWindow();
    if (hJanelaPlacar != NULL) {
        PostMessage(hJanelaPlacar, WM_CLOSE, 0, 0);
    }

    return 0;
}