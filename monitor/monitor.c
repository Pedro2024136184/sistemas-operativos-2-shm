#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "protocolo.h"
#include "resource.h"

// --- ESTRUTURAS E VARIÁVEIS GLOBAIS ---
typedef struct {
    DWORD id;
    DWORD duracao;
    TCHAR msg[MAX_MSG];
} ALERTA_LOCAL;

// Cópia local para não bloquear a SHM enquanto a janela desenha
ALERTA_LOCAL alertasAtivos[MAX_PLACARES];
int totalAlertas = 0;
HANDLE hMutexLocal;

// Configurações do Utilizador
int maxAlertasVisiveis = 5;
int paginaAtual = 0;
TCHAR nomeSHM[256] = NOME_SHM;
TCHAR nomeEvento[256] = NOME_EVENTO;
TCHAR nomeMutex[256] = NOME_MUTEX_SHM;

// Recursos de Comunicação
HANDLE hMapFile = NULL;
SHM_ALERTA* pBuf = NULL;
HANDLE hEventCentral = NULL;
HANDLE hMutexSHM = NULL;
HANDLE hThreadComunicacao = NULL;

BOOL aplicacaoA_Correr = TRUE;
BOOL plataformaEncerrada = FALSE;
HWND hMainWindow = NULL;

// --- FUNÇÕES DE COMUNICAÇÃO ---
void DesligarRecursosSHM() {
    if (pBuf) { UnmapViewOfFile(pBuf); pBuf = NULL; }
    if (hMapFile) { CloseHandle(hMapFile); hMapFile = NULL; }
    if (hEventCentral) { CloseHandle(hEventCentral); hEventCentral = NULL; }
    if (hMutexSHM) { CloseHandle(hMutexSHM); hMutexSHM = NULL; }
}

void LigarRecursosSHM() {
    DesligarRecursosSHM();
    hEventCentral = CreateEvent(NULL, TRUE, FALSE, nomeEvento);
    hMutexSHM = CreateMutex(NULL, FALSE, nomeMutex);
    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SHM_ALERTA), nomeSHM);

    if (hMapFile) {
        pBuf = (SHM_ALERTA*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(SHM_ALERTA));
    }
}

DWORD WINAPI ThreadLeituraSHM(LPVOID lpParam) {
    while (aplicacaoA_Correr) {
        if (hEventCentral == NULL) {
            Sleep(1000);
            continue;
        }

        // Fica à espera sem Polling
        DWORD dwWait = WaitForSingleObject(hEventCentral, 1000);

        if (!aplicacaoA_Correr) break;

        if (dwWait == WAIT_OBJECT_0 && pBuf && hMutexSHM) {
            WaitForSingleObject(hMutexSHM, INFINITE);
            WaitForSingleObject(hMutexLocal, INFINITE);

            if (pBuf->desligar) {
                plataformaEncerrada = TRUE;
                totalAlertas = 0;
                ReleaseMutex(hMutexLocal);
                ReleaseMutex(hMutexSHM);
                InvalidateRect(hMainWindow, NULL, TRUE);

                break;
            }
            else {
                plataformaEncerrada = FALSE;
                totalAlertas = 0;
                for (int i = 0; i < MAX_PLACARES; i++) {
                    // Copiar apenas os que têm alerta ativo
                    if (pBuf->placar[i].identificador != 0 && pBuf->placar[i].duracao > 0) {
                        alertasAtivos[totalAlertas].id = pBuf->placar[i].identificador;
                        alertasAtivos[totalAlertas].duracao = pBuf->placar[i].duracao;
                        _tcscpy_s(alertasAtivos[totalAlertas].msg, MAX_MSG, pBuf->placar[i].msg);
                        totalAlertas++;
                    }
                }
            }

            // Garantir que a página atual não ultrapassa o limite ao receber novos dados
            int paginasTotais = (totalAlertas + maxAlertasVisiveis - 1) / maxAlertasVisiveis;
            if (paginaAtual >= paginasTotais && paginasTotais > 0) {
                paginaAtual = paginasTotais - 1;
            }
            else if (paginasTotais == 0) {
                paginaAtual = 0;
            }

            ReleaseMutex(hMutexLocal);
            ReleaseMutex(hMutexSHM);

            // Avisar a janela para se redesenhar com os novos dados
            InvalidateRect(hMainWindow, NULL, TRUE);
        }
    }
    return 0;
}

// --- INTERFACE GRÁFICA ---
INT_PTR CALLBACK DialogoConfigProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        SetDlgItemInt(hDlg, IDC_EDIT_MAX_ALERTAS, maxAlertasVisiveis, FALSE);
        SetDlgItemText(hDlg, IDC_EDIT_SHM_NOME, nomeSHM);
        SetDlgItemText(hDlg, IDC_EDIT_EVENTO_NOME, nomeEvento);
        SetDlgItemText(hDlg, IDC_EDIT_MUTEX_NOME, nomeMutex); // CORRIGIDO AQUI
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            maxAlertasVisiveis = GetDlgItemInt(hDlg, IDC_EDIT_MAX_ALERTAS, NULL, FALSE);
            if (maxAlertasVisiveis < 1) maxAlertasVisiveis = 1;

            GetDlgItemText(hDlg, IDC_EDIT_SHM_NOME, nomeSHM, 256);
            GetDlgItemText(hDlg, IDC_EDIT_EVENTO_NOME, nomeEvento, 256);
            GetDlgItemText(hDlg, IDC_EDIT_MUTEX_NOME, nomeMutex, 256); // CORRIGIDO AQUI

            // Forçar a reconexão com os novos nomes
            LigarRecursosSHM();
            InvalidateRect(hMainWindow, NULL, TRUE);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        hMainWindow = hWnd;
        LigarRecursosSHM();
        hMutexLocal = CreateMutex(NULL, FALSE, NULL);
        hThreadComunicacao = CreateThread(NULL, 0, ThreadLeituraSHM, NULL, 0, NULL);

        // --- ADICIONA APENAS ESTE BLOCO DAQUI PARA BAIXO ---
        HMENU hMenu = CreateMenu();
        HMENU hMenuFicheiro = CreatePopupMenu();

        AppendMenu(hMenuFicheiro, MF_STRING, ID_FICHEIRO_CONFIGURACAO, TEXT("Configuracao..."));
        AppendMenu(hMenuFicheiro, MF_STRING, ID_FICHEIRO_ACERCA, TEXT("Acerca"));
        AppendMenu(hMenuFicheiro, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenuFicheiro, MF_STRING, ID_FICHEIRO_SAIR, TEXT("Sair"));

        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuFicheiro, TEXT("Ficheiro"));
        SetMenu(hWnd, hMenu);
        // --------------------------------------------------
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_FICHEIRO_CONFIGURACAO) {
            INT_PTR resultado = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CONFIG_DIALOG), hWnd, DialogoConfigProc);

            // Se o Visual Studio não compilou o ficheiro .rc, o resultado é -1
            if (resultado == -1) {
                MessageBox(hWnd, TEXT("ERRO: O ficheiro monitor.rc não está ligado ao projeto!\n\nPara resolver:\n1. Vai ao Solution Explorer (à direita no Visual Studio).\n2. Botão direito na pasta 'Resource Files' (Ficheiros de Recurso).\n3. 'Add' -> 'Existing Item...'\n4. Seleciona o teu 'monitor.rc' e 'resource.h'.\n5. Clica em Rebuild e testa outra vez."), TEXT("Erro Visual Studio"), MB_OK | MB_ICONERROR);
            }
        }
        else if (LOWORD(wParam) == ID_FICHEIRO_ACERCA) {
            MessageBox(hWnd, TEXT("Monitor de Alertas\nAutores:\nDavid Ribeiro da Silva (2024136184)\nPedro A. M. G. Costa (2024136184)"), TEXT("Acerca"), MB_OK | MB_ICONINFORMATION);
        }
        else if (LOWORD(wParam) == ID_FICHEIRO_SAIR) {
            DestroyWindow(hWnd);
        }
        break;

    case WM_KEYDOWN:
        WaitForSingleObject(hMutexLocal, INFINITE);
        int maxPaginas = (totalAlertas + maxAlertasVisiveis - 1) / maxAlertasVisiveis;

        if (wParam == VK_NEXT) { // Page Down
            if (paginaAtual < maxPaginas - 1) {
                paginaAtual++;
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        else if (wParam == VK_PRIOR) { // Page Up
            if (paginaAtual > 0) {
                paginaAtual--;
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        ReleaseMutex(hMutexLocal);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        WaitForSingleObject(hMutexLocal, INFINITE);

        if (plataformaEncerrada) {
            TextOut(hdc, 20, 20, TEXT("[ PLATAFORMA ENCERRADA PELA CENTRAL ]"), 37);
        }
        else if (totalAlertas == 0) {
            TextOut(hdc, 20, 20, TEXT("Nenhum alerta ativo neste momento."), 34);
        }
        else {
            int y = 20;
            TCHAR linha[512];

            int indiceInicio = paginaAtual * maxAlertasVisiveis;
            int indiceFim = indiceInicio + maxAlertasVisiveis;
            if (indiceFim > totalAlertas) indiceFim = totalAlertas;

            _stprintf_s(linha, 512, TEXT("Alertas Ativos (Pagina %d de %d)"), paginaAtual + 1, (totalAlertas + maxAlertasVisiveis - 1) / maxAlertasVisiveis);
            TextOut(hdc, 20, y, linha, (int)_tcslen(linha));
            y += 30;

            for (int i = indiceInicio; i < indiceFim; i++) {
                _stprintf_s(linha, 512, TEXT("Placar ID: %d | Duracao: %d seg | Msg: '%s'"),
                    alertasAtivos[i].id, alertasAtivos[i].duracao, alertasAtivos[i].msg);
                TextOut(hdc, 20, y, linha, (int)_tcslen(linha));
                y += 25;
            }

            TextOut(hdc, 20, y + 20, TEXT("Use 'Page Up' e 'Page Down' para navegar."), 41);
        }

        ReleaseMutex(hMutexLocal);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_DESTROY:
        aplicacaoA_Correr = FALSE;
        // Acordar a thread de leitura para ela poder terminar
        if (hEventCentral) SetEvent(hEventCentral);
        WaitForSingleObject(hThreadComunicacao, 2000);

        CloseHandle(hThreadComunicacao);
        CloseHandle(hMutexLocal);
        DesligarRecursosSHM();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wcex;
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDR_MENU_PRINCIPAL);
    wcex.lpszClassName = TEXT("MonitorAlertasClass");

    if (!RegisterClassEx(&wcex)) return 1;

    HWND hWnd = CreateWindow(TEXT("MonitorAlertasClass"), TEXT("Monitor de Alertas - SO2"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 400,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) return 1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}