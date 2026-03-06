cmake_minimum_required(VERSION 3.16)
project(scxctl-manager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(scxctl-manager
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(scxctl-manager PRIVATE Qt6::Core Qt6::Widgets)

install(TARGETS scxctl-manager DESTINATION bin)
install(FILES scxctl-manager.desktop DESTINATION share/applications)
