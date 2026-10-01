cmake_minimum_required(VERSION 3.20)
project(order_book LANGUAGES CXX)

add_library(orderbook src/order_book.cpp)
target_include_directories(orderbook PUBLIC include)
target_compile_features(orderbook PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(orderbook PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(order_book_app src/main.cpp)
target_link_libraries(order_book_app PRIVATE orderbook)

include(CTest)
if(BUILD_TESTING)
  add_executable(order_book_tests tests/order_book_tests.cpp)
  target_link_libraries(order_book_tests PRIVATE orderbook)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(order_book_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
  add_test(NAME order_book_tests COMMAND order_book_tests)
endif()
