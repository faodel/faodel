
#######################################################################
#          define how to create libraries, exes, texts, etc.          #


set( HEADERS
     ${HEADERS}
     net/net.hh
     PARENT_SCOPE
    )

set( NET_SOURCES
     net/net.cpp
)

# Select network library wrapper to use

if(Faodel_NETWORK_LIBRARY STREQUAL "nnti")
   message(STATUS "Using NNTI")
   set(Faodel_OPBOX_NET_NNTI 1 CACHE BOOL "")
   mark_as_advanced(Faodel_OPBOX_NET_NNTI)
   set( SOURCES
        ${SOURCES}
        ${NET_SOURCES}
        net/nnti/NetNnti.cpp
        PARENT_SCOPE
   )

elseif(Faodel_NETWORK_LIBRARY STREQUAL "libfabric")
   message(STATUS "Using LIBFABRIC")
   set(Faodel_OPBOX_NET_LIBFABRIC 1 CACHE BOOL "")
   mark_as_advanced(Faodel_OPBOX_NET_LIBFABRIC)
   set( SOURCES
        ${SOURCES}
        ${NET_SOURCES}
        net/libfabric_wrapper/fab_transport.cpp
        net/libfabric_wrapper/libfabric_wrapper.cpp
        PARENT_SCOPE
   )

else()
  message(FATAL_ERROR "Faodel_NETWORK_LIBRARY must be defined as either nnti or libfabric")
endif()
