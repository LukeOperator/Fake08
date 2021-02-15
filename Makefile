# Project Name
TARGET = ex_Fake08

# Sources
CPP_SOURCES = Fake08.cpp

# Library Locations
LIBDAISY_DIR = ../../DaisyExamples/libdaisy
DAISYSP_DIR = ../../DaisyExamples/DaisySP

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
