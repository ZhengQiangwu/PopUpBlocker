# 设置最低要求的 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 定义项目名称
project(PopUpBlocker)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 添加库目标，类型为 SHARED (共享库 .so)
add_library(popup_blocker SHARED popup_blocker.cpp)

# 设置头文件的包含目录，这样 #include "popup_blocker_api.h" 才能被找到
# PUBLIC 表示任何链接到这个库的其它CMake项目也会自动获得这个头文件路径
target_include_directories(popup_blocker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 查找并链接必要的库
find_package(X11 REQUIRED)
target_link_libraries(popup_blocker PUBLIC ${X11_LIBRARIES} pthread)
