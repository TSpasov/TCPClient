cmake_minimum_required(VERSION 3.10)

# Set project name and C++ standard
project(Pos VERSION 1.0 LANGUAGES CXX)

# Use C++17 or later
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(SQLite3 REQUIRED)

# Add the executable
add_executable(posgw src/main.cpp)

target_link_libraries(posgw PRIVATE ${SQLite3_LIBRARIES})

