
# https://chatgpt.com/c/2a14ccda-47ef-47c8-9b94-dfd18606bf6e

file(GLOB_RECURSE SRC_FILES 
    "*.c"
    "*.cpp"
)
file(GLOB_RECURSE INC_DIRS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*/"
)

idf_component_register(SRCS ${SRC_FILES}
                       INCLUDE_DIRS "." "utils" "wifi" "owl"
                       )