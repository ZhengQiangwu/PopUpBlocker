#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <locale>

// 包含我们库的公共API头文件
// (请确保这个相对路径相对于您的项目结构是正确的)
#include "../PopUpBlocker/popup_blocker_api.h"

void printTaskList() {
    int count = 0;
    // 1. 调用API获取任务栏窗口列表
    WindowInfo* windows = GetTaskbarWindows(&count);

    std::cout << "\n--- 当前任务栏窗口列表 (" << count << "个) ---" << std::endl;
    if (count > 0 && windows) {
        for (int i = 0; i < count; ++i) {
            std::cout << "  [" << i << "] ID: 0x" << std::hex << windows[i].id << std::dec
                      << ", 标题: " << windows[i].title
                      << ", 进程: " << windows[i].process_name 
                      << ", 类: " << windows[i].wm_class << std::endl;
            //CloseWindowById(windows[i].id)
        }
        // 2. 使用完毕后必须释放内存
        FreeWindowInfoArray(windows, count);
    } else {
        std::cout << "  (列表为空或获取失败)" << std::endl;
    }
    std::cout << "------------------------------------" << std::endl;
}

void printBlacklist() {
    int count = 0;
    // 1. 调用API获取黑名单
    const char** blacklist_c = GetBlacklist(&count);
    std::cout << "\n--- 当前黑名单列表 (" << count << "个) ---" << std::endl;
    if (count > 0 && blacklist_c) {
        for(int i=0; i < count; ++i) {
            std::cout << "  - " << blacklist_c[i] << std::endl;
        }
        // 2. 使用完毕后必须释放内存
        FreeStringArray(blacklist_c, count);
    } else {
        std::cout << "  (列表为空)" << std::endl;
    }
    std::cout << "------------------------------------" << std::endl;
}

/**
 * @brief 【新增】一个专门用于测试 GetTaskbarWindowTitles 的函数
 */
void printTitleList() {
    int count = 0;
    // 1. 调用新的API获取标题列表
    const char** titles = GetTaskbarWindowTitles(&count);

    std::cout << "\n--- 当前任务栏窗口列表 (仅标题) (" << count << "个) ---" << std::endl;
    if (count > 0 && titles) {
        for (int i = 0; i < count; ++i) {
            std::cout << "  - 标题 [" << i << "]: " << titles[i] << std::endl;
        }
        // 2. 使用完毕后必须释放内存
        FreeStringArray(titles, count);
    } else {
        std::cout << "  (列表为空或获取失败)" << std::endl;
    }
    std::cout << "------------------------------------" << std::endl;
}

int main() {
    std::setlocale(LC_ALL, "");
    std::cout << "--- C++ 任务栏应用管理测试程序 ---" << std::endl;

    // 启动后台自动拦截服务
    std::cout << "[信息] 正在启动自动拦截服务..." << std::endl;
    StartMonitoring();
    EnableDebugLogging(true);

    // 设置黑名单
    std::cout << "[信息] 正在设置黑名单..." << std::endl;
    const char* blacklist[] = { 
        "rhythmbox",    // 按进程名拦截
        "Rhythmbox",    // 按窗口类拦截
        "dde-file-manage", 
        "browser",		
        nullptr         // 数组必须以NULL结尾
    };
    SetBlacklist(blacklist);
    
    // 获取并打印已设置的黑名单
    printBlacklist();
    
    std::cout << "[信息] 服务正在运行。Rhythmbox 应用将会被自动关闭。" << std::endl;
    std::cout << "程序将每 5 秒自动刷新一次任务栏列表。" << std::endl;
    std::cout << "按 [Enter] 键停止。" << std::endl;

    // 模拟UI轮询
    while (true) {
        // 检查用户是否按了Enter (非阻塞方式)
        // 这是一个简化的模拟，实际UI应用会有自己的事件循环
        if (std::cin.rdbuf()->in_avail() > 0) {
            std::cin.get();
            break;
        }

        // 调用函数打印任务栏列表
        printTaskList();
        printTitleList();   // 【新增】打印仅标题信息
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // 停止服务
    std::cout << "\n[信息] 正在停止监控服务..." << std::endl;
    StopMonitoring();

    std::cout << "[信息] 服务已停止。测试完成。" << std::endl;
    return 0;
}