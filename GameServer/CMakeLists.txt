project(GameServer)

set(TARS_WEB_HOST "http://127.0.0.1:3000")
set(TARS_TOKEN "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOiJhZG1pbiIsImlhdCI6MTY2ODMyNTc4MSwiZXhwIjoxNzA0MDA4OTgxfQ.CKzRmIJYmn5MmjacLTAtIJ8FBpzDKx9BSVDFBpuYDzA")
#生成带tars文件的可执行程序
macro(gen_cmake_server APP TARGET)

    set(UPLOAD_FILES ${ARGN})

    include_directories(${PROJECT_SOURCE_DIR})

    build_tars_server(${TARGET} "")

    target_link_libraries(${TARGET}  ${LIB_TARS_SERVANT} ${LIB_TARS_UTIL})

    if(TARS_MYSQL)
        target_link_libraries(${TARGET} ${LIB_MYSQL})
    endif()

    if(TARS_SSL)
        target_link_libraries(${TARGET} ${LIB_SSL} ${LIB_CRYPTO})
    endif()

    if(TARS_HTTP2)
        target_link_libraries(${TARGET} ${LIB_HTTP2})
    endif()

    IF (UNIX)
        target_link_libraries(${TARGET} pthread dl)
    ENDIF ()

    #make tar #########################################################################
    #must create tmp directory, avoid linux cmake conflict!
    SET(RUN_TAR_COMMAND_FILE "${CMAKE_BINARY_DIR}/run-tar-${TARGET}.cmake")
    FILE(WRITE ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo mkdir -p ${CMAKE_BINARY_DIR}/tmp/${TARGET})\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo rm -rf ${CMAKE_BINARY_DIR}/tmp/${TARGET})\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tmp/${TARGET})\n")

    SET(TARGET_EXE_NAME ${TARGET})

    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo copy bin/${TARGET_EXE_NAME} ${CMAKE_BINARY_DIR}/tmp/${TARGET}/)\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy bin/${TARGET_EXE_NAME} ${CMAKE_BINARY_DIR}/tmp/${TARGET}/)\n")

    foreach(UPLOAD ${UPLOAD_FILES})
        FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo copy ${UPLOAD} ${CMAKE_BINARY_DIR}/tmp/${TARGET}/)\n")
        IF(WIN32)
            FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${UPLOAD} ${CMAKE_BINARY_DIR}/tmp/${TARGET}/)\n")
        ELSE()
            FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${UPLOAD} ${CMAKE_BINARY_DIR}/tmp/${TARGET}/)\n")
        ENDIF()
    endforeach(UPLOAD ${UPLOAD_FILES})

    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/tmp/ tar czfv ${TARGET}.tgz ${TARGET})\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/tmp/${TARGET}.tgz ${CMAKE_BINARY_DIR}/${TARGET}.tgz)\n")

    add_custom_command(OUTPUT ${TARGET}.tgz
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -P ${RUN_TAR_COMMAND_FILE}
            COMMENT "call ${RUN_TAR_COMMAND_FILE}")

    FILE(APPEND ${TARS_TAR} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -P ${RUN_TAR_COMMAND_FILE})\n")

    #make upload #########################################################################
    SET(RUN_UPLOAD_COMMAND_FILE "${PROJECT_BINARY_DIR}/run-upload-${TARGET}.cmake")
    FILE(WRITE ${RUN_UPLOAD_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo ${TARS_WEB_HOST}/api/upload_and_publish -Fsuse=@${TARGET}.tgz -Fapplication=${APP} -Fmodule_name=${TARGET} -Fcomment=developer-auto-upload)\n")
    FILE(APPEND ${RUN_UPLOAD_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND curl ${TARS_WEB_HOST}/api/upload_and_publish?ticket=${TARS_TOKEN} -Fsuse=@${TARGET}.tgz -Fapplication=${APP} -Fmodule_name=${TARGET} -Fcomment=developer-auto-upload)\n")
    FILE(APPEND ${RUN_UPLOAD_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo \n---------------------------------------------------------------------------)\n")


    #执行命令
    add_custom_target(${TARGET}-upload
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS ${TARGET}-tar
            COMMAND ${CMAKE_COMMAND} -P ${RUN_UPLOAD_COMMAND_FILE}
            COMMENT "upload ${APP}.${TARGET}.tgz and publish...")

    FILE(APPEND ${TARS_UPLOAD} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -P ${RUN_UPLOAD_COMMAND_FILE})\n")
endmacro()

add_subdirectory(src)

message("-------------------------------------------------------------------------------------")
message("CMAKE_SOURCE_DIR:          ${CMAKE_SOURCE_DIR}")
message("CMAKE_BINARY_DIR:          ${CMAKE_BINARY_DIR}")
message("PROJECT_SOURCE_DIR:        ${PROJECT_SOURCE_DIR}")
message("CMAKE_BUILD_TYPE:          ${CMAKE_BUILD_TYPE}")
message("PLATFORM:                  ${PLATFORM}")
message("TARS2CPP:                  ${TARS2CPP}")
message("TARSMERGE:                 ${TARSMERGE}")
message("TARS_MYSQL:                ${TARS_MYSQL}")
message("TARS_HTTP2:                ${TARS_HTTP2}")
message("TARS_SSL:                  ${TARS_SSL}")
message("TARS_WEB_HOST:             ${TARS_WEB_HOST}")
message("TARS_TOKEN:                ${TARS_TOKEN}")
message("-------------------------------------------------------------------------------------")
