# cmake/Proto.cmake
#
# Provides the generate_grpc_cpp macro which drives protoc + grpc_cpp_plugin
# to produce C++ source files from a list of .proto inputs, and bundles those
# generated sources into an OBJECT library called `inspection_proto`.
#
# Usage:
#   generate_grpc_cpp(PROTO_FILES path/to/file.proto [path/to/other.proto ...])
#
# After calling the macro the following target is available:
#   inspection_proto  – OBJECT library with all generated .pb.cc / .grpc.pb.cc
#
# Consumers link with:
#   target_link_libraries(my_target PRIVATE inspection_proto)
# which propagates protobuf and gRPC link dependencies transitively.

# ---------------------------------------------------------------------------
# Locate the grpc_cpp_plugin executable
# ---------------------------------------------------------------------------
if(NOT GRPC_CPP_PLUGIN)
    find_program(GRPC_CPP_PLUGIN
        NAMES grpc_cpp_plugin
        HINTS /usr/bin /usr/local/bin
        DOC "gRPC C++ plugin for protoc"
    )
endif()

if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR
        "[Proto.cmake] grpc_cpp_plugin not found. "
        "Install protobuf-compiler-grpc or set GRPC_CPP_PLUGIN manually."
    )
endif()

message(STATUS "[Proto.cmake] Using grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")

# ---------------------------------------------------------------------------
# Locate protoc
# ---------------------------------------------------------------------------
if(NOT Protobuf_PROTOC_EXECUTABLE)
    find_program(Protobuf_PROTOC_EXECUTABLE
        NAMES protoc
        HINTS /usr/bin /usr/local/bin
        DOC "Google Protocol Buffers compiler"
    )
endif()

if(NOT Protobuf_PROTOC_EXECUTABLE)
    message(FATAL_ERROR
        "[Proto.cmake] protoc not found. "
        "Install protobuf-compiler or set Protobuf_PROTOC_EXECUTABLE manually."
    )
endif()

message(STATUS "[Proto.cmake] Using protoc: ${Protobuf_PROTOC_EXECUTABLE}")

# ---------------------------------------------------------------------------
# Macro: generate_grpc_cpp
# ---------------------------------------------------------------------------
macro(generate_grpc_cpp)
    # Parse arguments
    set(_options "")
    set(_one_value_args "")
    set(_multi_value_args PROTO_FILES)
    cmake_parse_arguments(_GRPC "${_options}" "${_one_value_args}" "${_multi_value_args}" ${ARGN})

    if(NOT _GRPC_PROTO_FILES)
        message(FATAL_ERROR "[generate_grpc_cpp] PROTO_FILES argument is required.")
    endif()

    # ------------------------------------------------------------------
    # Determine include root directories for protoc.
    #
    # protoc requires that each .proto file is passed as a path RELATIVE
    # to one of the -I roots.  We therefore:
    #   1. Compute the absolute directory of each proto file.
    #   2. Pass that directory as -I (so the file is seen as just its
    #      filename, e.g. "inspection_gateway.proto").
    #   3. Also add /usr/include so that well-known types like
    #      google/protobuf/timestamp.proto resolve correctly.
    # ------------------------------------------------------------------
    set(_PROTO_INCLUDE_DIRS /usr/include)
    foreach(_proto ${_GRPC_PROTO_FILES})
        get_filename_component(_proto_abs_tmp "${_proto}" ABSOLUTE)
        get_filename_component(_proto_dir_tmp "${_proto_abs_tmp}" DIRECTORY)
        list(APPEND _PROTO_INCLUDE_DIRS "${_proto_dir_tmp}")
    endforeach()
    list(REMOVE_DUPLICATES _PROTO_INCLUDE_DIRS)

    # Build the list of -I flags
    set(_PROTOC_INCLUDE_FLAGS "")
    foreach(_dir ${_PROTO_INCLUDE_DIRS})
        list(APPEND _PROTOC_INCLUDE_FLAGS "-I${_dir}")
    endforeach()

    # Output directory for generated files (placed inside the build tree so
    # that out-of-source builds stay clean)
    set(_PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto_gen")
    file(MAKE_DIRECTORY "${_PROTO_GEN_DIR}")

    # ------------------------------------------------------------------
    # Generate source files for each .proto
    # ------------------------------------------------------------------
    set(_ALL_PROTO_SRCS "")
    set(_ALL_PROTO_HDRS "")

    foreach(_proto_file ${_GRPC_PROTO_FILES})
        # Absolute path of the .proto source (handles relative input paths)
        get_filename_component(_proto_abs  "${_proto_file}" ABSOLUTE)
        # Base name without extension (e.g. "inspection_gateway")
        get_filename_component(_proto_name "${_proto_abs}"  NAME_WE)
        # Directory that contains the .proto (used as -I root)
        get_filename_component(_proto_dir  "${_proto_abs}"  DIRECTORY)
        # Filename only (e.g. "inspection_gateway.proto") – passed to protoc
        # as the input file after the -I roots are specified.
        get_filename_component(_proto_fname "${_proto_abs}" NAME)

        set(_pb_cc   "${_PROTO_GEN_DIR}/${_proto_name}.pb.cc")
        set(_pb_h    "${_PROTO_GEN_DIR}/${_proto_name}.pb.h")
        set(_grpc_cc "${_PROTO_GEN_DIR}/${_proto_name}.grpc.pb.cc")
        set(_grpc_h  "${_PROTO_GEN_DIR}/${_proto_name}.grpc.pb.h")

        # ------------------------------------------------------------------
        # Custom command: protobuf message code generation
        #
        # protoc is invoked with:
        #   -I<proto_dir>     (so the file resolves as just its filename)
        #   -I/usr/include    (so google/protobuf/timestamp.proto resolves)
        #   --cpp_out=<dir>
        #   <proto_filename>  (relative to -I root, NOT the absolute path)
        # ------------------------------------------------------------------
        add_custom_command(
            OUTPUT  "${_pb_cc}" "${_pb_h}"
            COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
                    "-I${_proto_dir}"
                    "-I/usr/include"
                    "--cpp_out=${_PROTO_GEN_DIR}"
                    "${_proto_fname}"
            DEPENDS "${_proto_abs}"
            COMMENT "[Proto.cmake] Generating protobuf C++ for ${_proto_fname}"
            VERBATIM
        )

        # ------------------------------------------------------------------
        # Custom command: gRPC service stub code generation
        # ------------------------------------------------------------------
        add_custom_command(
            OUTPUT  "${_grpc_cc}" "${_grpc_h}"
            COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
                    "-I${_proto_dir}"
                    "-I/usr/include"
                    "--grpc_out=${_PROTO_GEN_DIR}"
                    "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
                    "${_proto_fname}"
            DEPENDS "${_proto_abs}"
            COMMENT "[Proto.cmake] Generating gRPC C++ stubs for ${_proto_fname}"
            VERBATIM
        )

        list(APPEND _ALL_PROTO_SRCS "${_pb_cc}" "${_grpc_cc}")
        list(APPEND _ALL_PROTO_HDRS "${_pb_h}"  "${_grpc_h}")
    endforeach()

    # ------------------------------------------------------------------
    # Build an OBJECT library that compiles the generated sources once.
    # Consumers link against inspection_proto (or its alias inspection::proto)
    # to inherit the compiled objects and transitive dependencies without
    # re-compiling the heavy proto/gRPC generated code.
    # ------------------------------------------------------------------
    add_library(inspection_proto OBJECT
        ${_ALL_PROTO_SRCS}
        ${_ALL_PROTO_HDRS}
    )

    # Generated files include each other and the well-known protos
    target_include_directories(inspection_proto
        PUBLIC
            "${_PROTO_GEN_DIR}"
            "${Protobuf_INCLUDE_DIRS}"
            /usr/include
    )

    # Propagate protobuf link dependency to every consumer
    target_link_libraries(inspection_proto
        PUBLIC
            protobuf::libprotobuf
    )

    # gRPC link dependency: prefer CMake config targets, fall back to
    # pkg-config imported target or raw link names.
    if(TARGET gRPC::grpc++)
        target_link_libraries(inspection_proto PUBLIC gRPC::grpc++)
    elseif(TARGET gRPC::grpc++_unsecure)
        target_link_libraries(inspection_proto PUBLIC gRPC::grpc++_unsecure)
    elseif(TARGET PkgConfig::GRPC)
        target_link_libraries(inspection_proto PUBLIC PkgConfig::GRPC)
    else()
        target_link_libraries(inspection_proto PUBLIC grpc++ grpc gpr)
    endif()

    # Suppress warnings that protoc-generated code routinely triggers
    target_compile_options(inspection_proto PRIVATE
        -Wno-unused-parameter
        -Wno-deprecated-declarations
    )

    # Convenience ALIAS for namespaced usage
    add_library(inspection::proto ALIAS inspection_proto)

    message(STATUS "[Proto.cmake] inspection_proto OBJECT library configured.")
    message(STATUS "[Proto.cmake] Generated headers will be in: ${_PROTO_GEN_DIR}")
endmacro()
