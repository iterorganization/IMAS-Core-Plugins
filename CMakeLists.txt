# CMake build configuration for Access Layer plugins
cmake_minimum_required( VERSION 3.16 )

if( AL_DOCS_ONLY )
  return()
endif()

# Assume that al-core and the `al` target already exist
# TODO: fix standalone building when there is a use case for it
if( NOT TARGET al )
  message( FATAL_ERROR "Target `al` does not exist. Please ensure that al-core is added before al-plugins." )
endif()


project( al-plugins )


# Dependencies
find_package( LibXml2 REQUIRED )
find_package( Boost REQUIRED COMPONENTS log )

# CMake build options
set( CMAKE_POSITION_INDEPENDENT_CODE ON )
set( CMAKE_INCLUDE_CURRENT_DIR ON )

# Object libraries
add_library( al-plugin-simple-logger OBJECT simple_logger.cpp )
target_link_libraries( al-plugin-simple-logger al Boost::log )
add_library( al-plugin-reader-helper OBJECT al_reader_helper_plugin.cpp )
target_link_libraries( al-plugin-reader-helper al )

# debug plugin
add_library( al-debug-plugin SHARED debug_plugin.cpp )
set_target_properties( al-debug-plugin PROPERTIES PREFIX "" OUTPUT_NAME debug_plugin )
target_link_libraries( al-debug-plugin al-plugin-simple-logger al )

# nbc plugin -- EXCLUDE_FROM_ALL: this plugin doesn't compile at the moment
add_library( al-nbc-plugin SHARED EXCLUDE_FROM_ALL nbc_plugin.cpp )
set_target_properties( al-nbc-plugin PROPERTIES PREFIX "" OUTPUT_NAME nbc_plugin )
target_link_libraries( al-nbc-plugin al-plugin-simple-logger al LibXml2::LibXml2 )

# creation date plugin
add_library( al-creation-date-plugin SHARED creation_date_plugin.cpp )
set_target_properties( al-creation-date-plugin PROPERTIES PREFIX "" OUTPUT_NAME creation_date_plugin )
target_link_libraries( al-creation-date-plugin al )

# parameter plugin
add_library( al-parameter-plugin SHARED parameter_plugin.cpp )
set_target_properties( al-parameter-plugin PROPERTIES PREFIX "" OUTPUT_NAME parameter_plugin )
target_link_libraries( al-parameter-plugin al-plugin-reader-helper al )

# partial get plugin
add_library( al-partial-get-plugin SHARED partial_get_plugin.cpp )
set_target_properties( al-partial-get-plugin PROPERTIES PREFIX "" OUTPUT_NAME partial_get_plugin )
target_link_libraries( al-partial-get-plugin al-plugin-simple-logger al )

# patch reader plugin
add_library( al-patch-reader-plugin SHARED patch_reader_plugin.cpp )
set_target_properties( al-patch-reader-plugin PROPERTIES PREFIX "" OUTPUT_NAME patch_reader_plugin )
target_link_libraries( al-patch-reader-plugin al-plugin-simple-logger al-plugin-reader-helper al )

# Custom target that depends on all plugin library targets  
add_custom_target( al-plugins DEPENDS
  al-debug-plugin
  # al-nbc-plugin
  al-creation-date-plugin
  al-parameter-plugin
  al-partial-get-plugin
  al-patch-reader-plugin
)
