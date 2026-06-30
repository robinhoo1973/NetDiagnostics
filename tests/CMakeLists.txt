find_package(Qt6 REQUIRED COMPONENTS Core Test Concurrent)

set(TEST_SOURCES
    test_engine_quick.cpp
    ${CMAKE_SOURCE_DIR}/src/models/DiagnosticResult.cpp
    ${CMAKE_SOURCE_DIR}/src/util/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/diagnostic/DiagnosticEngine.cpp
)

add_executable(test_engine_quick ${TEST_SOURCES})
target_include_directories(test_engine_quick PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_engine_quick PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Concurrent
    Qt6::Network
)
add_test(NAME engine_smoke COMMAND test_engine_quick)
