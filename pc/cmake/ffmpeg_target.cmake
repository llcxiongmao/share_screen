function(ffmpeg_target NAME MAJOR_HEADER)
    find_path(${NAME}_INCLUDE_DIR ${MAJOR_HEADER} REQUIRED)
    set(_INCLUDE_DIR ${${NAME}_INCLUDE_DIR})

    find_file(${NAME}_RELEASE_LIB lib${NAME}.a PATH_SUFFIXES lib REQUIRED)
    set(_RELEASE_LIB ${${NAME}_RELEASE_LIB})

    add_library(${NAME} STATIC IMPORTED)
    set_target_properties(${NAME} PROPERTIES
        IMPORTED_LOCATION "${_RELEASE_LIB}"
        IMPORTED_IMPLIB "${_RELEASE_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${_INCLUDE_DIR}")
endfunction()