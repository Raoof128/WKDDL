set(COMMON_COMPILE_WARNING_FLAGS "")
if(MSVC)
    set(COMMON_COMPILE_WARNING_FLAGS /W4 /WX)
else()
    set(COMMON_COMPILE_WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

if(WIN32 AND ENABLE_CLIENT)
    add_executable(ModuleInspectorClient client/Client.cpp)
    target_link_libraries(ModuleInspectorClient PRIVATE moduleinspector_common)
    target_compile_definitions(ModuleInspectorClient PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0A00)
    target_compile_options(ModuleInspectorClient PRIVATE ${COMMON_COMPILE_WARNING_FLAGS})
    target_link_libraries(ModuleInspectorClient PRIVATE wintrust crypt32)
    target_include_directories(ModuleInspectorClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
endif()

if(ENABLE_DRIVER)
    if(NOT WIN32)
        message(FATAL_ERROR "Kernel driver build requires a Windows host with the WDK installed.")
    endif()

    if(DEFINED ENV{WDK_INCLUDE_PATH} AND DEFINED ENV{WDK_LIB_PATH})
        add_library(ModuleInspectorDriver SHARED driver/Driver.c)
        target_include_directories(ModuleInspectorDriver PRIVATE $ENV{WDK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/../include)
        target_compile_definitions(ModuleInspectorDriver PRIVATE _KERNEL_MODE)
        target_compile_options(ModuleInspectorDriver PRIVATE ${COMMON_COMPILE_WARNING_FLAGS})
        target_link_directories(ModuleInspectorDriver PRIVATE $ENV{WDK_LIB_PATH})
        target_link_libraries(ModuleInspectorDriver PRIVATE ntoskrnl.lib aux_klib.lib)
        set_target_properties(ModuleInspectorDriver PROPERTIES
            OUTPUT_NAME "ModuleInspector"
            PREFIX ""
            SUFFIX ".sys"
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/driver
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/driver
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/driver
            LINK_FLAGS "/NODEFAULTLIB /SUBSYSTEM:NATIVE /DRIVER /ENTRY:DriverEntry"
        )
    else()
        message(WARNING "WDK_INCLUDE_PATH and WDK_LIB_PATH are not set. Skipping driver build target.")
    endif()
endif()
