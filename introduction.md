项目文件介绍：src是给程序员看的，build是给编译器用的，dist是给最终用户运行的。

总体数据流如下
XML文件->ProtocolParser解析和校验（读懂XML）->ProtocolDefinition内存对象（形成协议配置）->DataGenerator生成字段值和文本载荷

Qt是一个跨平台的C++应用开发框架，不是操作系统，不是编译器，也不只是一个GUI库，它所处的位面是C++标准库之上，业务应用程序之下。
最早程序员直接调用操作系统 GUI；后来 MFC 等框架把原生 API 封装起来；再后来 Qt、GTK、wxWidgets 解决跨平台问题；之后 Qt 又从 GUI 库扩展成完整应用框架；进入现代以后，Qt Quick、Electron、Flutter 等进一步把 UI 开发推向声明式、GPU 渲染和多端统一。
最初的桌面程序基本直接调用操作系统提供的原生GUI API，windows是Win32 API，Unix/Linux世界后来有X Windows System，Mac有自己的图形接口。这个极端开发者要亲自处理窗口句柄、消息循环、绘制、事件分发，代码和操作系统绑得很死。后来到了1990年代，出现了把原生GUI包装成更好用的面向对象框架的产品，Windows阵营中典型的是MFC，本质是对Win32 API的C++封装，随着Unix、Linux、Windows多平台软件越来越多，同一套代码不能跨平台就变得越来越重要，于是1990年代中期开始，Qt、GTK、wxWidgets这类框架进入主流视野。Qt 走的是一条非常典型的路线：不仅解决“窗口怎么画”，还逐渐把字符串、容器、文件、线程、网络、数据库、XML、插件机制等一起做进框架里。于是它从一个 GUI toolkit，逐渐演化成了一个完整的 C++ 应用开发平台。Qt、GTK、wxWidgets 虽然经常被放在一起比较，但它们从一开始理念就不完全一样。

wxWidgets 比较强调“尽量调用操作系统原生控件”。也就是说 Windows 上按钮尽量真的是 Windows 按钮，macOS 上尽量使用 macOS 原生控件。所以它更像一层“跨平台包装”。

GTK 则和 Unix/Linux 桌面生态联系更深，后来尤其成为 GNOME 的核心技术路线。它最初是 GIMP 的工具包，名称本来就是 GIMP Toolkit，后来才发展成通用 GUI 框架。

Qt 则逐渐走得更“大而全”。它自己提供大量抽象，因此不仅能做 Windows/Linux/macOS 桌面软件，还越来越深入嵌入式、车载、工业控制等场景。
所以 2000 年之后，Qt 的定位已经逐渐不是单纯：

“和 MFC 一样的 GUI 库。”

而更接近：

“C++ 应用程序开发平台。”




XML 是文本形式的协议描述文件，ProtocolParser 负责读取、解析并校验 XML，然后将其中的协议规则转换为 C++ 方便使用的数据结构，也就是 ProtocolDefinition 内存对象。随后 DataGenerator 根据 ProtocolDefinition 中保存的字段类型、范围等规则生成合法数据。UdpSendController 负责按照发送次数、频率、目标 IP 和端口等参数调度 UDP 发送。发送记录为了后续查询和回溯，由 TransmissionRepository 写入 SQLite 数据库。MainWindow 负责 GUI 展示以及接收用户操作。
为了测试程序的 UDP 性能，项目不使用正常的 GUI/数据库业务流程，而是在同一台计算机上建立一个 UDP 发送端和接收端。DataGenerator 连续生成带序号的数据包，发送端将其发送到本机临时分配的 UDP 端口，接收端记录实际收到的数据包，再根据序号检查重复、异常和丢失情况，并计算测试耗时、吞吐量和丢包率。
自测结束后，使用 Packet Sender 作为独立 UDP 接收端，将本程序生成的数据发送至 Packet Sender 监听端口，通过比对发送端生成的 payload 和 Packet Sender 实际接收的数据，验证 UDP 发送功能、目标地址、端口和数据内容的正确性。


dist中的内容
protocol_sender.exe不是一个什么都自己带着的单文件程序，运行时依赖Qt、编译器运行库、图像插件、数据库驱动等很多动态库。
其中文件的关系是，protocol_sender.exe运行时需要Qt5Core.dll、Qt5Gui.dll、Qt5Widgets.dll...，某些功能又需要插件，包括platforms、imageformats、sqldrivers、bearer、iconengines。
bearer的作用是网络承载/网络状态相关插件，iconengines的作用是图标格式支持，比如SVG图标，imageformats的作用是图片格式支持，例如JPG、GIF、SVG等，platforms作用是Qt和操作系统窗口系统之间的接口，sqldrivers是数据库驱动。


protocol_sender.exe是主程序，Qt5Core.dll是Qt最基础的核心功能，Qt5Gui.dll是GUI、字体、图像等底层功能，Qt5Widgets.dll是按钮