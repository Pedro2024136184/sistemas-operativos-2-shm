# Plataforma Concorrente de Placares em Tempo Real - SO2

![C](https://img.shields.io/badge/C-Win32_API-00599C?style=for-the-badge&logo=c&logoColor=white)
![Windows](https://img.shields.io/badge/OS-Windows_Kernel-0078D7?style=for-the-badge&logo=windows&logoColor=white)
![Multithreading](https://img.shields.io/badge/IPC-Shared_Memory_&_Events-success?style=for-the-badge)
![Academic](https://img.shields.io/badge/ISEC-LEI-blue?style=for-the-badge)

Trabalho prático desenvolvido no âmbito da unidade curricular de **Sistemas Operativos 2 (SO2)** da Licenciatura em Engenharia Informática no **Instituto Superior de Engenharia de Coimbra (ISEC)**.

O projeto consiste numa plataforma distribuída em ambiente **Windows** para difusão e monitorização em tempo real de mensagens informativas e alertas através de placares virtuais, utilizando mecanismos nativos de comunicação entre processos (IPC), sincronização e interface gráfica Win32.

---

## 🏗️ Arquitetura do Sistema

A solução é composta por três módulos principais totalmente desacoplados:

```
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

1. **Central (`central`):**
   - Núcleo gestor do sistema.
   - Cria e gere o segmento de **Memória Partilhada (Shared Memory / SHM)** via `CreateFileMapping` e `MapViewOfFile`.
   - Coordena o acesso concorrente aos dados partilhados através de **Mutexes** e notifica os monitores via **Eventos Win32 (`CreateEvent`, `SetEvent`)** sem recurso a técnicas ineficientes de *polling*.

2. **Monitor (`monitor`):**
   - Aplicação com interface gráfica desenvolvida diretamente com a **Win32 API** (sem frameworks externas).
   - Renderização dos placares ativos, identificador, mensagem atual e tempo de permanência na área de cliente.
   - Menus nativos de aplicação: *Configuração*, *Acerca* e *Sair* (com caixas de diálogo modais `DialogBox` e `MessageBox`).
   - Paginação dinâmica e intuitiva de múltiplos alertas recorrendo à captura dos eventos de teclado **Page Up** e **Page Down**.

3. **Placar (`placar`):**
   - Processo cliente que emite mensagens e alertas dinâmicos para a Central.

---

## ✨ Destaques de Implementação

- **Zero-Polling:** Toda a sincronização e atualização de dados entre os processos baseia-se exclusivamente em primitivas de sincronização do kernel do Windows (`WaitForMultipleObjects`, `CreateMutex`, `CreateEvent`).
- **Gestão Limpa de Recursos:** Ao encerrar qualquer um dos componentes ou a plataforma global, todos os *handles* do sistema operativo (`CloseHandle`), mapas de memória (`UnmapViewOfFile`) e recursos gráficos são integralmente libertados.
- **Multithreading Nativo:** Separação entre a thread de desenho da interface e as threads de receção de dados de sincronização.

---

## 🛠️ Tecnologias Utilizadas

- **Linguagem:** C (C11 / C99)
- **API do Sistema Operativo:** Microsoft Windows Win32 API
- **Mecanismos de Concorrência & IPC:**
  - Shared Memory (`CreateFileMapping`, `MapViewOfFile`)
  - Mutexes (`CreateMutex`, `WaitForSingleObject`, `ReleaseMutex`)
  - Eventos (`CreateEvent`, `SetEvent`, `ResetEvent`)
- **Compilador / IDE:** Microsoft Visual C++ (MSVC) / Visual Studio

---

## 🚀 Como Compilar e Executar

### Pré-requisitos
- Microsoft Visual Studio 2019 ou 2022 com suporte a **Desenvolvimento para desktop com C++**.

### Instruções

1. **Clonar o repositório:**
   ```bash
   git clone https://github.com/Pedro2024136184/sistemas-operativos-2-shm.git
   cd sistemas-operativos-2-shm
   ```

2. **Compilação no Visual Studio:**
   - Abra a solução do módulo que pretende compilar (`central/central.slnx` ou `monitor/monitor.slnx` ou `placar/placar.slnx`).
   - Selecione a configuração `x64` e `Release` (ou `Debug`).
   - Compile a solução (`Ctrl + Shift + B`).

3. **Execução:**
   - Inicie primeiramente a **Central**: `central.exe`
   - Inicie uma ou mais instâncias do **Monitor**: `monitor.exe`
   - Inicie instâncias do **Placar** para enviar mensagens: `placar.exe`

---

## 👨‍💻 Autor

- **Pedro Costa** (Nº 2024136184) – Licenciatura em Engenharia Informática, ISEC
