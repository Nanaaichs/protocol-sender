# 课程协议文件兼容与验收

项目同时支持两种 XML 写法：

1. 原项目的属性式写法，例如 `<field name="id" dataType="HEX" .../>`；
2. 课程样例的子元素写法，例如 `<field><fieldName>id</fieldName>...</field>`。

课程原始文件是无扩展名的 UTF-8 XML。项目保留原文件，并在 `data` 目录提供带 `.xml` 扩展名的可复现副本：

- `course_protocol_example_1.xml`：2 个字段，总长 32 bit；
- `course_protocol_example_2.xml`：5 个字段，总长 96 bit。

## 课程格式的根级信息

| XML 元素 | 内部含义 | 当前用途 |
| --- | --- | --- |
| `sourceIP` | 建议源 IP | 解析并保留，不强制绑定，避免样例地址不存在时无法发送 |
| `destIP` | 默认目标 IP | 选择课程文件后自动填入界面，仍可手工修改 |
| `sourcePort` | 建议源端口 | 解析并保留，不强制绑定 |
| `destPort` | 默认目标端口 | 选择课程文件后自动填入界面 |
| `protoHead` | 协议头描述 | 原样例为空，当前仅保留 |
| `nType` | 0 正常，1 报文头 | 解析并保留 |
| `system` | 系统名称 | 解析并保留 |

## 字段位布局

课程格式中：

- `bitIndex` 是字段起始位；
- `length` 是字段位长；
- `loopEnd` 是字段结束位；
- 必须满足 `loopEnd = bitIndex + length`；
- 字段不得重叠；
- bit 0 按网络字节序映射到第一个字节的最高位。

最后一条是因课程样例没有声明位序而采用的明确工程假设。如果教师后续给出小端或 LSB-first 规则，必须同步修改生成器和测试向量。

课程格式会生成真正的二进制 UDP 数据，而不是 `name=value` 文本。界面和 SQLite 日志使用大写、空格分隔的十六进制摘要，例如：

```text
HEX: 12 34 0A FF
```

## 如何用 Packet Sender 验收

1. 启动 Packet Sender，监听 UDP `39001`。
2. 启动本项目，点击“选择协议”。
3. 选择 `data/course_protocol_example_1.xml` 或 `data/course_protocol_example_2.xml`。
4. 界面会填入文件中的 `192.168.0.2:2000`。本机验收时必须再手工改成 `127.0.0.1:39001`。
5. 设置 `1 Hz` 和 `3 条`，点击“开始发送”。
6. 在 Packet Sender 中查看 **HEX** 而不是 ASCII。

预期：

- 样例 1 每条报文为 4 字节，前两字节固定为 `12 34`；
- 样例 2 每条报文为 12 字节，前两字节固定为 `12 34`；
- 其余字节根据 `minimum`/`maximum` 随机生成；
- 项目预览中的 HEX 应与 Packet Sender 接收的 HEX 相同。

## 兼容性边界

- 旧属性式 XML 继续生成 UTF-8 `name=value` 文本；
- 课程子元素式 XML 生成按位打包的二进制报文；
- `isSelected` 在课程文件中表示解析/存储关注状态，不会删掉它在线缆报文中的位区间；
- 课程样例未规定浮点字节序、字符编码或 `protoHead` 非空时的格式；当前分别使用 IEEE 754 网络字节序、UTF-8 和“仅保留元数据”策略。
