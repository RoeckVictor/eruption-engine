# BundleCMake.cmake - Downloads and extracts CMake for bundling with the editor
#
# This script is executed during build to download CMake if not already present.
# Usage: cmake -DOUTPUT_DIR=<path> -P BundleCMake.cmake

# CMake version to bundle
set(CMAKE_BUNDLE_VERSION "3.28.1")
set(CMAKE_BUNDLE_PLATFORM "windows-x86_64")
set(CMAKE_BUNDLE_FILENAME "cmake-${CMAKE_BUNDLE_VERSION}-${CMAKE_BUNDLE_PLATFORM}.zip")
set(CMAKE_BUNDLE_URL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_BUNDLE_VERSION}/${CMAKE_BUNDLE_FILENAME}")

# Output paths
set(TOOLS_DIR "${OUTPUT_DIR}/tools")
set(CMAKE_BUNDLE_DIR "${TOOLS_DIR}/cmake")
set(CMAKE_BUNDLE_EXE "${CMAKE_BUNDLE_DIR}/bin/cmake.exe")
set(DOWNLOAD_DIR "${OUTPUT_DIR}/downloads")
set(DOWNLOAD_PATH "${DOWNLOAD_DIR}/${CMAKE_BUNDLE_FILENAME}")

# Check if CMake is already bundled
if(EXISTS "${CMAKE_BUNDLE_EXE}")
    message(STATUS "Bundled CMake already exists at: ${CMAKE_BUNDLE_EXE}")
    return()
endif()

message(STATUS "Bundling CMake ${CMAKE_BUNDLE_VERSION} for the editor...")

# Create directories
file(MAKE_DIRECTORY "${TOOLS_DIR}")
file(MAKE_DIRECTORY "${DOWNLOAD_DIR}")

# Download if not already downloaded
if(NOT EXISTS "${DOWNLOAD_PATH}")
    message(STATUS "Downloading CMake from: ${CMAKE_BUNDLE_URL}")
    file(DOWNLOAD
        "${CMAKE_BUNDLE_URL}"
        "${DOWNLOAD_PATH}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY ON
    )
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    list(GET DOWNLOAD_STATUS 1 STATUS_MESSAGE)

    if(NOT STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download CMake: ${STATUS_MESSAGE}")
    endif()

    message(STATUS "Download complete: ${DOWNLOAD_PATH}")
else()
    message(STATUS "Using cached download: ${DOWNLOAD_PATH}")
endif()

# Extract the archive
message(STATUS "Extracting CMake to: ${TOOLS_DIR}")
file(ARCHIVE_EXTRACT
    INPUT "${DOWNLOAD_PATH}"
    DESTINATION "${TOOLS_DIR}"
)

# Rename the extracted folder to just "cmake"
set(EXTRACTED_DIR "${TOOLS_DIR}/cmake-${CMAKE_BUNDLE_VERSION}-${CMAKE_BUNDLE_PLATFORM}")
if(EXISTS "${EXTRACTED_DIR}")
    if(EXISTS "${CMAKE_BUNDLE_DIR}")
        file(REMOVE_RECURSE "${CMAKE_BUNDLE_DIR}")
    endif()
    file(RENAME "${EXTRACTED_DIR}" "${CMAKE_BUNDLE_DIR}")
endif()

# Verify extraction
if(EXISTS "${CMAKE_BUNDLE_EXE}")
    message(STATUS "CMake bundled successfully: ${CMAKE_BUNDLE_EXE}")
else()
    message(FATAL_ERROR "Failed to extract CMake - executable not found at: ${CMAKE_BUNDLE_EXE}")
endif()
