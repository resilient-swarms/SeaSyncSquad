CMAKE_MINIMUM_REQUIRED(VERSION 3.27)
PROJECT(seasyncsquad)

# ENABLE MULTI-THREADING
# --------------------------------------
OPTION(ENABLE_MULTI_THREADING "Enable multi-threading." ON) # Enabled by default
IF(NOT ENABLE_MULTI_THREADING)
    ADD_DEFINITIONS(-DDISABLE_MULTI_THREADING)
    MESSAGE(STATUS "Multi-threading disabled.")
ELSE()
    MESSAGE(STATUS "Multi-threading enabled.")
endif(NOT ENABLE_MULTI_THREADING)

# ENABLE TIME SYNC WHILE MULTI-THREADING
# --------------------------------------
OPTION(ENABLE_TIME_SYNC "Enable time sync between parallel threads." ON) # Enable by default
IF(ENABLE_MULTI_THREADING)
    IF(ENABLE_TIME_SYNC)
        ADD_DEFINITIONS(-DENABLE_TIME_SYNC)
        MESSAGE(STATUS "Time sync between threads enabled.")
    ELSE()
        MESSAGE(STATUS "Time sync between threads disabled.")
    ENDIF(ENABLE_TIME_SYNC)
ENDIF(ENABLE_MULTI_THREADING)

# INCLUDE HEADER FILES DIRECTORIES
# --------------------------------
INCLUDE_DIRECTORIES(
        include
        dependency/ASVLite/include
        dependency/matplotlib-cpp        
)

# SOURCE FILES
# ------------
SET( SOURCE
        source/main.cpp
        source/simulate_LOS.cpp
        dependency/ASVLite/source/errors.c
        dependency/ASVLite/source/geometry.c
        dependency/ASVLite/source/regular_wave.c
        dependency/ASVLite/source/sea_surface.c
        dependency/ASVLite/source/asv.c
)

# PREPROCESSOR MACROS
# -------------------
IF(WIN32)
    ADD_COMPILE_DEFINITIONS(WIN32)
ENDIF()

# CREATE BINARIES
# ---------------
ADD_EXECUTABLE(seasyncsquad ${SOURCE})

# LINK LIBRARIES
# --------------
FIND_PACKAGE(Threads REQUIRED)
FIND_PACKAGE(Python3 COMPONENTS Interpreter Development REQUIRED)
FIND_PACKAGE(Python3 COMPONENTS NumPy REQUIRED)
FIND_PACKAGE(fmt REQUIRED)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
TARGET_LINK_LIBRARIES(seasyncsquad
        m
        Threads::Threads
        Python3::Python
        Python3::Module
        Python3::NumPy
        fmt::fmt)