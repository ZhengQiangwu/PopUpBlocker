#include "popup_blocker_api.h"

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>
#include <algorithm>
#include <cctype>
#include <queue>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

// --- 全局状态变量 ---
static std::atomic<bool> g_isRunning(false);
static std::thread g_workerThread;
static std::atomic<bool> g_debugLoggingEnabled(false);
static std::vector<std::string> g_blacklist;
static std::set<long> g_pendingCloseWindows;
static std::mutex g_dataMutex;

// --- 命名空间包含所有内部实现 ---
namespace {
static std::atomic<bool> g_x11_error_occurred(false);

int x11ErrorHandler(Display* display, XErrorEvent* event) {
    g_x11_error_occurred = true;
    return 0;
}

std::string getProcessNameByPid(pid_t pid) {
    if (pid <= 0) {
        return "";
    }
    std::string path = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream comm_file(path);
    if (comm_file.is_open()) {
        std::string name;
        std::getline(comm_file, name);
        return name;
    }
    return "";
}

unsigned long getWindowLongProperty_internal(Display* display, Window window, const char* prop_name) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop_data = nullptr;
    unsigned long property = 0;
    Atom atom = XInternAtom(display, prop_name, False);
    
    g_x11_error_occurred = false;
    XGetWindowProperty(display, window, atom, 0, 1, False, AnyPropertyType,
                       &actual_type, &actual_format, &nitems, &bytes_after, &prop_data);

    if (g_x11_error_occurred) {
        if (prop_data) {
            XFree(prop_data);
        }
        return 0;
    }
    
    if (prop_data && nitems > 0 && actual_format == 32) {
        property = *(unsigned long*)prop_data;
    }
    
    if (prop_data) {
        XFree(prop_data);
    }
    
    return property;
}

std::string getWindowClass(Display* display, Window window) {
    XClassHint class_hints;
    
    g_x11_error_occurred = false;
    Status result = XGetClassHint(display, window, &class_hints);
    
    if (g_x11_error_occurred || result == 0) {
        return "";
    }
    
    std::string class_name = class_hints.res_class;
    XFree(class_hints.res_name);
    XFree(class_hints.res_class);
    
    return class_name;
}

void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

std::string getWindowTitle(Display* display, Window window) {
    std::string title = "(无标题)";
    char* window_name_c = nullptr;
    Atom actual_type;
    Atom net_wm_name_atom;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop_data = nullptr;
    
    net_wm_name_atom = XInternAtom(display, "_NET_WM_NAME", False);
    
    g_x11_error_occurred = false;
    XGetWindowProperty(display, window, net_wm_name_atom, 0, 1024, False, XInternAtom(display, "UTF8_STRING", False),
                       &actual_type, &actual_format, &nitems, &bytes_after, &prop_data);
                       
    if (!g_x11_error_occurred && prop_data && nitems > 0) {
        title = std::string((char*)prop_data);
    }
    
    if (prop_data) {
        XFree(prop_data);
    }
    
    if (title == "(无标题)") {
        g_x11_error_occurred = false;
        Status result = XFetchName(display, window, &window_name_c);
        if (!g_x11_error_occurred && result != 0 && window_name_c) {
            title = std::string(window_name_c);
        }
        if (window_name_c) {
            XFree(window_name_c);
        }
    }
    
    const std::vector<std::string> separators = {" - ", " — ", " | "};
    size_t best_pos = std::string::npos;
    size_t sep_len = 0;
    
    for (const auto& sep : separators) {
        size_t pos = title.rfind(sep);
        if (pos != std::string::npos) {
            best_pos = pos;
            sep_len = sep.length();
        }
    }
    
    if (best_pos != std::string::npos) {
        std::string app_name = title.substr(best_pos + sep_len);
        trim(app_name);
        if (!app_name.empty()) {
            return app_name;
        }
    }
    
    return title;
}

/**
 * @brief 【核心修正】向指定窗口直接发送关闭请求。
 */
void closeWindow(Display* display, Window window) {
    XEvent event;
    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", True);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
    event.xclient.data.l[1] = CurrentTime;

    g_x11_error_occurred = false;
    
    // 关键修正：将事件直接发送到目标窗口，而不是根窗口。
    XSendEvent(display, window, False, NoEventMask, &event);
    
    XFlush(display);
}

// 内部版本，使用持久连接
WindowInfo* getTaskbarWindows_internal(Display* display, int* count) {
    *count = 0;
    if (!display) {
        return nullptr;
    }

    Window root = DefaultRootWindow(display);
    Atom net_client_list_atom = XInternAtom(display, "_NET_CLIENT_LIST", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop_data = nullptr;

    g_x11_error_occurred = false;
    XGetWindowProperty(display, root, net_client_list_atom, 0, 1024, False, XA_WINDOW,
                       &actual_type, &actual_format, &nitems, &bytes_after, &prop_data);

    if (g_x11_error_occurred || !prop_data) {
        if (prop_data) {
            XFree(prop_data);
        }
        return nullptr;
    }

    Window* window_list = (Window*)prop_data;
    std::vector<WindowInfo> valid_windows;
    
    {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        std::set<long> still_exist_windows;
        
        for (unsigned long i = 0; i < nitems; ++i) {
            Window win = window_list[i];
            still_exist_windows.insert(win);
            
            std::string title = getWindowTitle(display, win);
            std::string proc = getProcessNameByPid(getWindowLongProperty_internal(display, win, "_NET_WM_PID"));
            std::string wmc = getWindowClass(display, win);

            char* title_c = new char[title.length() + 1];
            std::strcpy(title_c, title.c_str());

            char* proc_c = new char[proc.length() + 1];
            std::strcpy(proc_c, proc.c_str());

            char* wmc_c = new char[wmc.length() + 1];
            std::strcpy(wmc_c, wmc.c_str());

            valid_windows.push_back({ (long)win, title_c, proc_c, wmc_c });
        }

        for (auto it = g_pendingCloseWindows.begin(); it != g_pendingCloseWindows.end(); ) {
            if (still_exist_windows.find(*it) == still_exist_windows.end()) {
                it = g_pendingCloseWindows.erase(it);
            } else {
                ++it;
            }
        }
    }

    XFree(prop_data);
    
    *count = valid_windows.size();
    if (*count == 0) {
        return nullptr;
    }

    WindowInfo* result_array = new WindowInfo[*count];
    for (int i = 0; i < *count; ++i) {
        result_array[i] = valid_windows[i];
    }
    
    return result_array;
}

void workerLoop() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        if (g_debugLoggingEnabled) {
            std::cerr << "[库日志] 错误: 无法打开 X display。" << std::endl;
        }
        g_isRunning = false;
        return;
    }

    XErrorHandler old_handler = XSetErrorHandler(x11ErrorHandler);

    while (g_isRunning) {
        int count = 0;
        WindowInfo* windows = getTaskbarWindows_internal(display, &count);

        if (count > 0 && windows) {
            {
                std::lock_guard<std::mutex> lock(g_dataMutex);
                if (!g_blacklist.empty()) {
                    for (int i = 0; i < count; ++i) {
                        if (g_pendingCloseWindows.count(windows[i].id) > 0) {
                            continue;
                        }
                        bool should_block = false;
                        for (const auto& blockedName : g_blacklist) {
                            if (std::strcmp(windows[i].process_name, blockedName.c_str()) == 0 ||
                                std::strcmp(windows[i].wm_class, blockedName.c_str()) == 0) {
                                should_block = true;
                                break;
                            }
                        }
                        if (should_block) {
                            if (g_debugLoggingEnabled) {
                                std::cout << "[库日志] 自动拦截匹配到黑名单的窗口: " << windows[i].title << std::endl;
                            }
                            closeWindow(display, (Window)windows[i].id);
                            g_pendingCloseWindows.insert(windows[i].id);
                        }
                    }
                }
            }
            FreeWindowInfoArray(windows, count);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    XSetErrorHandler(old_handler);
    XCloseDisplay(display);
}

} // 结束匿名命名空间

// --- API 函数实现 ---
void StartMonitoring() {
    if (g_isRunning) {
        return;
    }
    g_isRunning = true;
    g_workerThread = std::thread(workerLoop);
}

void StopMonitoring() {
    if (!g_isRunning) {
        return;
    }
    g_isRunning = false;
    if (g_workerThread.joinable()) {
        g_workerThread.join();
    }
    std::lock_guard<std::mutex> lock(g_dataMutex);
    g_blacklist.clear();
    g_pendingCloseWindows.clear();
}

void EnableDebugLogging(bool enable) {
    g_debugLoggingEnabled = enable;
}

// 在 popup_manager.cpp 中...

void CloseWindowById(long window_id) {
    if (window_id == 0) {
        return;
    }

    if (g_debugLoggingEnabled) {
        std::cout << "[库日志] 收到独立关闭指令，窗口ID: 0x" << std::hex << window_id << std::dec << std::endl;
    }

    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        if (g_debugLoggingEnabled) {
            std::cerr << "[库日志] 错误: CloseWindowById 无法打开 X display。" << std::endl;
        }
        return;
    }

    // 激活我们的错误安全网
    XErrorHandler old_handler = XSetErrorHandler(x11ErrorHandler);
    
    // 【修改点 1】在调用危险操作前，重置错误标志
    // 这是为了确保我们检查的是本次操作产生的错误，而不是之前残留的
    g_x11_error_occurred = false;

    // 执行关闭窗口的操作
    closeWindow(display, (Window)window_id);
    
    // 【新增点】在这里检查错误标志是否被设置
    // XFlush() 是一个好习惯，它确保所有挂起的请求（包括错误）都被处理
    XFlush(display); 
    if (g_x11_error_occurred) {
        // 如果标志为 true，说明上面的 closeWindow 操作失败了
        if (g_debugLoggingEnabled) {
            std::cerr << "[库日志] 警告: 尝试关闭一个无效或已消失的窗口ID (0x" 
                      << std::hex << window_id << std::dec << ")。" << std::endl;
        }
    }

    // 恢复旧的错误处理器
    XSetErrorHandler(old_handler);

    // 关闭并释放这个临时连接
    XCloseDisplay(display);

    // (可选) 如果监控正在运行，我们也应该更新 pending 列表
    if (g_isRunning) {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        // 即使ID是错误的，也将其加入待关闭列表，
        // 这样后台线程在下一次刷新时就不会再尝试处理这个无效ID
        g_pendingCloseWindows.insert(window_id);
    }
}

void SetBlacklist(const char* blacklist[]) {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    g_blacklist.clear();
    if (blacklist) {
        for (int i = 0; blacklist[i] != nullptr; ++i) {
            g_blacklist.push_back(blacklist[i]);
        }
    }
    if (g_debugLoggingEnabled) {
        std::cout << "[库日志] 黑名单已更新，包含 " << g_blacklist.size() << " 个条目。" << std::endl;
    }
}

const char** GetBlacklist(int* count) {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    *count = g_blacklist.size();
    if (*count == 0) {
        return nullptr;
    }
    const char** array = new const char*[*count + 1];
    for (int i = 0; i < *count; ++i) {
        char* str = new char[g_blacklist[i].length() + 1];
        std::strcpy(str, g_blacklist[i].c_str());
        array[i] = str;
    }
    array[*count] = nullptr;
    return array;
}

WindowInfo* GetTaskbarWindows(int* count) {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        *count = 0;
        return nullptr;
    }
    XErrorHandler old_handler = XSetErrorHandler(x11ErrorHandler);
    WindowInfo* result = getTaskbarWindows_internal(display, count);
    XSetErrorHandler(old_handler);
    XCloseDisplay(display);
    return result;
}

void FreeWindowInfoArray(WindowInfo* array, int count) {
    if (!array || count == 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        delete[] array[i].title;
        delete[] array[i].process_name;
        delete[] array[i].wm_class;
    }
    delete[] array;
}

void FreeStringArray(const char** array, int count) {
    if (!array || count == 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        delete[] array[i];
    }
    delete[] array;
}