# 项目记忆

最后更新：2026-11-02（Asia/Hong_Kong）

## 协作约定

- 修改前阅读本文件并检查实际文件与 Git 状态；以代码和最新用户要求为准，保留用户的未提交修改。
- 用户偏好中文和薄封装；原生 Socket 操作保持自由函数，不为单个系统调用结果额外引入持有所有权的结构体。

## 当前状态

- 项目主线是 C++23 header-only 网络库 `unp`；公开网络 API 位于 `unp::`，内部平台实现位于 `unp::detail`。`src/main.cpp` 是多线程 IPv4 TCP Daytime 练习服务器。
- `sockfd` 仅管理 Socket 所有权。`accept_socket(listener)` 直接返回 `sockfd`；三参数重载在内部初始化地址长度。`readn`/`writen`/`readline`、`buffered_reader`、`pton(expected)` 已是当前接口。
- 现有进程代码已封口为独立 `process_compat` INTERFACE 目标，不再由 `unp.h` 或 `unp` 目标导出。功能冻结在现有 `fork`/spawn/`wait` 范围；若要增加 detach、重定向、环境变量等能力，应抽成独立“进程兼容”项目，不再扩张本网络库。
- `process` 析构不终止也不等待子进程；POSIX 调用者必须在丢弃或覆盖最后一个 handle 前调用 `wait()`，或自行安排外部 reaper。不能在对象析构中修改进程级 `SIGCHLD` 策略。
- Linux spawn 使用 `pipe2(O_CLOEXEC)` 同步回传 exec 错误，精确处理错误码读写；状态管道异常或短读时会终止并回收已 fork 子进程，避免丢失 PID。Windows 参数引用按 CRT 规则转义。macOS/BSD 暂不在范围内。
- 上述最近重构与测试改动尚未提交；基线提交是 `5e346bb` 。

## 本次验证

- 2026-11-02：Windows/MinGW 重新以 `-Wall -Wextra -Wpedantic` 编译 `Native`、`unp_tests` 和 `process_tests`；两个测试程序均通过。受限沙箱内 loopback `connect` 返回 `WSAEACCES`，同一测试在沙箱外通过。
- 2026-11-02：MSVC 复验被本机 `C1902`（PDB 管理器版本不匹配）阻断，属于工具链安装状态；未将本次 MSVC 构建记录为通过。
- 2026-09-21：Windows/MSVC 19.51 全量 CMake 构建通过；网络头与进程兼容头分别完成独立翻译单元检查，CTest 2/2 通过。
- 2026-09-21：Ubuntu WSL/GCC 14.3 在全新 `/tmp/unp-native-closeout-20260921` 构建目录中以 `-Wall -Wextra -Wpedantic` 全量构建通过；`Native`、两组头检查和 CTest 2/2 均通过。
- `git diff --check` 通过（仅有 Git 的 LF/CRLF 转换提示）。

## 已知边界与待办

- `tests/run_process_tests.py` 只验证冻结的 `process_compat` 头和 `process_tests`，不是全量 runner；网络库继续通过 CMake 全量构建/CTest 验证。
- 待补 POSIX 信号终止、`get_hostname`/`get_peer_name` 及 `accept_socket(listener)` 简化重载的针对性测试。
- `writen(Container)` 标记 `noexcept`，但对任意自定义 Container 的 `data`/`size` 并无 noexcept 约束；应移除该承诺或收紧约束。CMake 也尚未显式链接 `Threads::Threads`，虽当前 WSL 已成功链接，仍属 Unix 可移植性债务。
