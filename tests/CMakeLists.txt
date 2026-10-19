set(TEST_SOURCES
    unit/test_ConfigManager.cpp
    unit/test_IpAcl.cpp
    unit/test_DnsCache.cpp
    unit/test_TokenBucket.cpp
    unit/test_RateLimiter.cpp
    unit/test_StatsCollector.cpp
    unit/test_AuthFailureTracker.cpp
    unit/test_HashUtil.cpp
    unit/test_ProtocolDetector.cpp
    unit/test_WebSocketFrame.cpp
)

set(CORE_SOURCES
    ../src/common/Config.cpp
    ../src/common/ConfigManager.cpp
    ../src/common/HashUtil.cpp
    ../src/server/IpAcl.cpp
    ../src/server/DnsCache.cpp
    ../src/server/TokenBucket.cpp
    ../src/server/RateLimiter.cpp
    ../src/server/StatsCollector.cpp
    ../src/server/AuthFailureTracker.cpp
    ../src/server/ProtocolDetector.cpp
    ../src/server/WebSocketFrame.cpp
)

add_executable(proxy_tests ${TEST_SOURCES} ${CORE_SOURCES})

target_include_directories(proxy_tests PRIVATE
    ../src/server
    ../src/common
    ../third_party
    ${asio_SOURCE_DIR}/include
)

target_compile_definitions(proxy_tests PRIVATE ASIO_STANDALONE)

target_link_libraries(proxy_tests PRIVATE
    gtest
    gtest_main
)

if(WIN32)
    target_link_libraries(proxy_tests PRIVATE bcrypt crypt32 ws2_32)
endif()

# Apply static CRT to tests too
if(MSVC)
    foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
        string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach()
endif()

include(GoogleTest)
gtest_discover_tests(proxy_tests)