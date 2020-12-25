cmake_minimum_required(VERSION 3.17)
project(extAPI)

set(CMAKE_CXX_STANDARD 17)

add_executable(extAPI main.cpp)

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/whoshuu/cpr.git GIT_TAG c8d33915dbd88ad6c92b258869b03aba06587ff9) # the commit hash for 1.5.0
FetchContent_MakeAvailable(cpr)
target_link_libraries(extAPI PRIVATE cpr::cpr)

include_directories(include/)
add_subdirectory(yaml-cpp)
target_link_libraries(extAPI PRIVATE mysqlcppconn)
target_link_libraries(extAPI PRIVATE yaml-cpp)

