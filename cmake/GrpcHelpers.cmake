# add_proto_messages(<target> PROTO <path/to/file.proto> [DEPENDS <other_proto_targets...>])
#
# Generates only C++ message types (.pb.cc / .pb.h) — no gRPC stubs.
# Use for proto files that define shared messages imported by service protos.
function(add_proto_messages TARGET_NAME)
    cmake_parse_arguments(ARG "" "PROTO" "DEPENDS" ${ARGN})
    if(NOT ARG_PROTO)
        message(FATAL_ERROR "add_proto_messages: PROTO argument is required")
    endif()
    _generate_proto_target(${TARGET_NAME} "${ARG_PROTO}" FALSE "${ARG_DEPENDS}")
endfunction()

# add_grpc_service(<target> PROTO <path/to/file.proto> [DEPENDS <other_proto_targets...>])
#
# Generates both message types and gRPC service stubs (.grpc.pb.cc / .grpc.pb.h).
# Use DEPENDS to list add_proto_messages targets whose types this service uses.
function(add_grpc_service TARGET_NAME)
    cmake_parse_arguments(ARG "" "PROTO" "DEPENDS" ${ARGN})
    if(NOT ARG_PROTO)
        message(FATAL_ERROR "add_grpc_service: PROTO argument is required")
    endif()
    _generate_proto_target(${TARGET_NAME} "${ARG_PROTO}" TRUE "${ARG_DEPENDS}")
endfunction()

# Internal implementation — not intended for direct use.
function(_generate_proto_target TARGET_NAME PROTO_FILE WITH_GRPC PROTO_DEPS)
    get_filename_component(PROTO_ABS "${PROTO_FILE}" ABSOLUTE)
    get_filename_component(PROTO_DIR "${PROTO_ABS}" PATH)
    get_filename_component(PROTO_STEM "${PROTO_ABS}" NAME_WE)

    set(OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_out")
    file(MAKE_DIRECTORY "${OUT_DIR}")

    # Collect -I paths from dependency targets so protoc can resolve imports
    set(IMPORT_DIRS "-I${PROTO_DIR}")
    foreach(DEP ${PROTO_DEPS})
        get_target_property(DEP_INCLUDE_DIRS ${DEP} INTERFACE_INCLUDE_DIRECTORIES)
        if(DEP_INCLUDE_DIRS)
            foreach(DIR ${DEP_INCLUDE_DIRS})
                list(APPEND IMPORT_DIRS "-I${DIR}")
            endforeach()
        endif()
    endforeach()

    set(PROTO_SRCS "${OUT_DIR}/${PROTO_STEM}.pb.cc")
    set(PROTO_HDRS "${OUT_DIR}/${PROTO_STEM}.pb.h")
    set(OUTPUT_FILES "${PROTO_SRCS}" "${PROTO_HDRS}")
    set(PROTOC_ARGS ${IMPORT_DIRS} --cpp_out "${OUT_DIR}" "${PROTO_ABS}")

    if(WITH_GRPC)
        set(GRPC_SRCS "${OUT_DIR}/${PROTO_STEM}.grpc.pb.cc")
        set(GRPC_HDRS "${OUT_DIR}/${PROTO_STEM}.grpc.pb.h")
        list(APPEND OUTPUT_FILES "${GRPC_SRCS}" "${GRPC_HDRS}")
        list(APPEND PROTOC_ARGS
            --grpc_out "${OUT_DIR}"
            --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
        )
    endif()

    add_custom_command(
        OUTPUT  ${OUTPUT_FILES}
        COMMAND $<TARGET_FILE:protoc>
        ARGS    ${PROTOC_ARGS}
        DEPENDS "${PROTO_ABS}"
        COMMENT "Generating protobuf stubs for ${PROTO_STEM}.proto"
    )

    add_library(${TARGET_NAME} STATIC ${OUTPUT_FILES})
    target_link_libraries(${TARGET_NAME} PUBLIC libprotobuf $<$<BOOL:${WITH_GRPC}>:grpc++> ${PROTO_DEPS})
    target_include_directories(${TARGET_NAME} PUBLIC "${OUT_DIR}")
endfunction()
