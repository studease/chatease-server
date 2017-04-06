################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include src/cn/studease/core/subdir.mk
-include src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: chatease-server

# Tool invocations
chatease-server: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L/usr/local/ssl/lib -L/usr/local/lua-5.3.3_Linux35_64_lib -o "chatease-server" $(OBJS) $(USER_OBJS) $(LIBS) -pthread -lm -march=x86-64
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) chatease-server
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
