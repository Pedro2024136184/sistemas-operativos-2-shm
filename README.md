# Plataforma Concorrente de Placares em Tempo Real - SO2 (2026)

![C](https://img.shields.io/badge/C-Win32_API-00599C?style=for-the-badge&logo=c&logoColor=white)
![Windows](https://img.shields.io/badge/OS-Windows_Kernel-0078D7?style=for-the-badge&logo=windows&logoColor=white)
![Multithreading](https://img.shields.io/badge/IPC-Shared_Memory_&_Events-success?style=for-the-badge)
![Academic](https://img.shields.io/badge/ISEC-LEI-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Versão-Melhorada-success?style=for-the-badge)

---

## 📌 Enquadramento Académico
* **Cadeira:** Sistemas Operativos 2 (SO2 SHM)
* **Ano Letivo / Realização:** 2026
* **Instituição:** Instituto Superior de Engenharia de Coimbra (ISEC)
* **Curso:** Licenciatura em Engenharia Informática

---

## 🏗️ Arquitetura do Sistema e IPC
O projeto consiste numa plataforma distribuída em ambiente nativo **Windows (Win32 API)** concebida para a difusão, controlo e monitorização concorrente de mensagens informativas e alertas através de múltiplos placares virtuais em tempo real.

```text
               ┌───────────────┐
               │    Placar     │ (Emissor de mensagens / alertas)
               └───────┬───────┘
                       │ (Named Pipes / Eventos)
                       ▼
               ┌───────────────┐
               │    Central    │ (Servidor de Coordenação & Memória Partilhada)
               └───────┬───────┘
                       │
       ┌───────────────┴───────────────┐
       ▼                               ▼
┌───────────────┐               ┌───────────────┐
│  Monitor GUI  │               │  Monitor GUI  │ (Clientes de visualização Win32)
└───────────────┘               └───────────────┘
```

### Componentes:
1. **Central (`central`):**
   * Servidor coordenador que gere o segmento de **Memória Partilhada (Shared Memory / SHM)** via `CreateFileMapping` e `MapViewOfFile`.
   * Comunicação ponto-a-ponto com os placares através de instâncias de **Named Pipes** (`CreateNamedPipe`).
   * Sincronização concorrente livre de *data races* com **Mutexes de Sistema** (`CreateMutex`) e sinalização a múltiplos monitores via **Eventos Win32** (`CreateEvent`, `SetEvent`).
2. **Monitor Gráfico (`monitor`):**
   * Interface gráfica desenvolvida diretamente com a **Win32 API** pura (sem recurso a MFC ou frameworks externas).
   * Mapeamento em modo leitura da SHM, menus nativos (*Configuração*, *Acerca*, *Sair*), caixas de diálogo modais e paginação de múltiplos alertas via teclas **Page Up / Page Down**.
3. **Placar (`placar`):**
   * Emissor interativo de alertas e anúncios com temporização de exibição e cancelamento dinâmico.

---

## 🚀 Comparação de Versões: Original vs. Versão Melhorada

| Componente / Mecanismo | Versão Original (Ramo `main`) | Versão Melhorada (Ramo `versao-melhorada`) |
| :--- | :--- | :--- |
| **Sincronização de Notificação SHM** | No servidor `central`, a notificação usava `SetEvent()`, seguida de um atraso fixo arbitrário `Sleep(20)` e `ResetEvent()`. Caso algum processo monitor sofresse atraso de escalonamento superior a 20ms, o evento era perdido (*race condition*). | Introduzido um **contador sequencial de versão (`DWORD versao`)** na estrutura `SHM_ALERTA`. O evento é mantido ativo pelo tempo seguro e os monitores validam deterministicamente se há dados novos através da comparação de versões. |
| **Integridade dos Headers do Protocolo** | O ficheiro `protocolo.h` possuía um erro de sintaxe na última linha (`#endif#pragma once#pragma once`), o que gerava avisos em compiladores rigorosos. | **Correção formal de sintaxe**: diretivas de pré-processador padronizadas com guardas `#ifndef PROTOCOLO_H` consistentes nos três módulos (`central`, `monitor`, `placar`). |
| **Constantes e Canais de Comunicação** | Definição fragmentada de nomes de canais de comunicação com strings soltas no código. | **Constantes centralizadas**: inclusão de `NOME_PIPE_BASE`, `NOME_PIPE_CENTRAL`, `NOME_SHM`, `NOME_EVENTO` e `NOME_MUTEX_SHM` unificados no protocolo. |
| **Sistema de Compilação Multi-Ambiente** | Apenas soluções e projetos do Visual Studio 2022 (`.slnx`, `.vcxproj`), dificultando compilação por linha de comandos ou em pipelines CI/CD. | Adicionado **`CMakeLists.txt` unificado**, permitindo compilação automatizada via Visual Studio, CMake ou compiladores MinGW-w64. |

---

## 📂 Organização dos Módulos

```text
sistemas-operativos-2-shm/
├── central/           # Servidor coordenador, gestão de SHM e Named Pipes
│   ├── central.c
│   └── protocolo.h
├── monitor/           # Aplicação gráfica Win32 GUI com paginação
│   ├── monitor.c
│   ├── monitor.rc     # Ficheiro de recursos (menus e diálogos)
│   ├── resource.h
│   └── protocolo.h
├── placar/            # Emissor de alertas e mensagens
│   ├── placar.c
│   └── protocolo.h
├── CMakeLists.txt     # Script CMake multiplataforma para Windows
└── README.md          # Documentação completa
```

---

## 💻 Como Compilar e Executar no Windows

1. **Clonar e aceder ao repositório:**
   ```cmd
   git clone https://github.com/Pedro2024136184/sistemas-operativos-2-shm.git
   cd sistemas-operativos-2-shm
   ```

2. **Alternar entre as versões:**
   * Para a versão original:
     ```cmd
     git checkout main
     ```
   * Para a versão melhorada:
     ```cmd
     git checkout versao-melhorada
     ```

3. **Compilar com o Visual Studio:**
   * Abra a solução no Visual Studio e faça **Build Solution (F7)**.

4. **Ou compilar com CMake / Ninja:**
   ```cmd
   cmake -B build
   cmake --build build --config Release
   ```

5. **Ordem de Execução:**
   1. Executar `central.exe` (Servidor coordenador).
   2. Executar uma ou mais instâncias de `monitor.exe` (Interface gráfica de placares).
   3. Executar instâncias de `placar.exe` para emitir mensagens e alertas.

---

## 👨‍💻 Autor
* **Pedro Costa** (Nº 2024136184) – Licenciatura em Engenharia Informática, ISEC.
