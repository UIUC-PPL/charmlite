add_custom_target(CharmLite)

set(LIB_FILES
    core
)

# get_property(CharmLiteAR VARIABLE PROPERTY CMAKE_AR)
# message(STATUS "${CharmLiteAR}")
# set(CMAKE_AR "${CHARMC_PATH}/bin/charmc")
add_library(charmlite OBJECT ${LIB_FILES})
target_include_directories(charmlite
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
# set(CMAKE_AR "${CharmLiteAR}")
