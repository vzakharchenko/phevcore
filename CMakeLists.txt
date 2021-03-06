project("Phev" C)
cmake_minimum_required(VERSION 3.0)

set (CMAKE_C_STANDARD 11)

find_library(MSG_CORE msg_core "/usr/local/lib")
find_library(CJSON cjson)

option(BUILD_TESTS "Build the test binaries")

add_library(phev STATIC
    src/phev_register.c
    src/phev_pipe.c
    src/phev_core.c
    src/phev_service.c
    src/phev_model.c
    src/phev_tcpip.c
    src/phev.c
)

# find_package(zf_queue)
# target_link_libraries(phev msg_core cjson)

if(${BUILD_TESTS})
    find_package(unity)
    include(CTest)
    add_subdirectory(test)
endif()

#add_subdirectory(external) 

#target_include_directories(msg_core PUBLIC msg_core/include)


if(WIN32)
    target_link_libraries(phev LINK_PUBLIC
        msg_core
        cjson
        mswsock
        advapi32 
        ws2_32

    )
else()
    target_link_libraries (phev LINK_PUBLIC 
        ${MSG_CORE}
        ${CJSON}
    )
endif()

set_property(TARGET phev PROPERTY C_STANDARD 11)

target_include_directories(phev PUBLIC include)

install(
    TARGETS
      phev
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install (FILES
	include/phev.h
    include/phev_service.h
    include/phev_core.h
    include/phev_pipe.h
    include/phev_model.h
    include/phev_register.h
	DESTINATION include/
)
