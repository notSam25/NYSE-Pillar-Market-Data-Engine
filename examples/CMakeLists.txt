add_executable(stock-summary stock-summary/main.cpp)
target_link_libraries(stock-summary PRIVATE ${PROJECT_NAME})
target_compile_definitions(stock-summary PRIVATE
    NYSE_TAQ_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)

add_executable(bbo-validation bbo-validation/main.cpp)
target_link_libraries(bbo-validation PRIVATE ${PROJECT_NAME})
target_compile_definitions(bbo-validation PRIVATE
    NYSE_TAQ_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)
