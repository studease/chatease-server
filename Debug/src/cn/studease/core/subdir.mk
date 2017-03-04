################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/cn/studease/core/stu_alloc.c \
../src/cn/studease/core/stu_base64.c \
../src/cn/studease/core/stu_channel.c \
../src/cn/studease/core/stu_connection.c \
../src/cn/studease/core/stu_cycle.c \
../src/cn/studease/core/stu_errno.c \
../src/cn/studease/core/stu_event.c \
../src/cn/studease/core/stu_filedes.c \
../src/cn/studease/core/stu_hash.c \
../src/cn/studease/core/stu_http.c \
../src/cn/studease/core/stu_http_parse.c \
../src/cn/studease/core/stu_http_request.c \
../src/cn/studease/core/stu_json.c \
../src/cn/studease/core/stu_list.c \
../src/cn/studease/core/stu_log.c \
../src/cn/studease/core/stu_lua.c \
../src/cn/studease/core/stu_palloc.c \
../src/cn/studease/core/stu_process.c \
../src/cn/studease/core/stu_ram.c \
../src/cn/studease/core/stu_shmem.c \
../src/cn/studease/core/stu_slab.c \
../src/cn/studease/core/stu_socket.c \
../src/cn/studease/core/stu_spinlock.c \
../src/cn/studease/core/stu_string.c \
../src/cn/studease/core/stu_thread.c \
../src/cn/studease/core/stu_user.c \
../src/cn/studease/core/stu_utils.c \
../src/cn/studease/core/stu_websocket_parse.c \
../src/cn/studease/core/stu_websocket_request.c 

OBJS += \
./src/cn/studease/core/stu_alloc.o \
./src/cn/studease/core/stu_base64.o \
./src/cn/studease/core/stu_channel.o \
./src/cn/studease/core/stu_connection.o \
./src/cn/studease/core/stu_cycle.o \
./src/cn/studease/core/stu_errno.o \
./src/cn/studease/core/stu_event.o \
./src/cn/studease/core/stu_filedes.o \
./src/cn/studease/core/stu_hash.o \
./src/cn/studease/core/stu_http.o \
./src/cn/studease/core/stu_http_parse.o \
./src/cn/studease/core/stu_http_request.o \
./src/cn/studease/core/stu_json.o \
./src/cn/studease/core/stu_list.o \
./src/cn/studease/core/stu_log.o \
./src/cn/studease/core/stu_lua.o \
./src/cn/studease/core/stu_palloc.o \
./src/cn/studease/core/stu_process.o \
./src/cn/studease/core/stu_ram.o \
./src/cn/studease/core/stu_shmem.o \
./src/cn/studease/core/stu_slab.o \
./src/cn/studease/core/stu_socket.o \
./src/cn/studease/core/stu_spinlock.o \
./src/cn/studease/core/stu_string.o \
./src/cn/studease/core/stu_thread.o \
./src/cn/studease/core/stu_user.o \
./src/cn/studease/core/stu_utils.o \
./src/cn/studease/core/stu_websocket_parse.o \
./src/cn/studease/core/stu_websocket_request.o 

C_DEPS += \
./src/cn/studease/core/stu_alloc.d \
./src/cn/studease/core/stu_base64.d \
./src/cn/studease/core/stu_channel.d \
./src/cn/studease/core/stu_connection.d \
./src/cn/studease/core/stu_cycle.d \
./src/cn/studease/core/stu_errno.d \
./src/cn/studease/core/stu_event.d \
./src/cn/studease/core/stu_filedes.d \
./src/cn/studease/core/stu_hash.d \
./src/cn/studease/core/stu_http.d \
./src/cn/studease/core/stu_http_parse.d \
./src/cn/studease/core/stu_http_request.d \
./src/cn/studease/core/stu_json.d \
./src/cn/studease/core/stu_list.d \
./src/cn/studease/core/stu_log.d \
./src/cn/studease/core/stu_lua.d \
./src/cn/studease/core/stu_palloc.d \
./src/cn/studease/core/stu_process.d \
./src/cn/studease/core/stu_ram.d \
./src/cn/studease/core/stu_shmem.d \
./src/cn/studease/core/stu_slab.d \
./src/cn/studease/core/stu_socket.d \
./src/cn/studease/core/stu_spinlock.d \
./src/cn/studease/core/stu_string.d \
./src/cn/studease/core/stu_thread.d \
./src/cn/studease/core/stu_user.d \
./src/cn/studease/core/stu_utils.d \
./src/cn/studease/core/stu_websocket_parse.d \
./src/cn/studease/core/stu_websocket_request.d 


# Each subdirectory must supply rules for building sources it contributes
src/cn/studease/core/%.o: ../src/cn/studease/core/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DCONFIG_SMP=1 -I/usr/local/ssl/include -I/usr/local/lua-5.3.3_Linux35_64_lib/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<" -pthread -lm -march=x86-64
	@echo 'Finished building: $<'
	@echo ' '


