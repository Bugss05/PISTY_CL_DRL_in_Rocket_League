cmake_minimum_required (VERSION 3.8)

project("GigaLearnBot")

file(GLOB_RECURSE FILES_SRC "src/*.cpp" "src/*.h" "src/*.hpp")
add_executable(GigaLearnBot ${FILES_SRC})

# Set C++ version to 20
set_target_properties(GigaLearnBot PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(GigaLearnBot PROPERTIES CXX_STANDARD 20)

# Make sure GigaLearnCPP is going to build in the same directory as us
# Otherwise, we won't be able to import it at runtime
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

# Include RLGymSim_PPO
add_subdirectory(GigaLearnCPP)
target_link_libraries(GigaLearnBot GigaLearnCPP)

# Include RLBot
add_subdirectory(RLBotCPP)
target_link_libraries(GigaLearnBot RLBotCPP)

# --- Teste standalone da lógica do scheduler (sem torch/Learner) ---
# Build: cmake --build build --config Release --target scheduler_test
add_executable(scheduler_test "tests/scheduler_test.cpp")
target_include_directories(scheduler_test PRIVATE "src")
set_target_properties(scheduler_test PROPERTIES CXX_STANDARD 20 LINKER_LANGUAGE CXX)