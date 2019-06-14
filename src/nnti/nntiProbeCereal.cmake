INCLUDE(CheckIncludeFileCXX)

if( TARGET cereal )
    SET( save_CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}" )
    SET( CMAKE_REQUIRED_INCLUDES "${CEREAL_INCLUDES}" )

    CHECK_INCLUDE_FILE_CXX("cereal/cereal.hpp" NNTI_HAVE_CEREAL_CEREAL_HPP)

    SET( CMAKE_REQUIRED_INCLUDES "${save_CMAKE_REQUIRED_INCLUDES}" )

    SET( NNTI_HAVE_CEREAL 1 )
    if( NOT NNTI_HAVE_CEREAL_CEREAL_HPP )
      SET( NNTI_HAVE_CEREAL 0 )
    endif()
endif()