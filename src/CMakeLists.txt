add_custom_target(CharmLite)

set(LIB_FILES
    core
)

add_library(charmlite STATIC ${LIB_FILES})
target_include_directories(charmlite
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
