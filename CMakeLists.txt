cmake_minimum_required(VERSION 3.14)
project(Htkis-LANProxySerCli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /W3)
    foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach()
endif()

include(FetchContent)

# Fetch asio
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG master
)
FetchContent_MakeAvailable(asio)

# Fetch glfw
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

# Find OpenGL (optional, D3D11 fallback available)
find_package(OpenGL)

# Find OpenSSL (via vcpkg, requires -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake)
find_package(OpenSSL REQUIRED)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# Server executable (the only program)
add_executable(proxy_server
    src/server/Server.cpp
    src/server/GuiMain.cpp
    src/server/UserManager.cpp
    src/server/IpAcl.cpp
    src/server/Authenticator.cpp
    src/server/AuthFailureTracker.cpp
    src/server/DeadlineTimer.cpp
    src/server/DnsCache.cpp
    src/server/ReconnectPolicy.cpp
    src/server/ConnectionPool.cpp
    src/server/ProtocolDetector.cpp
    src/server/UdpRelaySession.cpp
    src/server/WebSocketFrame.cpp
    src/server/WebSocketHandler.cpp
    src/server/TokenBucket.cpp
    src/server/RateLimiter.cpp
    src/server/QosScheduler.cpp
    src/server/StatsCollector.cpp
    src/server/TlsContext.cpp
    src/server/HttpsHandler.cpp
    src/server/AeadCipher.cpp
    src/server/SsKeyDerivation.cpp
    src/server/AesCfbCipher.cpp
    src/server/CryptoSessionBase.cpp
    src/server/ShadowsocksSession.cpp
    src/server/ShadowsocksHandler.cpp
    src/server/SsMultiPortListener.cpp
    src/server/VmessSession.cpp
    src/server/VmessHeaderDecoder.cpp
    src/server/VmessChunkCodec.cpp
    src/server/VmessHandler.cpp
    src/server/VlessSession.cpp
    src/server/VlessHeaderParser.cpp
    src/server/VlessHandler.cpp
    src/server/TrojanSession.cpp
    src/server/TrojanHashAuth.cpp
    src/server/TrojanHandler.cpp
    src/server/StunClient.cpp
    src/server/NatTraversalManager.cpp
    src/server/RelayTunnelClient.cpp
    src/server/RelayManager.cpp
    src/server/RelayConfigStore.cpp
    src/server/RelaySessionRouter.cpp
    src/server/MemoryMonitor.cpp
    src/server/StabilityMetrics.cpp
    src/server/BufferPool.cpp
    src/server/BenchmarkEngine.cpp
    src/server/BenchmarkWorker.cpp
    src/common/Localization.cpp
    src/common/Config.cpp
    src/common/ConfigManager.cpp
    src/common/CrashHandler.cpp
    src/common/HashUtil.cpp
    ${IMGUI_SOURCES}
)

# Embed Windows resources (icon, etc.)
if(WIN32)
    target_sources(proxy_server PRIVATE src/server/resource.rc)
    # RC compiler needs to find HtkisPro.png relative to the .rc file
    set_source_files_properties(src/server/resource.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/HtkisPro.png
    )
endif()
target_include_directories(proxy_server PRIVATE
    src/server
    src/common
    third_party
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${asio_SOURCE_DIR}/include
)
target_compile_definitions(proxy_server PRIVATE ASIO_STANDALONE)
if(MSVC)
    target_link_options(proxy_server PRIVATE /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS)
endif()
target_link_libraries(proxy_server PRIVATE glfw)
if(OpenGL_FOUND)
    target_link_libraries(proxy_server PRIVATE OpenGL32)
    target_compile_definitions(proxy_server PRIVATE HAS_OPENGL)
endif()
if(WIN32)
    target_link_libraries(proxy_server PRIVATE gdiplus d3d11 dxgi d3dcompiler crypt32)
endif()
target_link_libraries(proxy_server PRIVATE OpenSSL::SSL OpenSSL::Crypto)
# ─── Testing ───
option(BUILD_TESTING "Build unit tests" OFF)
if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(tests)
endif()
