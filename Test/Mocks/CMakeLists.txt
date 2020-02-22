add_library(asyncly_mocks INTERFACE)

target_include_directories(asyncly_mocks INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(asyncly_mocks INTERFACE asyncly)
target_link_libraries(asyncly_mocks INTERFACE GTest::gmock)
