##########################################################################
# Unit Test
##########################################################################
project(test_security)

add_executable(${PROJECT_NAME} main.cpp)

add_catch_test(${PROJECT_NAME})

##########################################################################
# Link
##########################################################################
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    Threads::Threads
    cpprest
    ${OPENSSL_LIBRARIES}
    ldapcpp
    common
)
