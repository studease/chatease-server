# CMake version 
cmake_minimum_required (VERSION 3.5)

#project name
PROJECT(chatease-server)

#head file path
INCLUDE_DIRECTORIES(src/cn/studease/core)

#source directory
AUX_SOURCE_DIRECTORY(src DIR_SRC)

#core src
ADD_SUBDIRECTORY(src/cn/studease/core)

#add executable file
ADD_EXECUTABLE(chatease-server ${DIR_SRC})

#add link library
TARGET_LINK_LIBRARIES(chatease-server core)
TARGET_LINK_LIBRARIES(chatease-server pthread)
TARGET_LINK_LIBRARIES(chatease-server m)
TARGET_LINK_LIBRARIES(chatease-server crypto)
