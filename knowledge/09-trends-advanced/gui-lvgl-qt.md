# 图形界面：LVGL 与 Qt

嵌入式 GUI 的难点不是画出一个界面，而是在有限 CPU、RAM、显存和总线带宽下保持流畅、稳定、可维护。

## 框架选择

| 框架 | 适合 |
|---|---|
| LVGL | MCU、RTOS、小屏幕、SPI/RGB 屏 |
| Qt Widgets/QML | 嵌入式 Linux、HMI、复杂交互 |
| Flutter Embedded | 资源较强的 Linux 设备 |
| DirectFB/SDL | 简单图形和定制场景 |
| 原生 framebuffer | 极简显示、启动动画 |

MCU 屏幕优先考虑 LVGL；Linux HMI 优先考虑 Qt 或轻量 framebuffer 方案。

## 显示链路

```text
UI 对象
  -> 渲染
  -> draw buffer
  -> flush callback
  -> SPI/RGB/DSI/framebuffer
  -> LCD panel
```

性能瓶颈可能在：

- UI 重绘面积过大。
- draw buffer 太小。
- SPI 频率太低。
- DMA 未启用。
- 颜色格式转换。
- 图片资源过大。
- 字体太多。
- Linux 上 GPU/DRM/Wayland 配置不当。

## LVGL 工程要点

关键配置：

- 颜色深度。
- draw buffer 大小。
- tick 周期。
- 输入设备。
- 字体和图片资源。
- 内存分配方式。
- 刷新策略。

典型任务模型：

```text
lv_tick_inc() 由定时器周期调用
ui_task 周期调用 lv_timer_handler()
flush_cb 使用 DMA/SPI 刷屏
input_cb 读取触摸或按键
```

注意：

- LVGL API 通常不要在多个任务中无保护调用。
- 大图和字体会显著占用 Flash/RAM。
- SPI 屏尽量使用局部刷新和 DMA。

## Qt 嵌入式要点

Qt 在嵌入式 Linux 上要关注：

- 交叉编译工具链。
- Qt 版本和模块裁剪。
- framebuffer、EGLFS、Wayland、X11 后端选择。
- 触摸输入和校准。
- 字体和国际化。
- 启动时间。
- GPU 驱动和图形加速。

常见启动参数：

```bash
QT_QPA_PLATFORM=eglfs ./hmi-app
QT_QPA_PLATFORM=linuxfb ./hmi-app
```

## UI 架构

建议分层：

```text
View       页面和控件
ViewModel 绑定数据、格式化显示
Service   设备状态、网络、配置
Driver    屏幕、触摸、背光
```

不要让 UI 直接读写硬件寄存器或网络连接。UI 应显示状态并发出用户意图，业务状态机负责执行。

## 资源预算

GUI 项目要提前估算：

- 分辨率。
- 色深。
- framebuffer 大小。
- draw buffer 大小。
- 图片总大小。
- 字体大小。
- 页面对象数量。
- 刷新帧率。

framebuffer 估算：

```text
width * height * bytes_per_pixel
```

例如 800x480 RGB565 约 750 KB，双缓冲约 1.5 MB。

## 调试方法

卡顿：

- 统计每次刷新耗时。
- 降低重绘区域。
- 检查 DMA 是否工作。
- 检查 SPI/RGB 时钟。
- 检查图片解码和内存分配。

花屏：

- 检查屏幕初始化序列。
- 检查颜色格式和字节序。
- 检查行列偏移。
- 检查 buffer 生命周期。
- 检查 DMA cache 一致性。

触摸异常：

- 坐标轴是否反转。
- 分辨率映射是否正确。
- 中断脚和复位脚是否正常。
- I2C 地址和采样频率是否正确。

## 练习

1. 用 LVGL 做一个传感器仪表盘，显示温度、湿度和网络状态。
2. 计算目标屏幕在 RGB565 下单缓冲和双缓冲所需 RAM。
3. 在 Linux framebuffer 上启动一个 Qt demo，并记录启动依赖。
