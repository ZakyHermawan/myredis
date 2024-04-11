add_executable(unit_tests client_test.cpp
  ../src/eventloop.cpp
)
target_link_libraries(unit_tests
  GTest::GTest
  GTest::Main
)
target_include_directories(unit_tests PRIVATE ../include)
add_test(client_test unit_tests)
