# 图形界面：LVGL 与 Qt

嵌入式 GUI 的难点不是画出一个界面，而是在有限 CPU、RAM、显存和总线带宽下保持流畅、稳定、可维护。

## 本章任务与边界

完成本章后，你应能从分辨率、颜色格式、刷新区域和帧率估算显存/带宽，设计 UI 线程与业务状态边界，测量输入到显示的尾延迟，并在资源不足、显示/触摸异常和业务重启时恢复一致状态。

本章提供 LVGL/Qt 选型与工程模型，具体 API、后端和 GPU 路径依版本与平台而异。平均 FPS 和演示动画流畅不能证明最坏帧时间、输入可靠性和长稳无泄漏。

完成证据：资源预算、帧时间分布、满屏/局部刷新对比、线程违规或慢 flush 故障，以及 UI 状态与设备真实状态不一致的恢复测试。

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

## 工程洞察

嵌入式 GUI 不只是画界面，而是在有限 CPU、内存、显示带宽和输入延迟下提供可理解、可响应、可恢复的人机交互。GUI 系统会把底层状态暴露给用户，也会引入新的实时性和资源问题。

设计 GUI 时要关注：

- 显示链路：framebuffer、flush、DMA2D/GPU、刷新率、撕裂和脏矩形。
- 资源预算：显存、图片资源、字体、动画、对象树和缓存。
- 响应路径：触摸/按键事件、业务状态更新、线程边界和主循环阻塞。
- 状态一致性：设备离线、传感器错误、升级中、故障保护时界面如何表达。
- 降级策略：内存不足、渲染卡顿、资源加载失败和触摸异常如何处理。

好的嵌入式界面不是视觉元素堆叠，而是让用户准确理解设备状态和可执行动作。尤其在工业、医疗、能源等场景中，界面要避免把“命令已发送”误表达成“动作已完成”，否则会造成操作风险。
