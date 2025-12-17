# USB数据传输模块集成指南

## 概述

USB数据传输模块为Plouton SMM框架提供了比串口和内存日志更高带宽的数据传输方案。该模块允许将SMM层的数据直接写入USB存储设备，支持多种数据格式，便于外部系统读取和分析。

## 主要优势

### 1. 高带宽传输
- **USB 2.0**: 理论带宽480Mbps (vs 串口115Kbps = 4000+倍提升)
- **USB 3.0**: 理论带宽5Gbps (vs 串口 = 40000+倍提升)
- **实际应用**: 即使考虑SMM环境限制，仍可实现100-1000倍带宽提升

### 2. 跨平台兼容
- 任何有USB接口的系统都能读取数据
- 无需特殊驱动或工具
- 支持Windows、Linux、macOS

### 3. 大容量存储
- 支持GB级别的数据存储
- 自动文件轮转机制
- 环形缓冲区防止数据丢失

### 4. 结构化数据
- JSON格式：便于程序解析
- CSV格式：便于Excel处理
- 自定义格式：灵活适应不同需求

## 快速开始

### 1. 启用USB数据传输

在 `Plouton/general/config.h` 中添加：

```c
// 启用USB数据传输
#define ENABLE_USB_DATA_TRANSFER TRUE

// 配置参数
#define USB_DATA_BUFFER_SIZE      (64 * 1024)  // 64KB缓冲区
#define USB_DATA_MAX_FILE_SIZE    (1024 * 1024) // 1MB文件大小
#define USB_DATA_MAX_FILES        10             // 最大文件数
```

### 2. 初始化USB数据传输

在 `plouton.c` 的 `UefiMain` 函数中添加：

```c
#if ENABLE_USB_DATA_TRANSFER
// 初始化USB数据传输
if (!InitializeUsbDataTransfer()) {
    SerialPrintf("[PL] USB data transfer initialization failed, falling back to serial\n");
} else {
    SerialPrintf("[PL] USB data transfer initialized successfully\n");
}
#endif
```

### 3. 使用USB日志记录

替换现有的串口日志：

```c
// 旧方式 (串口)
SerialPrintf("[PL] Found target %s\n", targetName);

// 新方式 (USB + 串口)
#if ENABLE_USB_DATA_TRANSFER
USB_DATA_INFO(&gUsbDataContext, "Found target %s", targetName);
#else
SerialPrintf("[PL] Found target %s\n", targetName);
#endif
```

## 配置选项

### 数据格式选择

```c
// JSON格式 (推荐) - 结构化，便于解析
InitUsbDataTransfer(&gUsbDataContext, USB_DATA_FORMAT_JSON, TRUE);

// CSV格式 - 便于Excel处理
InitUsbDataTransfer(&gUsbDataContext, USB_DATA_FORMAT_CSV, TRUE);

// 纯文本格式 - 轻量级
InitUsbDataTransfer(&gUsbDataContext, USB_DATA_FORMAT_TEXT, TRUE);

// 二进制格式 - 高效传输大数据
InitUsbDataTransfer(&gUsbDataContext, USB_DATA_FORMAT_BINARY, TRUE);
```

### 缓冲区设置

```c
// 小缓冲区 (低延迟)
#define USB_DATA_BUFFER_SIZE      (16 * 1024)  // 16KB

// 大缓冲区 (高效率)
#define USB_DATA_BUFFER_SIZE      (256 * 1024) // 256KB
```

### 文件轮转设置

```c
// 频繁轮转 (更多文件，更少数据丢失)
#define USB_DATA_MAX_FILE_SIZE    (512 * 1024) // 512KB
#define USB_DATA_MAX_FILES        20

// 少频轮转 (更大文件，更好性能)
#define USB_DATA_MAX_FILE_SIZE    (10 * 1024 * 1024) // 10MB
#define USB_DATA_MAX_FILES        5
```

## 集成示例

### 1. 替换内存日志

```c
// 在memory_log.c中添加USB支持
#if ENABLE_USB_DATA_TRANSFER
    // 同时写入USB和内存日志
    USB_DATA_INFO(&gUsbDataContext, "Memory log: %s", formattedMessage);
#endif

// 现有内存日志代码...
MemoryLogPrint(format, ...);
```

### 2. 增强错误日志

```c
// 详细错误记录
#if ENABLE_USB_DATA_TRANSFER
    EnhancedErrorLogging("XHCI_INIT_FAILED", "initXHCI", status);
#else
    LOG_ERROR("[PL] Failed initializing XHCI\n");
#endif
```

### 3. 性能监控

```c
// 定期记录性能指标
#if ENABLE_USB_DATA_TRANSFER
    UsbPerformanceMetrics();
#endif
```

## 数据读取示例

### 1. Linux/macOS读取

```bash
# 挂载USB设备
sudo mount /dev/sdb1 /mnt/usb

# 读取数据文件
cat /mnt/usb/plouton_data_*.dat

# 解析JSON格式
python3 -c "
import json
import sys
for line in sys.stdin:
    if line.strip():
        data = json.loads(line)
        print(f\"[{data.get('level', '?')}] {data.get('message', '')}\")
"
```

### 2. Windows读取

```powershell
# 使用PowerShell读取
Get-Content "D:\plouton_data_*.dat" | 
    ConvertFrom-Json | 
    Format-Table Level, Message, Timestamp
```

### 3. 程序化处理

```python
#!/usr/bin/env python3
import json
import os
import sys
from datetime import datetime

def parse_plouton_logs(log_dir="/mnt/usb"):
    """解析Plouton日志文件"""
    for filename in sorted(os.listdir(log_dir)):
        if filename.startswith("plouton_data_") and filename.endswith(".dat"):
            filepath = os.path.join(log_dir, filename)
            print(f"Processing: {filename}")
            
            with open(filepath, 'r') as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if not line:
                        continue
                    
                    try:
                        data = json.loads(line)
                        timestamp = datetime.now().isoformat()
                        print(f"[{timestamp}] {data}")
                    except json.JSONDecodeError:
                        print(f"Line {line_num}: {line}")

if __name__ == "__main__":
    parse_plouton_logs()
```

## 性能基准

### 带宽比较

| 传输方式 | 理论带宽 | 实际带宽 | 相对性能 |
|----------|----------|----------|----------|
| 串口 (115200bps) | 115 Kbps | ~10 Kbps | 1x |
| 串口 (921600bps) | 921 Kbps | ~80 Kbps | 8x |
| USB 2.0 | 480 Mbps | ~100 Mbps | 10,000x |
| USB 3.0 | 5 Gbps | ~1 Gbps | 100,000x |

### 延迟比较

| 传输方式 | 平均延迟 | 适用场景 |
|----------|----------|----------|
| 串口 | 100-1000ms | 调试信息 |
| 内存日志 | 0ms (无外部传输) | 内部调试 |
| USB 2.0 | 10-100ms | 实时数据传输 |
| USB 3.0 | 1-10ms | 高频数据传输 |

## 故障排除

### 1. USB设备未检测到

```c
// 检查USB设备枚举
 EFI_HANDLE *Handles;
 UINTN HandleCount = 0;
 Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);
 SerialPrintf("Found %d block devices\n", HandleCount);
```

### 2. 文件系统错误

```c
// 检查文件系统状态
 EFI_FILE_PROTOCOL *Root;
 Status = FileSystem->OpenVolume(FileSystem, &Root);
 if (EFI_ERROR(Status)) {
     SerialPrintf("Filesystem error: %r\n", Status);
 }
```

### 3. 缓冲区溢出

```c
// 监控缓冲区状态
 USB_DATA_STATS Stats;
 UsbDataGetStats(&gUsbDataContext, &Stats);
 SerialPrintf("Buffer overflows: %d\n", Stats.BufferOverflows);
```

## 最佳实践

### 1. 数据结构化
- 使用JSON格式便于解析
- 避免过长的单条记录
- 包含时间戳和上下文信息

### 2. 错误处理
- 始终检查返回状态
- 提供fallback到串口
- 记录详细的错误信息

### 3. 性能优化
- 使用适当的缓冲区大小
- 启用自动flush机制
- 定期轮转文件

### 4. 安全考虑
- 敏感数据加密
- 文件访问权限控制
- 定期清理旧文件

## 结论

USB数据传输模块为Plouton SMM框架提供了显著的性能提升和更好的可观测性。相比传统的串口和内存日志方案，USB传输能够：

1. **提升带宽** 100-1000倍
2. **改善跨平台兼容性**
3. **支持大容量数据存储**
4. **提供结构化数据格式**
5. **实现远程监控能力**

这使得SMM框架的数据输出更加实用和可维护，特别适合需要详细日志记录和数据分析的场景。