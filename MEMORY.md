# 项目记忆

最后更新：2026-09-15（Asia/Hong_Kong）

## 协作约定

- 开始任务和修改代码前阅读本文件；完成一组代码修改后更新。若记忆与代码、Git 状态或最新要求冲突，以后三者为准。
- 用户偏好中文；`include/unp/` 是项目重点，`src/main.cpp` 主要用于练习和验证。代码优先保证可读、明确、正确和跨平台。

## 当前结构

- 项目是 C++23 的 header-only `unp` INTERFACE 库；Windows 链接 `ws2_32`，当前无第三方依赖。
- `include/unp.h` 是兼容入口，`include/unp/unp.h` 是新总入口。
- `unp::net`：`socket.h`（`sockfd`、`socket_env`、平台类型/错误）、`address.h`、`operations.h`。
- `unp::net::io`：`io.h`（`send_all`、`readn`、`buffered_reader`、`readline`）；`unp::fs`：`file.h`（`open_file`）；`unp::net::detail`：内部错误处理。
- `src/main.cpp` 是 IPv4 TCP Daytime 客户端：接收一个 IP 参数，连接端口 `13` 并用 `buffered_reader`/`readline` 读取时间；目标地址必须实际运行 Daytime 服务。`tests/test_unp.cpp` 直接包含窄头文件测试各模块。

## 关键决定

- `sockfd` 只负责句柄所有权和创建，原始网络操作保持自由函数；`create` 失败返回无效对象，`release()` 为 `[[nodiscard]]`，`reset(get())` 安全。
- `send_socket` 表示一次系统调用，`send_all` 负责完整发送。`readn` 尽量读取指定长度，EOF 时允许短返回，并处理调用中断、空指针和长度溢出。
- `buffered_reader` 借用一个 Socket 且不负责关闭；`reset(socket)` 可安全换绑并清空缓存。移动操作会转移 Socket 绑定和未读缓存，并将源对象置为未绑定。两个 `readline` 达到长度上限时返回本次读取长度，剩余数据留给下次调用。
- `auto_sockaddr` 只接受左值，避免悬空 view。`open_file` 返回由调用者用 `std::fclose` 关闭的 `FILE*`。
- 接口已按模块和命名空间拆分；旧的 `include/unp.h` 继续兼容现有包含路径。

## 验证与待办

- 模块拆分及行读取修正后，MSVC 19.51 和 MinGW GCC 16.2 的干净构建均通过，CTest 均为 1/1；测试覆盖截断续读、换行、EOF、reader 解绑及带未读缓存的移动构造/赋值，通过 `git diff --check`。
- Daytime 客户端已修正反向的 Socket 有效性判断；MSVC 构建和 CTest 通过，并使用临时本地 `127.0.0.1:13` 服务验证可完整收到时间行。本机平时没有该服务时，连接会正确返回 `10061`。
- POSIX/Linux/macOS 尚未实际编译；当前环境枚举 WSL 返回 `E_ACCESSDENIED`。
