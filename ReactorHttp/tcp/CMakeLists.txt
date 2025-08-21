cmake_minimum_required(VERSION 3.10)

aux_source_directory(./ SRC)

add_library(ReactorHttpTcp STATIC ${SRC})

if (TARGET OpenSSL::SSL)
    target_link_libraries(ReactorHttpTcp PUBLIC OpenSSL::SSL OpenSSL::Crypto)
else()
    target_link_libraries(ReactorHttpTcp PUBLIC ${OPENSSL_LIBRARIES})
endif()