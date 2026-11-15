cmake_minimum_required(VERSION 3.20)
if(TARGET libultraship)
    add_executable(OotmmSavePairTests OotmmSavePairTests.cpp)
    target_compile_features(OotmmSavePairTests PRIVATE cxx_std_20)
    target_link_libraries(OotmmSavePairTests PRIVATE libultraship)
    add_test(NAME OotmmSavePairTests COMMAND OotmmSavePairTests "${CMAKE_CURRENT_BINARY_DIR}/pair-evidence/$<CONFIG>")
    return()
endif()
project(OotmmLibultrashipTests LANGUAGES CXX)

find_package(nlohmann_json CONFIG REQUIRED)
enable_testing()

add_executable(AtomicFileTests AtomicFileTests.cpp ../src/ship/utils/AtomicFile.cpp)
target_compile_features(AtomicFileTests PRIVATE cxx_std_20)
target_include_directories(AtomicFileTests PRIVATE ../include)
add_test(NAME AtomicFileTests COMMAND AtomicFileTests "${CMAKE_CURRENT_BINARY_DIR}/atomic-evidence")

add_executable(OotmmSavePairTests OotmmSavePairTests.cpp ../src/ship/utils/AtomicFile.cpp)
target_compile_features(OotmmSavePairTests PRIVATE cxx_std_20)
target_include_directories(OotmmSavePairTests PRIVATE ../include)
target_link_libraries(OotmmSavePairTests PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME OotmmSavePairTests COMMAND OotmmSavePairTests "${CMAKE_CURRENT_BINARY_DIR}/pair-evidence/$<CONFIG>")

if(WIN32)
    add_executable(GameIpcSessionTests GameIpcSessionTests.cpp
        ../src/ship/GameIpcSession.cpp ../src/ship/IpcClient.cpp
        ../src/ship/OotmmInventory.cpp ../src/ship/OotmmPresence.cpp ../src/ship/OotmmItemGrant.cpp)
    target_compile_features(GameIpcSessionTests PRIVATE cxx_std_20)
    target_compile_definitions(GameIpcSessionTests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(GameIpcSessionTests PRIVATE ../include)
    target_link_libraries(GameIpcSessionTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME GameIpcSessionTests COMMAND GameIpcSessionTests
        "${CMAKE_CURRENT_BINARY_DIR}/ipc-evidence/$<CONFIG>")
endif()

add_executable(OotmmDialogTests OotmmDialogTests.cpp ../src/ship/OotmmDialog.cpp)
target_compile_features(OotmmDialogTests PRIVATE cxx_std_20)
target_include_directories(OotmmDialogTests PRIVATE ../include)
add_test(NAME OotmmDialogTests COMMAND OotmmDialogTests)

add_executable(OotmmSaveRecoveryTests OotmmSaveRecoveryTests.cpp ../src/ship/utils/AtomicFile.cpp)
target_compile_features(OotmmSaveRecoveryTests PRIVATE cxx_std_20)
target_include_directories(OotmmSaveRecoveryTests PRIVATE ../include)
add_test(NAME OotmmSaveRecoveryTests COMMAND OotmmSaveRecoveryTests "${CMAKE_CURRENT_BINARY_DIR}/recovery-evidence/$<CONFIG>")
