cmake_minimum_required(VERSION 3.10)
project(PopupBlockerTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ----------------------------------------------------------------------------
# 查找我们自己的 libAudioVideoProc.so 库
# ----------------------------------------------------------------------------
# 定义库头文件的搜索路径
# 使用相对路径，假设测试项目和库项目在同一个父目录下
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../PopUpBlocker)

# 定义库文件的搜索路径
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../PopUpBlocker/build)
# ----------------------------------------------------------------------------
# 创建可执行文件目标
# ----------------------------------------------------------------------------
add_executable(test_blocker test_blocker.cpp)

# 2. 告诉编译器去哪里找我们的API头文件 (.h)
#    我们使用相对路径指向库项目的源目录
target_include_directories(test_blocker PUBLIC ../PopupBlockerLib)

# 3. 查找X11库
find_package(X11 REQUIRED)

# 4. 【核心改动】将我们的可执行文件链接到所有需要的库
#    我们直接提供 libpopup_blocker.so 的完整相对路径
#    这比使用 link_directories 更明确、更可靠

target_link_libraries(test_blocker
    
    popup_blocker
    pthread
    ${X11_LIBRARIES}
)
