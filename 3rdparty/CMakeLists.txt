include(FetchContent)

set(DEP_NAMES )

# Fetch nlohmann_json
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz
)
list(APPEND DEP_NAMES nlohmann_json)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    URL "${CMAKE_CURRENT_SOURCE_DIR}/spdlog-1.8.5.zip"
    #GIT_REPOSITORY https://github.com/gabime/spdlog.git
    #GIT_TAG        v1.8.5
    #GIT_PROGRESS   ON
    #GIT_SHALLOW    ON
    #UPDATE_DISCONNECTED ON
    #LOG_DOWNLOAD ON
    #LOG_CONFIGURE ON
    #LOG_BUILD ON
)
list(APPEND DEP_NAMES spdlog)

FetchContent_MakeAvailable(${DEP_NAMES})