cmake_minimum_required (VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_NAME hydra)
set(APP_LONG_NAME "Hydra")

find_package(fmt CONFIG REQUIRED)

file(GLOB core_src CONFIGURE_DEPENDS
    "*.cpp"
    "ui/*.cpp"
    "../common/str.cpp"
    "../common/config/config.cpp"
    "../common/win32/serial_port.cpp"
    "../common/win32/reg.cpp"
    "../common/win32/app.cpp"
    "../common/win32/kernel.cpp"
    "../common/win32/shell.cpp"
    "../common/win32/shell_notify_icon.cpp"
    "../common/win32/popup_menu.cpp"
    "../common/win32/user.cpp"
    "../common/win32/clipboard.cpp")
add_executable (${APP_NAME} WIN32 ${core_src} "main.cpp" ipkm.rc)

target_compile_definitions(${APP_NAME} PRIVATE
    APP_SHORT_NAME="${APP_NAME}"
    APP_LONG_NAME="${APP_LONG_NAME}"
    APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(${APP_NAME}
	fmt::fmt-header-only
    libgrey
)
target_include_directories(${APP_NAME} PRIVATE "../grey/grey" "../common")

target_sources(${APP_NAME} PRIVATE dpi-aware.manifest)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")

# include .pdb symbol generation
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND CMAKE_BUILD_TYPE MATCHES "Release")
   target_compile_options(${APP_NAME} PRIVATE /Zi)

   # Tell linker to include symbol data
    set_target_properties(${APP_NAME} PROPERTIES 
        LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
    )

    # Set file name & location
    set_target_properties(${APP_NAME} PROPERTIES 
        COMPILE_PDB_NAME ${APP_NAME} 
        COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
    )
endif()