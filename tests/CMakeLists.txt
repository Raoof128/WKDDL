add_executable(payload_layout_test payload_layout_test.cpp)
target_link_libraries(payload_layout_test PRIVATE moduleinspector_common)

if(MSVC)
    target_compile_options(payload_layout_test PRIVATE /W4 /WX)
else()
    target_compile_options(payload_layout_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(payload_layout_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_test(NAME payload_layout_test COMMAND payload_layout_test)
