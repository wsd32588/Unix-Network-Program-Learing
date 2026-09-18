# 项目记忆

最后更新：2026-09-17（Asia/Hong_Kong）

## 协作约定

- 开始任务和修改代码前阅读本文件；完成一组代码修改后更新。若记忆与代码、Git 状态或最新要求冲突，以后三者为准。
- 用户偏好中文；`include/unp/` 是项目重点，`src/main.cpp` 主要用于练习和验证。代码优先保证可读、明确、正确和跨平台。

## 当前结构

- 项目是 C++23 的 header-only `unp` INTERFACE 库；Windows 链接 `ws2_32`，当前无第三方依赖。
- `include/unp.h` 是网络总入口；窄头文件为 `unp/socket.h`、`address.h`、`operations.h`、`io.h` 和 `fs/file.h`。进程接口正在 `unp/sys/process.h` 中开发。
- 网络公开接口已压平到 `unp::`；仅文件辅助保留 `unp::fs`，内部实现放在 `unp::detail`。不再使用冗余的 `unp::net`、`unp::net::io` 或 `unp/unp.h`。
- `src/main.cpp` 是绑定 `0.0.0.0:13` 的 IPv4 TCP Daytime 服务，接受连接后用 `unp::writen` 发送时间。

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
- Ubuntu CMake 4.2.3 配置成功；完整 Linux 构建当前被 `address.h` 缺少 POSIX `<netdb.h>` 阻塞，`operations.h` 另有两处无符号地址长度与零比较的警告。临时预包含 `<netdb.h>` 后三个目标均无其他编译错误。
