# 项目记忆

最后更新：2026-09-13（Asia/Hong_Kong）

## 协作约定

- 用户要求：在本文件夹维护记忆文件，每次更改代码前简要阅读，保持不同对话之间的上下文连贯。
- 阅读和更新流程见根目录 `AGENTS.md`。本文件只保存项目交接信息，实际状态需结合代码和 Git 检查。
- 用户偏好中文交流；代码优先可读、可维护、正确，避免不必要的抽象，控制语句使用花括号。

## 当前项目

- 路径：`D:\PROJECTS\CPP\Native`。
- 项目名及练习可执行目标：`Native`；CMake 最低版本声明为 4.2，C++ 标准设置为 23。
- `include/unp.h` 是项目重点：`unp` 命名空间下的 Windows/POSIX Socket 封装，包含可移动、不可拷贝的 RAII `sockfd`、Windows 网络环境初始化 `socket_env`、地址转换、bind/accept/send/recv 及完整发送等辅助函数。
- `src/main.cpp`：TCP 时间服务示例，默认端口 `8080`，支持一个服务名或端口参数；通过 `getaddrinfo` 遍历候选地址并绑定、监听，接受连接后发送当前时间。IPv6 候选会尝试开启双栈。
- CMake 将 `unp` 建模为 INTERFACE 库，Windows 下显式传递 `ws2_32` 链接依赖，并为 MSVC/GNU 类编译器开启较严格警告。vcpkg 清单当前没有第三方依赖。
- 存在 `cmake-build-debug/`、`cmake-build-debug-visual-studio/`、`out/` 等本地构建目录；目录存在不代表当前源码构建通过。
- `tests/test_unp.cpp` 通过 CTest 验证地址转换、借用地址 view、RAII 移动所有权和本地回环 Socket I/O。

## Git 状态快照

以下是建立记忆时的快照，后续操作前应重新检查：

- 分支：`main`；HEAD：`eaa857c`（`Add Windows-compatible UNP header`）；前一提交：`78810c7`（`Initial commit`）。
- 已有未提交修改：`include/unp.h`。
- 已有未跟踪内容：`AGENTS.md`、`MEMORY.md`、`CMakeLists.txt`、`src/`、`tests/`、`vcpkg-configuration.json`、`vcpkg.json`。
- 头文件中的进行中改动包括：地址类型 concept 和辅助函数、字节序常量、将 Socket 创建接口改为静态工厂 `sockfd::create`。
- 本次新增 `AGENTS.md` 和 `MEMORY.md`，未执行 Git 提交。

## 设计决定与待核验事项

- `sockfd::create` 与其余系统调用包装保持一致：不抛异常，失败时返回无效 RAII 对象，由调用者通过 `valid()`/布尔转换和 `last_socket_error()` 处理。这允许地址候选循环继续回退。
- `send_socket` 保留“一次系统调用”的语义；需要完整发送时使用 `send_all_socket`，它处理部分发送和中断重试。
- `auto_sockaddr` 只接受左值，避免从临时地址对象返回立即悬空的非拥有 view。
- Windows 已使用 MSVC 19.51 和 MinGW GCC 16.2 实际编译、链接并运行测试；Linux/macOS POSIX 分支尚未实际编译。尝试枚举 WSL 时环境返回 `E_ACCESSDENIED`。
- 旧的 CLion/vcpkg 构建目录在当前沙箱中重新配置时，vcpkg 无权清理 `C:\tool-chains\vcpkg` 下的旧包；不使用 vcpkg 的全新构建已验证成功。

## 最近工作

- 2026-09-13：重点修整 `unp.h`：统一 `sockfd::create` 错误语义，防止临时地址 view，校验空缓冲区/空地址字符串，让 `pton` 同步填写地址族，增加 `send_all_socket` 和 `socket_address_view` 的 bind 重载，并整理头文件依赖。`main.cpp` 改用完整发送和线程安全的本地时间转换。CMake 建立 `unp` INTERFACE 库、显式链接 `ws2_32`、改用 C++23，并新增 `tests/test_unp.cpp`。MSVC 与 MinGW 构建和 CTest 均通过；练习服务的本地回环连接也成功返回时间行。未执行 Git 提交。
- 2026-09-13：用户修正 `ntop` 的变量名、异常声明和 IPv4/IPv6 缓冲区，并移除重复的 `send_socket` 定义。使用 Visual Studio Developer PowerShell 执行 `cmake --build cmake-build-debug`，MSVC 编译和链接成功。只更新记忆，未修改业务代码。
- 2026-09-13：读取项目文件、Git 状态和头文件差异，建立本记忆及项目级阅读/更新约定。未修改业务代码，未运行构建或测试。
