cmake_minimum_required(VERSION 3.20)
project(SO2_Placares_SHM C)

set(CMAKE_C_STANDARD 11)

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)

    # 1. Executável Servidor Central (Consola)
    add_executable(central central/central.c)

    # 2. Executável Cliente Placar (Consola)
    add_executable(placar placar/placar.c)

    # 3. Executável Monitor Gráfico Win32 GUI
    add_executable(monitor WIN32 monitor/monitor.c monitor/monitor.rc)
    target_link_libraries(monitor comctl32)
endif()
