################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../main.cpp \
../ping.cpp \
../simpleLogger.cpp 

OBJS += \
./main.o \
./ping.o \
./simpleLogger.o 

CPP_DEPS += \
./main.d \
./ping.d \
./simpleLogger.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabihf-g++ -DBOOST_LOG_DYN_LINK -I"/home/vlad/boost/boost_1_63_0" -I"/home/vlad/boost/boost_1_63_0/libs/asio/example/cpp03/icmp" -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


