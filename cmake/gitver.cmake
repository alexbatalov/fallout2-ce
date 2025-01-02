# Get author name
execute_process(
    COMMAND git log -1 --pretty=format:%an
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE AUTHOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get current branch
execute_process(
    COMMAND git branch --show-current
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get hash of the latest commit
execute_process(
    COMMAND git log --pretty=format:%h -n 1
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get last tag
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE LATEST_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get date of the latest commit
execute_process(
    COMMAND git log -1 --date=format:"%b %d %Y %H:%M:%S" --pretty=format:%cd
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Define a variable for CI_BUILD
set(CI_BUILD 0)

# Create git_version.h file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/git_version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/git_version.h
)
