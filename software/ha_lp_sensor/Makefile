SDK_DEMO_PATH ?= .
BL_SDK_BASE ?= $(SDK_DEMO_PATH)/../../os/

export BL_SDK_BASE

NOW_REAL_PATH:= $(realpath ./)
BOARD_DIR = ${NOW_REAL_PATH}
BOARD ?= config_2M_board

CHIP ?= bl616

CROSS_COMPILE ?=$(PWD)/../../toolchain/bin/riscv64-unknown-elf-


# add custom cmake definition
#cmake_definition+=-Dxxx=sss

include $(BL_SDK_BASE)/project.build
