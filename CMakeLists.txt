cmake_minimum_required(VERSION 3.10)
project(mimefdupes C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMAGIC REQUIRED libmagic)

if(NOT LIBMAGIC_FOUND)
    find_library(MAGIC_LIB magic)
    find_path(MAGIC_INCLUDE_DIR magic.h)
    if(MAGIC_LIB AND MAGIC_INCLUDE_DIR)
        set(LIBMAGIC_LIBRARIES ${MAGIC_LIB})
        set(LIBMAGIC_INCLUDE_DIRS ${MAGIC_INCLUDE_DIR})
        set(LIBMAGIC_FOUND TRUE)
    endif()
endif()

if(NOT LIBMAGIC_FOUND)
    message(FATAL_ERROR "libmagic not found")
endif()

find_package(OpenSSL REQUIRED)
if(NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found")
endif()

add_executable(mimefdupes src/mimefdupes.c)

target_include_directories(mimefdupes PRIVATE 
    ${LIBMAGIC_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(mimefdupes 
    ${LIBMAGIC_LIBRARIES}
    OpenSSL::Crypto
    OpenSSL::SSL
)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/mimefdupes.c)
    message(FATAL_ERROR "Source file src/mimefdupes.c not found!")
endif()

install(TARGETS mimefdupes DESTINATION bin)