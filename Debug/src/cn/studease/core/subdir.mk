################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/cn/studease/core/stu_alloc.c \
../src/cn/studease/core/stu_connection.c \
../src/cn/studease/core/stu_cycle.c \
../src/cn/studease/core/stu_errno.c \
../src/cn/studease/core/stu_event.c \
../src/cn/studease/core/stu_filedes.c \
../src/cn/studease/core/stu_hash.c \
../src/cn/studease/core/stu_list.c \
../src/cn/studease/core/stu_log.c \
../src/cn/studease/core/stu_palloc.c \
../src/cn/studease/core/stu_process.c \
../src/cn/studease/core/stu_ram.c \
../src/cn/studease/core/stu_request.c \
../src/cn/studease/core/stu_shmem.c \
../src/cn/studease/core/stu_slab.c \
../src/cn/studease/core/stu_socket.c \
../src/cn/studease/core/stu_spinlock.c \
../src/cn/studease/core/stu_string.c \
../src/cn/studease/core/stu_thread.c \
../src/cn/studease/core/stu_user.c \
../src/cn/studease/core/stu_utils.c \
../src/cn/studease/core/stu_websocket.c 

OBJS += \
./src/cn/studease/core/stu_alloc.o \
./src/cn/studease/core/stu_connection.o \
./src/cn/studease/core/stu_cycle.o \
./src/cn/studease/core/stu_errno.o \
./src/cn/studease/core/stu_event.o \
./src/cn/studease/core/stu_filedes.o \
./src/cn/studease/core/stu_hash.o \
./src/cn/studease/core/stu_list.o \
./src/cn/studease/core/stu_log.o \
./src/cn/studease/core/stu_palloc.o \
./src/cn/studease/core/stu_process.o \
./src/cn/studease/core/stu_ram.o \
./src/cn/studease/core/stu_request.o \
./src/cn/studease/core/stu_shmem.o \
./src/cn/studease/core/stu_slab.o \
./src/cn/studease/core/stu_socket.o \
./src/cn/studease/core/stu_spinlock.o \
./src/cn/studease/core/stu_string.o \
./src/cn/studease/core/stu_thread.o \
./src/cn/studease/core/stu_user.o \
./src/cn/studease/core/stu_utils.o \
./src/cn/studease/core/stu_websocket.o 

C_DEPS += \
./src/cn/studease/core/stu_alloc.d \
./src/cn/studease/core/stu_connection.d \
./src/cn/studease/core/stu_cycle.d \
./src/cn/studease/core/stu_errno.d \
./src/cn/studease/core/stu_event.d \
./src/cn/studease/core/stu_filedes.d \
./src/cn/studease/core/stu_hash.d \
./src/cn/studease/core/stu_list.d \
./src/cn/studease/core/stu_log.d \
./src/cn/studease/core/stu_palloc.d \
./src/cn/studease/core/stu_process.d \
./src/cn/studease/core/stu_ram.d \
./src/cn/studease/core/stu_request.d \
./src/cn/studease/core/stu_shmem.d \
./src/cn/studease/core/stu_slab.d \
./src/cn/studease/core/stu_socket.d \
./src/cn/studease/core/stu_spinlock.d \
./src/cn/studease/core/stu_string.d \
./src/cn/studease/core/stu_thread.d \
./src/cn/studease/core/stu_user.d \
./src/cn/studease/core/stu_utils.d \
./src/cn/studease/core/stu_websocket.d 


# Each subdirectory must supply rules for building sources it contributes
src/cn/studease/core/%.o: ../src/cn/studease/core/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DCONFIG_SMP=1 -I/usr/include -I/usr/lib/gcc/x86_64-redhat-linux/4.8.5/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<" -pthread -march=x86-64
	@echo 'Finished building: $<'
	@echo ' '


