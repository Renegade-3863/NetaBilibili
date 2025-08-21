cmake_minimum_required(VERSION 3.10)

aux_source_directory(./ SRC)

add_library(ReactorHttpWebSocket STATIC ${SRC})
