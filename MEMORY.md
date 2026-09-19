# 项目记忆

最后更新：2026-09-20（Asia/Hong_Kong）

## 协作约定

- 开始任务和修改代码前阅读本文件；完成一组代码修改后更新。若记忆与代码、Git 状态或最新要求冲突，以后三者为准。
- 用户偏好中文；`include/unp/` 是项目重点，`src/main.cpp` 主要用于练习和验证。代码优先保证可读、明确、正确和跨平台。

## 当前结构

- 项目是 C++23 的 header-only `unp` INTERFACE 库；Windows 链接 `ws2_32`，当前无第三方依赖。
- `include/unp.h` 是网络总入口；窄头文件为 `unp/socket.h`、`address.h`、`operations.h`、`io.h` 和 `fs/file.h`。进程接口正在 `unp/sys/process.h` 中开发。
- 网络公开接口已压平到 `unp::`；仅文件辅助保留 `unp::fs`，内部实现放在 `unp::detail`。不再使用冗余的 `unp::net`、`unp::net::io` 或 `unp/unp.h`。
- `src/main.cpp` 是绑定 `0.0.0.0:13` 的 IPv4 TCP Daytime 服务；当前每个连接移动到独立 detached `std::thread`，测试处理函数等待 2 秒后用 `unp::writen` 发送时间。

## 关键决定

- `sockfd` 只负责句柄所有权和创建；原始 Socket 操作保持自由函数，失败通过返回值及 `last_socket_error()` 报告。
- 流式接口采用 UNP 风格名称：`readn`、`writen`、`readline`；旧的重复接口 `send_all` 已移除。`writen` 返回 `std::expected<std::size_t, std::error_code>`。
- `buffered_reader` 借用 Socket；`reset(socket)` 可换绑并清空缓存。移动会转移绑定和未读缓存，并将源对象置为未绑定。
- `auto_sockaddr` 只接受左值，避免悬空 view。`unp::fs::open_file` 返回由调用者用 `std::fclose` 关闭的 `FILE*`。

## 验证与待办

- 2026-09-16：扁平接口及 `writen` 在 MSVC 19.51、MinGW GCC 16.2 下干净构建，CTest 均为 1/1；GCC/Clang 独立包含 `unp.h` 通过。
- 本地启动 Daytime 服务并连接 `127.0.0.1:13`，已实际收到完整时间行。
- Ubuntu WSL 2 可从 Codex 调用；GCC 14.3 已严格编译并运行 `process_tests`。真实 `fork_process()` 测试中，子进程 `_exit(23)`，父进程成功 `wait()` 并取得退出码，回收后对象失效，第二次等待返回 `bad_file_descriptor`。信号终止路径及 macOS 仍待验证；端口 13 在类 Unix 系统通常需要相应权限。
- CMake 已注册 `process.h` 并增加独立的 `process_tests`。`process::wait()` 返回 `process_exit_status`，保留 Windows 完整退出码，并在 POSIX 下区分正常退出和信号终止；MinGW GCC 16.2 全量构建及 2/2 CTest、MSVC 19.51 `process_tests` 均通过。
- `process::spawn_process()` 已覆盖 Windows `CreateProcessW` 与 Linux `fork`/`execvp`。Linux 当前使用 `pipe2(O_CLOEXEC)` 将 exec 失败的 `errno` 回传父进程；Windows 使用 `detail::quote_windows_argument()` 按常见 CRT 规则处理空参数、空白、双引号及反斜杠。当前暂不扩展 macOS/BSD。
- 2026-09-19：MSVC 强制全量构建及 2/2 CTest 通过；Ubuntu WSL/GCC 14.3 强制全量构建无警告且 2/2 CTest 通过。Windows 与 POSIX 的 spawn 测试均实际启动子进程并取得退出码 23。`connect_socket()` 的有无符号长度检查也已无 GCC 警告。信号终止路径及 macOS 仍待验证。
- 2026-09-19：`tests/test_process.cpp` 新增不存在程序必须返回 `no_such_file_or_directory` 的跨平台测试；`python tests/run_process_tests.py` 会自动加载当前 MSVC 环境并运行 Windows CTest，再用 WSL/GCC 严格编译运行同一测试。`write`、`read`、`waitpid` 的 `EINTR` 重试已接入返回值；读取失败会保留对应 `errno`，错误管道短读返回 `io_error`。最新 MSVC 19.51 与 WSL GCC 14.3 验证均通过，`git diff --check` 无空白错误。
- 2026-09-19：修复 `unp.h` 先包含 Winsock 后，Windows `max` 宏破坏 `numeric_limits<int>::max()` 的问题，改用宏安全的 `(std::numeric_limits<int>::max)()` 并显式转换为 `size_t`。线程版 `Native` 已由 MSVC 19.51 构建通过，WSL/GCC 14.3 使用 `-pthread -Wall -Wextra -Wpedantic` 编译通过。
- 2026-09-19：`unp/socket.h` 新增返回 `expected<string, error_code>` 的 `get_hostname()`，系统调用仍是全局 `::gethostname()`；补齐直接依赖的 `<expected>` 与 `<system_error>`。当前 `main.cpp` 已由 MSVC `/W4` 和 WSL/GCC `-Wall -Wextra -Wpedantic -pthread` 干净编译。
- 2026-09-20：进程创建的系统细节已拆到 `unp::detail`：Windows 位于 `detail/windows_process.h`，Linux 位于 `detail/linux_process.h`。`process` 内部通过平台 `using created_process = ...` 统一 Windows `created_process` 与 Linux `fork_result`，只负责公开 API、原生结果接管和生命周期；旧的公开 `process::create_process()`、`fork_id()` 及 adopt 接口已移除。CMake 已登记两个 detail 头。
- 2026-09-20：`python tests/run_process_tests.py` 在 MSVC 19.51 和 WSL/GCC 14.3 下均通过，覆盖正常 spawn、退出码、fork/wait 及不存在程序的同步错误回传；Windows 和 WSL 的完整 `src/main.cpp` 也已分别构建通过。macOS/BSD 仍明确不在当前范围内。
