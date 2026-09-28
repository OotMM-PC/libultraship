include_guard(GLOBAL)

function(ship_prepare_vcpkg root revision repository)
    string(LENGTH "${revision}" revision_length)
    if(NOT revision_length EQUAL 40 OR NOT revision MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR "VCPKG_REVISION must be a full 40-character commit SHA")
    endif()
    string(TOLOWER "${revision}" revision)
    if(NOT EXISTS "${root}")
        execute_process(COMMAND git init "${root}" COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND git remote add origin "${repository}"
                        WORKING_DIRECTORY "${root}" COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND git fetch --depth 1 origin "${revision}"
                        WORKING_DIRECTORY "${root}" COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND git checkout --detach "${revision}"
                        WORKING_DIRECTORY "${root}" COMMAND_ERROR_IS_FATAL ANY)
    endif()

    if(NOT EXISTS "${root}/.git")
        message(FATAL_ERROR "VCPKG_ROOT is not a Git checkout: ${root}")
    endif()
    execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${root}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE actual ERROR_VARIABLE detail
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0 OR NOT actual STREQUAL revision)
        message(FATAL_ERROR
            "vcpkg revision mismatch at ${root}: expected ${revision}, found '${actual}'. "
            "Use a fresh VCPKG_ROOT or explicitly select an audited VCPKG_REVISION. ${detail}")
    endif()
    execute_process(COMMAND git status --porcelain --untracked-files=normal
                    WORKING_DIRECTORY "${root}" RESULT_VARIABLE dirty_result OUTPUT_VARIABLE dirty)
    if(NOT dirty_result EQUAL 0 OR NOT dirty STREQUAL "")
        message(FATAL_ERROR "vcpkg has source modifications at ${root}; refusing an unpinned build: ${dirty}")
    endif()

    if(WIN32)
        set(executable "${root}/vcpkg.exe")
        set(bootstrap "${root}/bootstrap-vcpkg.bat")
    else()
        set(executable "${root}/vcpkg")
        set(bootstrap "${root}/bootstrap-vcpkg.sh")
    endif()
    set(stamp "${root}/downloads/ootmm-bootstrap-revision.txt")
    set(expected_stamp "")
    set(actual_stamp "")
    if(EXISTS "${executable}")
        file(SHA256 "${executable}" executable_hash)
        set(expected_stamp "${revision}:${executable_hash}")
    endif()
    if(EXISTS "${stamp}")
        file(READ "${stamp}" actual_stamp)
    endif()
    if(expected_stamp STREQUAL "" OR NOT actual_stamp STREQUAL expected_stamp)
        message(STATUS "Bootstrapping vcpkg at ${revision}")
        execute_process(COMMAND "${bootstrap}" -disableMetrics
                        WORKING_DIRECTORY "${root}" COMMAND_ERROR_IS_FATAL ANY)
        if(NOT EXISTS "${executable}")
            message(FATAL_ERROR "vcpkg bootstrap did not produce ${executable}")
        endif()
        file(SHA256 "${executable}" executable_hash)
        file(MAKE_DIRECTORY "${root}/downloads")
        file(WRITE "${stamp}" "${revision}:${executable_hash}")
    endif()
    message(STATUS "Using pinned vcpkg ${revision} at ${root}")
endfunction()

function(ship_install_vcpkg_packages executable root)
    execute_process(COMMAND "${executable}" install ${ARGN}
                    WORKING_DIRECTORY "${root}" COMMAND_ERROR_IS_FATAL ANY)
endfunction()
