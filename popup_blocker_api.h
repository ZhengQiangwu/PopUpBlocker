#ifndef POPUP_MANAGER_API_H
#define POPUP_MANAGER_API_H

#ifdef __cplusplus
extern "C" {
#endif

// 定义一个结构体，用于在C++和C#之间传递窗口信息
struct WindowInfo {
    long id;
    const char* title;
    const char* process_name;
    const char* wm_class;
};


/**
 * @brief 启动后台的自动黑名单拦截服务。
 * 立即返回，并在一个独立的后台线程中开始工作。
 */
void StartMonitoring();


/**
 * @brief 停止后台服务并清理所有资源。
 */
void StopMonitoring();


/**
 * @brief 启用或禁用库的内部调试日志。
 */
void EnableDebugLogging(bool enable);


/**
 * @brief 根据窗口ID关闭一个特定的窗口（用于手动拦截）。
 * @param window_id 要关闭的窗口的ID。
 */
void CloseWindowById(long window_id);


/**
 * @brief 设置自动拦截的黑名单。
 * 后台服务将自动关闭匹配此列表的窗口。
 * @param blacklist 一个以NULL结尾的字符串数组 (const char*[])。
 */
void SetBlacklist(const char* blacklist[]);


/**
 * @brief 获取当前库内部维护的黑名单列表。
 * @param count [out] 用于接收列表中的条目数量。
 * @return 返回一个字符串数组 (const char**)，包含所有黑名单条目。
 * @note 调用方必须在之后调用 FreeStringArray 来释放返回的数组内存。
 */
const char** GetBlacklist(int* count);


/**
 * @brief 【核心】获取当前任务栏中所有应用程序窗口的列表。
 * @param count [out] 用于接收列表中的窗口数量。
 * @return 返回一个 WindowInfo 结构体数组。
 * @note 调用方必须在之后调用 FreeWindowInfoArray 来释放返回的数组内存。
 */
WindowInfo* GetTaskbarWindows(int* count);
/**
 * @brief 获取当前任务栏中所有应用程序窗口的标题列表。
 * @param count [out] 用于接收列表中的标题数量。
 * @return 返回一个以NULL结尾的字符串数组 (const char**)，包含所有窗口标题。
 * @note 调用方必须在之后调用 FreeStringArray 来释放返回的数组内存。
 */
const char** GetTaskbarWindowTitles(int* count);

/**
 * @brief 释放由 GetTaskbarWindows 返回的内存。
 * @param array 指向 GetTaskbarWindows 返回的数组的指针。
 * @param count GetTaskbarWindows 返回的数量。
 */
void FreeWindowInfoArray(WindowInfo* array, int count);

/**
 * @brief 释放由 GetBlacklist 或 GetTaskbarWindowTitles 返回的字符串数组内存。
 * @param array 指向返回的数组的指针。
 * @param count GetBlacklist 或 GetTaskbarWindowTitles 返回的数量。
 */
void FreeStringArray(const char** array, int count);


#ifdef __cplusplus
}
#endif

#endif // POPUP_MANAGER_API_H