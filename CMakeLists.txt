# CMake version 
cmake_minimum_required (VERSION 3.5)

#project info
project (Chatease)

#src file
aux_source_directory(src DIR_SRC)

#header file
include_directories(src/cn/studease/core)

#core src
add_subdirectory(src/cn/studease/core)

#build
add_executable(chatease-server ${DIR_SRC})

#link
target_link_libraries(chatease-server core)
target_link_libraries(chatease-server pthread)
target_link_libraries(chatease-server m)
target_link_libraries(chatease-server crypto)
