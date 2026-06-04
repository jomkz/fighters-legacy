# OpenAL Soft audio backend — implements IAudio
# On Apple, find_package(OpenAL) finds the deprecated system OpenAL.framework
# which provides <OpenAL/al.h>, not <AL/al.h>. Always use FetchContent on Apple
# to ensure we get OpenAL Soft with the correct include layout.
if(NOT (OPENAL_FOUND OR OpenAL_FOUND) OR APPLE)
    set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
    # OpenAL Soft 1.24.x uses its own LIBTYPE variable (not BUILD_SHARED_LIBS)
    # to select SHARED vs STATIC.  Default is SHARED when unset.
    set(LIBTYPE STATIC CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(openal-soft)
    unset(LIBTYPE CACHE)
    unset(CMAKE_COMPILE_WARNING_AS_ERROR)
endif()

add_library(platform-openal STATIC OALAudio.cpp OALAudioFactory.cpp)
target_link_libraries(platform-openal
    PUBLIC  platform-hal
    PRIVATE OpenAL::OpenAL
)
