add_library(Project135Core INTERFACE)
add_library(Project135::Core ALIAS Project135Core)
target_include_directories(Project135Core INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(Project135Core INTERFACE cxx_std_20)
