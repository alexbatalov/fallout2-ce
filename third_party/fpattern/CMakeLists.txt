include(FetchContent)

FetchContent_Declare(fpattern
    GIT_REPOSITORY "https://github.com/alexbatalov/fpattern"
    GIT_TAG "master"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fpattern)
