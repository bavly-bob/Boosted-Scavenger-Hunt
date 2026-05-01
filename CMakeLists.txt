cmake_minimum_required(VERSION 3.16)

project(ScavengerHunt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus /permissive-)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia)
find_package(Boost 1.89.0 REQUIRED CONFIG)

set(PROJECT_SOURCES
        main.cpp
        ui/GameWindow.cpp
        ui/GameWindow.h
        ui/PauseOverlay.cpp
        ui/PauseOverlay.h
        ui/GameOverOverlay.cpp
        ui/GameOverOverlay.h
        ui/MainMenu.cpp
        ui/MainMenu.h
        core/include/Game.h core/src/Game.cpp
        core/include/GameObject.h core/src/GameObject.cpp
        core/include/Level.h core/src/Level.cpp
        gameplay/include/Player.h gameplay/src/Player.cpp
        gameplay/include/PlayerAnimator.h gameplay/src/PlayerAnimator.cpp
        gameplay/include/Enemy.h gameplay/src/Enemy.cpp
        gameplay/include/Coin.h gameplay/src/Coin.cpp
        gameplay/include/Wall.h gameplay/src/Wall.cpp
        gameplay/include/TriggerWall.h gameplay/src/TriggerWall.cpp
        gameplay/include/HiddenWall.h gameplay/src/HiddenWall.cpp
        gameplay/include/TreasureRoom.h gameplay/src/TreasureRoom.cpp
        gameplay/include/ClueTrigger.h gameplay/src/ClueTrigger.cpp
        systems/include/LevelLoader.h systems/src/LevelLoader.cpp
        systems/include/DifficultyConfig.h systems/src/DifficultyConfig.cpp
        systems/include/ClueManager.h systems/src/ClueManager.cpp
        systems/include/InputHandler.h systems/src/InputHandler.cpp
        systems/include/SpriteManager.h systems/src/SpriteManager.cpp
        networking/include/types.hpp
        networking/include/connection.hpp networking/src/connection.cpp
        networking/include/protocol.hpp networking/src/protocol.cpp
        networking/include/client.hpp networking/src/client.cpp
        networking/include/server.hpp networking/src/server.cpp
        core/include/AIHelper.h core/src/AIHelper.cpp
        utils/Enums.h
        core/include/Chamber.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ScavengerHunt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(ScavengerHunt SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(ScavengerHunt
            ${PROJECT_SOURCES}
        )
    endif()
endif()

if(WIN32)
    target_link_libraries(ScavengerHunt PRIVATE ws2_32 mswsock)
endif()

target_link_libraries(ScavengerHunt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia Boost::headers)
target_include_directories(ScavengerHunt PRIVATE . core/include gameplay/include systems/include networking/include utils ui)

add_custom_command(TARGET ScavengerHunt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ScavengerHunt>/levels"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/levels" "$<TARGET_FILE_DIR:ScavengerHunt>/levels"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ScavengerHunt>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ScavengerHunt>/assets"
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ScavengerHunt)
endif()
set_target_properties(ScavengerHunt PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ScavengerHunt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ScavengerHunt)
endif()

enable_testing()

add_executable(test_game
    tests/test_game.cpp
    systems/src/DifficultyConfig.cpp
)
set_target_properties(test_game PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(test_game PRIVATE . core/include gameplay/include systems/include networking/include utils ui)
target_link_libraries(test_game PRIVATE Boost::headers Qt${QT_VERSION_MAJOR}::Core)

add_executable(test_player
    tests/test_player.cpp
)
set_target_properties(test_player PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(test_player PRIVATE . core/include gameplay/include systems/include networking/include utils ui)
target_link_libraries(test_player PRIVATE Boost::headers Qt${QT_VERSION_MAJOR}::Core)

add_executable(test_score
    tests/test_score.cpp
    gameplay/src/Coin.cpp
    core/src/GameObject.cpp
)
set_target_properties(test_score PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(test_score PRIVATE . core/include gameplay/include systems/include networking/include utils ui)
target_link_libraries(test_score PRIVATE Boost::headers Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)

add_executable(test_network
    tests/test_network.cpp
    networking/src/protocol.cpp
)
set_target_properties(test_network PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(test_network PRIVATE . core/include gameplay/include systems/include networking/include utils ui)
target_link_libraries(test_network PRIVATE Boost::headers Qt${QT_VERSION_MAJOR}::Core)

add_executable(test_ai_connection
    tests/test_ai_connection.cpp
    core/src/AIHelper.cpp
)
set_target_properties(test_ai_connection PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(test_ai_connection PRIVATE . core/include gameplay/include systems/include networking/include utils ui)
target_link_libraries(test_ai_connection PRIVATE Boost::headers Qt${QT_VERSION_MAJOR}::Core)

if(WIN32)
    target_link_libraries(test_network PRIVATE ws2_32 mswsock)
    target_link_libraries(test_ai_connection PRIVATE ws2_32 mswsock)
endif()

add_test(NAME test_game COMMAND test_game)
add_test(NAME test_player COMMAND test_player)
add_test(NAME test_score COMMAND test_score)
add_test(NAME test_network COMMAND test_network)
add_test(NAME test_ai_connection COMMAND test_ai_connection)

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_game test_player test_score test_network test_ai_connection
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
