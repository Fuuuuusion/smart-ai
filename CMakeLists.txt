cmake_minimum_required(VERSION 3.21)

project(SmartAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql Concurrent)
find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
    get_filename_component(_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_compiler_root "${_compiler_bin}/.." ABSOLUTE)
    find_path(ZLIB_INCLUDE_DIR zlib.h
        HINTS "${_compiler_root}/x86_64-w64-mingw32/include"
              "${_compiler_root}/include"
    )
    find_library(ZLIB_LIBRARY
        NAMES z zlib zlibstatic libz
        HINTS "${_compiler_root}/x86_64-w64-mingw32/lib"
              "${_compiler_root}/lib"
    )
    if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
        add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
        set_target_properties(ZLIB::ZLIB PROPERTIES
            IMPORTED_LOCATION "${ZLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "ZLIB was not found. Install zlib development files or set ZLIB_INCLUDE_DIR and ZLIB_LIBRARY.")
    endif()
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/ChatPage.cpp
    src/app/ChatPage.h
    src/app/VisionPage.cpp
    src/app/VisionPage.h
    src/app/KnowledgePage.cpp
    src/app/KnowledgePage.h
    src/app/AgentPage.cpp
    src/app/AgentPage.h
    src/app/SettingsDialog.cpp
    src/app/SettingsDialog.h
    src/app/MessageWidget.cpp
    src/app/MessageWidget.h
    src/core/ApiClient.cpp
    src/core/ApiClient.h
    src/core/AppSettings.cpp
    src/core/AppSettings.h
    src/core/ChatHistoryStore.cpp
    src/core/ChatHistoryStore.h
    src/core/KnowledgeStore.cpp
    src/core/KnowledgeStore.h
    src/core/DocumentExtractor.cpp
    src/core/DocumentExtractor.h
    src/core/ZipReader.cpp
    src/core/ZipReader.h
    src/core/PdfTextExtractor.cpp
    src/core/PdfTextExtractor.h
    src/core/AgentExecutor.cpp
    src/core/AgentExecutor.h
    src/core/ToolRegistry.cpp
    src/core/ToolRegistry.h
    src/core/LocalEmbedding.cpp
    src/core/LocalEmbedding.h
    src/core/MathParser.cpp
    src/core/MathParser.h
    resources/resources.qrc
)

add_executable(SmartAI WIN32 ${PROJECT_SOURCES})

target_include_directories(SmartAI PRIVATE src)

target_link_libraries(SmartAI PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    ZLIB::ZLIB
)

if(WIN32)
    target_compile_definitions(SmartAI PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    set_target_properties(SmartAI PROPERTIES MACOSX_BUNDLE TRUE)
endif()

install(TARGETS SmartAI
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
