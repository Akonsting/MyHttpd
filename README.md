# MyHttpd

这是我在学习 TinyHttpd（版本 0.1.0）源码之后的独立复现项目，旨在巩固对 HTTP 协议、静态文件服务以及 CGI 动态生成原理的理解。

---

## 项目结构

```
MyHttpd/
├── Makefile
├── myhttpd.c          # 服务器主程序，实现了静态与 CGI 服务，包括 GET/POST 支持
├── simpleclient.c     # 测试用的简易 HTTP 客户端
└── wwwroot/           # 静态资源与 CGI 脚本存放目录
    ├── index.html     # 网站首页示例
    ├── check.cgi      # 示例 CGI 脚本：表单校验
    └── color.cgi      # 示例 CGI 脚本：动态输出颜色
```

---

## 功能简介

* **静态文件服务**：根据请求 URL 映射到 `wwwroot` 目录下对应路径，将文件内容返回给客户端，支持常见 MIME 类型。
* **CGI 支持**：通过 `fork` + 管道与子进程交互，执行 `wwwroot` 下的可执行脚本。支持：

  * **GET** 请求（带 `?query`）
  * **POST** 表单数据（正确解析 `Content-Length` 并通过管道传递）
* **错误处理**：对 404、501、400、500 等常见 HTTP 错误返回对应的状态码与简单 HTML 提示。
* **可配置文档根与监听端口**：默认文档根为项目内 `wwwroot`，监听系统分配端口；可在 `setupListener` 中修改。

---

## 编译与运行

1. **编译**

   ```bash
   make
   ```

2. **运行服务器**

   ```bash
   ./myhttpd
   ```

   启动后会输出：

   ```
   Server listening on port <port>
   ```

3. **访问**

   * 打开浏览器，访问 `http://localhost:<port>/` 查看首页。
   * 提交表单后，CGI 脚本会处理并返回结果。

4. **清理**

   ```bash
   make clean
   ```

---

## 我的学习心得

通过这一项目的复现，我对下面知识点有了更深刻的理解：

* **socket 编程**：`socket`、`bind`、`listen`、`accept` 全流程。
* **HTTP 协议**：请求行解析、请求头读取、状态行与响应头构造。
* **静态与动态服务**：CGI 原理、管道通信与子进程执行、环境变量传参。
* **多进程与 I/O 重定向**：`fork`、`dup2`、`pipe`、`waitpid`。
* **工程组织**：Makefile 管理、`.gitignore` 使用、GitHub 版本托管。

---

感谢 TinyHttpd 项目提供的示例代码，感谢自己坚持手动复现，加深了对底层 HTTP 服务实现的理解。如有讨论或建议，请在 Issues 中留言！
