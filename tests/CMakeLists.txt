# Each test file becomes its own executable and CTest test.
# They link only tetromino_engine (and tetromino_tui for the input decoder test),
# never the application.

set(TETROMINO_ENGINE_TESTS
    BoardTests
    CollisionTests
    RotationTests
    SpinTests
    ScoringTests
    PieceGeneratorTests
    GameTests
    BotTests
)

set(TETROMINO_TUI_TESTS
    InputTests
    ScreenBufferTests
    HighScoresTests
)

foreach(test_name IN LISTS TETROMINO_ENGINE_TESTS TETROMINO_TUI_TESTS)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp)
        continue()
    endif()
    add_executable(${test_name} ${test_name}.cpp TestMain.cpp)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(test_name IN_LIST TETROMINO_TUI_TESTS)
        target_link_libraries(${test_name} PRIVATE tetromino_tui tetromino_options)
    else()
        target_link_libraries(${test_name} PRIVATE tetromino_engine tetromino_options)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
