################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/chatease-server.c 

OBJS += \
./src/chatease-server.o 

C_DEPS += \
./src/chatease-server.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DCONFIG_SMP=1 -I/usr/local/ssl/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<" -pthread -lm -march=x86-64
	@echo 'Finished building: $<'
	@echo ' '


