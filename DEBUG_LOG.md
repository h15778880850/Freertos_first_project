# First_project Debug Log

记录范围：从“OLED 实时显示当前温度”需求开始，到当前 W25Q64 使用 PB9 片选、K1 正常联动、系统可以正常执行为止。

## 1. OLED 实时显示温度

### 现象

最初项目已经拆成了 App / BSP / Service 的多任务结构，但 OLED 页面还没有明确显示 DS18B20 的实时温度。

### 修改

- 在 `AppSample` 中保留 DS18B20 温度字段 `temperature_centi`。
- `sensor_task` 周期读取 DS18B20，得到温度后通过 `sample_queue` 发送给 `ui_task`。
- `ui_task` 不再做采样，只从 `sample_queue` 接收采样结果并刷新 OLED。
- `BSP_Oled_ShowSample()` 增加温度显示逻辑，显示当前温度、阈值、报警状态和 K1 状态。

### 结果

OLED 可以进入运行页并显示实时温度。

## 2. OLED 显示 DS18B20 ERR 的排查

### 现象

OLED 显示：

```text
DS18B20 ERR
ADC:0000
THR:3000
NORMAL
```

### 判断

当时系统采样主链路仍然混有 ADC 逻辑：

- PA0 没有接任何传感器。
- ADC 读数为 `0000`，本质上只是空引脚或未接入模拟信号导致的无效采样。
- DS18B20 虽然已经有驱动文件，但一开始没有完全成为主采样链路。

### 修改

- 将采样主链路从 ADC 切换到 DS18B20。
- 从 CMake 主编译源中移除 ADC 主线依赖。
- OLED 页面从 ADC 显示改为温度显示。

### 结果

系统的主采样对象变成 DS18B20，不再把空 PA0 ADC 当成有效传感器。

## 3. 全部 SelfTest 显示 ERR 的排查

### 现象

修改后启动页曾出现 DS18B20、W25Q64、OLED 等全部 SelfTest 为 ERR。

### 判断

主要原因是自检时序和任务协作问题：

- OLED 启动页刷新时，部分外设任务还没有完成初始化。
- DS18B20 初始化和读取分散在不同阶段，启动页和运行页状态可能不同步。
- Flash 初始化失败时缺少足够日志，无法区分是芯片未响应还是保存失败。

### 修改

- `storage_task` 负责 W25Q64 初始化、读取 JEDEC ID、加载配置。
- `sensor_task` 负责 DS18B20 初始化和周期采样。
- `ui_task` 等待配置和传感器初始化状态后再显示启动页。
- OLED 启动页增加固件标记和 Flash JEDEC ID，方便确认烧录的是新固件。

### 结果

启动页和运行页状态逐步变得可诊断，不再只能看到笼统的 ERR。

## 4. DS18B20 启动 OK 但运行页 TEMP ERR

### 现象

启动页显示：

```text
DS18B20 OK
```

但运行页显示：

```text
TEMP ERR
DS18B20 READ
```

### 判断

这说明 DS18B20 的 presence 检测成功，但后续温度转换或 scratchpad 读取不稳定。

可能原因：

- 1-Wire 时序太慢或不稳定。
- 在位操作过程中频繁调用 `HAL_GPIO_Init()` 影响时序。
- CRC 校验过严，在实际连线或时序边缘情况下导致有效温度被丢弃。

### 修改

- DS18B20 驱动改成 STM32F1 寄存器级开漏控制，减少 HAL GPIO 重初始化带来的时序扰动。
- 使用 DWT 做微秒级延时。
- 温度读取增加多次重试。
- 对 scratchpad CRC 做容错：只要数据不是全 0x00 或全 0xFF，允许解析温度。

### 结果

DS18B20 可以稳定进入运行采样链路，OLED 能正常显示温度。

## 5. K1 没有参与四任务联动

### 现象

K1 按键按下后最初没有明显作用。

### 判断

硬件上 PB14 已经被配置成 EXTI，但任务联动不完整：

- 需要 `EXTI15_10_IRQHandler()` 调用 HAL EXTI 处理。
- 需要 `HAL_GPIO_EXTI_Callback()` 转发 PB14 事件。
- 需要 App 层知道 `sensor_task_handle`，否则 ISR 无法通知 `sensor_task`。

### 修改

- 在 `stm32f1xx_it.c` 中增加 `EXTI15_10_IRQHandler()`。
- 在 `HAL_GPIO_EXTI_Callback()` 中调用 `App_K1PressedFromIsr()`。
- 在 `sensor_task` 中加入 K1 触发后的强制采样和配置保存请求。
- 增加 PB14 轮询兜底，避免 EXTI 失效时 K1 完全不可用。
- 在 `freertos.c` 创建完 `sensor_task` 后调用：

```c
App_SetSensorTaskHandle(sensor_task_handle);
```

### 结果

K1 可以触发：

```text
K1 pressed: force sample and request config save
```

说明 K1 已经参与 sensor_task、storage_task、log_task 的任务联动。

## 6. K1 触发后 config save failed

### 现象

按下 K1 后串口显示：

```text
K1 pressed: force sample and request config save
config save failed
```

### 判断

这说明：

- K1 输入链路正常。
- `sensor_task` 可以请求保存。
- `save_timer` 和 `storage_queue` 正常工作。
- `storage_task` 收到了保存请求。
- 问题集中在 W25Q64 写入流程。

### 修改

在 W25Q64 驱动中增加更细的诊断：

- 写使能后读取状态寄存器，确认 `WEL` 位是否置 1。
- Flash busy 等待超时从 1s 增加到 5s。
- 增加最后错误码：

```text
0 = none
1 = param
2 = flash not ready
3 = write enable failed
4 = erase failed
5 = page program failed
6 = wait ready timeout
```

- 保存失败时串口打印：

```text
config save failed id=0x?????? sr=0x?? err=? ready=?
```

### 结果

Flash 保存失败从黑盒错误变成可定位错误。

## 7. W25Q64 片选引脚错误

### 现象

W25Q64 仍然保存异常，同时发现 BSP 驱动内部初始化的是 PA4。

### 判断

项目硬件实际定义中，W25Q64 的 CS 是 PB9：

- `main.c` 里 PB9 已经配置为输出脚。
- 但 `bsp_w25q64.c` 中仍写死：

```c
#define W25Q64_CS_PORT GPIOA
#define W25Q64_CS_PIN  GPIO_PIN_4
```

这会导致驱动操作的是 PA4，而真实 Flash 片选 PB9 没有被正确控制。

### 修改

将 W25Q64 CS 改为 PB9：

```c
#define W25Q64_CS_PORT GPIOB
#define W25Q64_CS_PIN  GPIO_PIN_9
```

同时 `BSP_W25Q64_Init()` 中打开 GPIOB 时钟。

### 结果

W25Q64 驱动的片选引脚与实际硬件一致。

## 8. PB9 上电默认电平错误

### 现象

`main.c` 中 PB9 默认输出电平原本是低电平：

```c
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
```

### 判断

W25Q64 的 CS 是低有效。上电阶段默认拉低会导致 Flash 被提前选中，可能干扰 SPI 初始化或造成异常命令序列。

### 修改

PB9 默认输出改为高电平：

```c
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
```

### 结果

上电时 W25Q64 不会被默认选中，SPI 初始化后再由 BSP 驱动控制 CS。

## 9. 当前最终状态

当前系统已经具备以下链路：

- `sensor_task`
  - 周期读取 DS18B20 温度。
  - K1 触发时强制采样。
  - 调用 `MonitorService_UpdateAlarm()` 更新报警状态。
  - 将采样结果发送到 `sample_queue`。

- `ui_task`
  - 接收 `sample_queue`。
  - OLED 实时显示温度、阈值、NORMAL/ALARM、K1 状态。

- `storage_task`
  - 初始化 W25Q64。
  - 上电加载配置。
  - 接收 `storage_queue` 保存请求。

- `log_task`
  - 串口输出启动、自检、采样、K1、保存结果日志。

- K1
  - PB14 EXTI + 轮询兜底。
  - 按下后触发强制采样和配置保存请求。

- W25Q64
  - CS 使用 PB9。
  - PB9 上电默认高电平。
  - 保存失败时具备 JEDEC ID、状态寄存器、错误码诊断。

## 10. 关键经验

这次问题不是单点 bug，而是多个小不一致叠加：

- 软件主链路一开始还残留 ADC，但硬件没有接 PA0 传感器。
- DS18B20 presence OK 不等于温度读取 OK。
- 自检页显示时机必须等待相关任务完成初始化。
- K1 EXTI 生效不等于已经能通知 RTOS 任务。
- W25Q64 能被配置成输出脚不等于 BSP 驱动真的用了同一个 CS 引脚。
- Flash CS 上电默认低电平会带来隐蔽问题，应默认高电平。

最终修正方向是：让硬件定义、BSP 驱动、RTOS 任务通信、OLED 显示和串口日志全部对齐。

## 11. K1 长按反复触发问题

### 现象

系统已经可以正常运行后，发现 K1 一直按住时会一直触发任务逻辑，串口持续打印 K1 触发日志，并不断请求强制采样和配置保存。

### 判断

K1 是上拉输入：

- 按下时 PB14 为低电平。
- 松开时 PB14 回到高电平。

原实现中有两个问题：

- PB14 配置为 `GPIO_MODE_IT_RISING_FALLING`，按下和松开两个边沿都会进入 EXTI。
- `sensor_task` 中的轮询兜底逻辑是检测低电平，只要一直按住，就会被周期性识别为新的 K1 事件。

这导致实际行为变成“按住触发”，而需求是“松开后触发一次”。

### 修改

将 PB14 中断改为只响应上升沿：

```c
GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
```

在 `App_K1PressedFromIsr()` 中增加电平确认：

```c
if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) != GPIO_PIN_SET)
{
  return;
}
```

将 `app_wait_period_or_k1()` 的轮询兜底逻辑改成释放触发：

- 先检测到 PB14 低电平，记录“曾经按下”。
- 再检测到 PB14 回到高电平。
- 高电平稳定超过消抖时间后，才返回一次 K1 事件。

同时删除采样前后通过“当前是否低电平”强行设置 `k1_triggered` 的逻辑，避免按住期间把每次采样都标记成 K1 触发。

串口日志也从：

```text
K1 pressed: force sample and request config save
```

改为：

```text
K1 released: force sample and request config save
```

### 结果

K1 当前行为变为：

- 按住 K1：不触发任务逻辑。
- 松开 K1：触发一次强制采样和配置保存请求。
- 长按不会反复触发。

编译验证通过：

```text
RAM:   17008 B / 20 KB  83.05%
FLASH: 39496 B / 64 KB  60.27%
```

## 12. OLED UI 重构 + EC11 旋转编码器集成

### 12.1 需求说明

将单调的温度显示界面改造为多模块菜单系统：

- 启动时显示 "Loading..." 加载页面。
- 加载完成后进入模块选择页面，通过 EC11 旋转编码器导航。
- 目前有温度测量模块和 Flash 模块可选，后续可扩展。
- 旋转 EC11 选择模块，按下 KEY 进入/退出模块。

### 12.2 EC11 硬件接线与原理

| 编码器信号 | STM32 引脚 | 配置模式 | 作用 |
|---|---|---|---|
| S1 (A 相) | PB12 | EXTI 上升沿中断 | 旋转触发信号 |
| S2 (B 相) | PB0 | GPIO 输入 | 旋转方向判断 |
| KEY (按键) | PB1 | GPIO 输入 | 确认/返回 |

**EC11 正交解码原理**:

EC11 旋转编码器输出两路正交方波 (S1/S2)。当旋转时:
- **顺时针 (CW)**: S1 上升沿时 S2 为低电平
- **逆时针 (CCW)**: S1 上升沿时 S2 为高电平

PB12 配置为上升沿中断触发。进入 ISR 后读取 PB0 (S2) 电平即可判断方向:

```
S1 (PB12) ──┐    ┌──    (上升沿 → ISR 触发)
             └────┘
S2 (PB0)  ────┐  ┌───   (低电平 → CW)
              └──┘
S2 (PB0)  ┌───┐  ┌───   (高电平 → CCW)
          └───┘  └
```

### 12.3 OLED UI 状态机设计

```
  ┌──────────┐   初始化完成   ┌──────────┐
  │  LOADING  │──────────────→│   MENU    │
  │ "Loading" │               │ 选择模块  │
  └──────────┘               └─────┬─────┘
                              KEY  │  KEY
                    ┌──────────────┼──────────────┐
                    ↓              │              ↓
              ┌───────────┐        │       ┌───────────┐
              │MODULE_TEMP│        │       │MODULE_FLASH│
              │ 实时温度  │        │       │ Flash存储  │
              └─────┬─────┘        │       └─────┬─────┘
                    └──────────────┼──────────────┘
                              KEY (返回)
```

状态转换逻辑:

| 当前状态 | 事件 | 下一状态 |
|---|---|---|
| LOADING | 所有外设初始化完成 + 延时 300ms | MENU |
| MENU | KEY 按下且选中 Temperature | MODULE_TEMP |
| MENU | KEY 按下且选中 Flash Storage | MODULE_FLASH |
| MENU | EC11 顺时针旋转 | 光标下移 |
| MENU | EC11 逆时针旋转 | 光标上移 |
| MODULE_TEMP | KEY 按下 | MENU |
| MODULE_FLASH | KEY 按下 | MENU |

### 12.4 ISR 与任务通信机制

EC11 S1 旋转通过 **ISR → Task Notification → 轮询** 三级机制处理:

```
PB12 上升沿
    │
    ▼
EXTI15_10_IRQHandler()
    │
    ▼
HAL_GPIO_EXTI_Callback(PIN_12)
    │
    ▼
BSP_EC11_HandleS1Isr()
    ├── 读取 PB0 (S2) 判断方向
    ├── 更新 volatile int32_t s_ec11_rotation (±1)
    └── osThreadFlagsSet(ui_task, EC11_FLAG_ROTATION)  → 唤醒 ui_task
                                                            │
ui_task 主循环                                              │
    ├── osThreadFlagsWait(EC11_FLAG_ROTATION, 100ms) ◄─────┘
    ├── BSP_EC11_GetRotation() → 原子读取+清零累加器
    ├── 根据旋转值更新菜单光标
    └── BSP_EC11_IsKeyPressed() → 轮询 PB1 + 50ms 消抖
```

关键设计原则:
- **ISR 极简**: 只做硬件读取和原子操作，不调用阻塞 API。
- **Task Notification 唤醒**: ISR 通过 `osThreadFlagsSet()` 唤醒 ui_task，比队列更轻量。
- **KEY 任务轮询**: PB1 无 EXTI，由 ui_task 每 100ms 轮询一次，50ms 软件消抖。
- **OLED 在任务上下文刷新**: I2C 阻塞操作在任务中执行，不再 ISR 内。

### 12.5 构建系统与 CMake 架构

#### 目录结构

```
First_project/
├── cmake/
│   └── stm32cubemx/
│       └── CMakeLists.txt        ← 生成代码构建配置
├── CMakeLists.txt                ← 用户代码构建配置
├── CMakePresets.json             ← 预设 (Debug/Release)
├── Core/                         ← CubeMX 生成的文件
│   ├── Inc/                      ← 头文件 (main.h, FreeRTOSConfig.h, ...)
│   └── Src/                      ← 源文件 (main.c, freertos.c, ...)
├── App/                          ← 应用层 (app_main.c/h)
├── BSP/                          ← 板级驱动 (bsp_*.c)
├── Service/                      ← 服务层 (config_service.c, monitor_service.c)
├── Drivers/                      ← HAL 库 + CMSIS
└── Middlewares/                   ← FreeRTOS
```

#### CMake 层次结构

```
顶层 CMakeLists.txt
    │
    ├── target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    │       App/app_main.c        ← 用户应用代码
    │       BSP/bsp_*.c           ← 用户 BSP 驱动
    │       Service/*.c           ← 用户服务层
    │   )
    │
    └── add_subdirectory(cmake/stm32cubemx)
            │
            └── cmake/stm32cubemx/CMakeLists.txt
                    │
                    ├── add_library(stm32cubemx INTERFACE)
                    │       └── 提供公共头文件路径 + 宏定义
                    │
                    ├── add_library(STM32_Drivers OBJECT)
                    │       └── HAL 驱动源码 (stm32f1xx_hal_*.c)
                    │
                    ├── add_library(FreeRTOS OBJECT)
                    │       └── FreeRTOS 内核源码
                    │
                    └── target_sources(${CMAKE_PROJECT_NAME} PRIVATE
                            Core/Src/main.c              ← CubeMX 生成的 main
                            Core/Src/freertos.c          ← CubeMX 生成的 RTOS 配置
                            Core/Src/stm32f1xx_it.c      ← 中断服务程序
                            Core/Src/stm32f1xx_hal_msp.c ← HAL MSP 配置
                            ...
                        )
```

#### 链接器符号解析与重定义冲突

本项目的关键架构特点: **用户代码 (app_main.c) 与 CubeMX 生成代码 (main.c) 同时定义同名 task 函数**。

**原因**:
- `main.c` (CubeMX 生成) 自动包含 4 个 task 的 stub 实现 (仅 `osDelay(1)` 空循环)。
- `app_main.c` (用户代码) 提供了 4 个 task 的完整实现。
- `freertos.c` 同时定义了 4 个 task 的 `osThreadAttr_t` 属性结构体。

**冲突表现**:
```
multiple definition of `sensor_task'
multiple definition of `ui_task'
multiple definition of `storage_task'
multiple definition of `log_task'
multiple definition of `sensor_task_attributes'
... (共 8 个重定义错误)
```

**解决方案**:

1. **从 `main.c` 中删除 stub task 函数体** (位于 `USER CODE BEGIN 5` / `USER CODE END 5` 用户编辑区):
   - 删除 `sensor_task()`, `ui_task()`, `storage_task()`, `log_task()` 四个空壳函数。

2. **从 `main.c` 中删除 task 属性定义** (auto-generated 区):
   - 删除 `sensor_task_attributes`, `ui_task_attributes` 等 4 个属性结构体。
   - 删除 `sensor_taskHandle`, `ui_taskHandle` 等 4 个 task handle 变量。

3. **从 `main()` 中删除重复的 `osThreadNew()` 调用** (auto-generated 区):
   - 任务创建统一由 `MX_FREERTOS_Init()` (freertos.c) 负责。

**链接行为说明**:
- CubeMX 的 Drivers 和 FreeRTOS 编译为 OBJECT library (`.o` 集合)。
- 用户代码和 CubeMX 应用代码直接编译到可执行文件中。
- 当同一个符号在多个 `.o` 中出现时，链接器报 `multiple definition` 错误。
- 删除 `main.c` 中的重复定义后，只保留 `app_main.c` + `freertos.c` 的唯一定义。

### 12.6 编译命令与结果

#### 配置

```bash
# 首次配置 (使用 Debug 预设)
cmake --preset Debug
```

预设定义来自 `CMakePresets.json`:
```json
{
  "configurePresets": [
    {
      "name": "Debug",
      "binaryDir": "${sourceDir}/build/Debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_TOOLCHAIN_FILE": ".../stm32-toolchain.cmake"
      }
    }
  ]
}
```

#### 编译

```bash
cmake --build build/Debug
```

使用 Ninja 构建系统 + `arm-none-eabi-gcc` 13.3.1 工具链。

编译选项:
- `-mcpu=cortex-m3`: Cortex-M3 指令集
- `-O0 -g3`: Debug 优化级别 (无优化, 完整调试信息)
- `-Wall`: 启用所有警告
- `-fdata-sections -ffunction-sections`: 按段编译
- `-Wl,--gc-sections`: 链接时移除未引用段

#### 最终构建结果

```
RAM:   17032 B / 20 KB (83.16%)
FLASH: 41024 B / 64 KB (62.60%)
0 warnings, 0 errors
```

#### 内存分析

| 区域 | 已用 | 总量 | 占比 | 说明 |
|---|---|---|---|---|
| RAM | 17032 B | 20 KB | 83.16% | 含 4 个任务栈 + FreeRTOS 堆 (heap_4, 10KB) + 系统变量 |
| FLASH | 41024 B | 64 KB | 62.60% | 含 HAL 库 + FreeRTOS + 用户代码 + BSP 驱动 |

**RAM 构成估算**:

| 组件 | 大小 | 说明 |
|---|---|---|
| FreeRTOS heap_4 | 10240 B | `configTOTAL_HEAP_SIZE` |
| sensor_task 栈 | 1280 B | 320 × 4 |
| ui_task 栈 | 1536 B | 384 × 4 |
| storage_task 栈 | 1280 B | 320 × 4 |
| log_task 栈 | 1536 B | 384 × 4 |
| 系统/内核/其他 | ~1160 B | 系统变量、IDLE 任务等 |
| **合计** | **~17032 B** | |

**FLASH 构成估算**:

| 组件 | 大小 | 说明 |
|---|---|---|
| HAL 库 (I2C/SPI/GPIO/TIM 等) | ~18 KB | 按需链接 (gc-sections) |
| FreeRTOS 内核 (tasks/queue/timers 等) | ~10 KB | CMSIS-RTOS v2 封装层 |
| 用户应用层 (app_main.c) | ~5 KB | 任务逻辑 + UI 状态机 |
| BSP 驱动 (ds18b20/w25q64/oled/ec11 等) | ~6 KB | 硬件驱动层 |
| 启动代码 + 系统初始化 | ~2 KB | startup + system + HAL_Init |
| **合计** | **~41 KB** | |

### 12.7 文件变更清单

| 操作 | 文件 | 说明 |
|---|---|---|
| **新建** | `BSP/bsp_ec11.c` | EC11 旋转编码器 BSP 驱动 (正交解码 + 按键消抖) |
| **新建** | `Core/Inc/bsp_ec11.h` | EC11 驱动头文件 |
| **修改** | `App/app_main.c` | ui_task 重写为状态机; 新增 UiState 枚举和模块数组; 新增 app_ui_init() + App_GetUiTaskHandle() |
| **修改** | `App/app_main.h` | 新增 App_GetUiTaskHandle() 声明 |
| **修改** | `BSP/bsp_oled.c` | 新增 BSP_Oled_ShowLoading / ShowMenu / ShowFlashInfo |
| **修改** | `Core/Inc/bsp_oled.h` | 新增 3 个显示函数声明 |
| **修改** | `Core/Src/stm32f1xx_it.c` | PB12 中断回调接入 BSP_EC11_HandleS1Isr() |
| **修改** | `Core/Src/main.c` | 移除重复的 task stub/属性/创建调用 |
| **修改** | `CMakeLists.txt` | 添加 BSP/bsp_ec11.c 到编译源列表 |

### 12.8 模块可扩展设计

模块列表通过编译期数组定义，添加新模块只需两步:

```c
// 1. 在枚举中添加
typedef enum {
  UI_STATE_LOADING,
  UI_STATE_MENU,
  UI_STATE_MODULE_TEMP,
  UI_STATE_MODULE_FLASH,
  UI_STATE_MODULE_NEW,       // ← 新增
} UiState;

// 2. 在数组和 case 中添加
static const char *const s_module_names[MODULE_COUNT] = {
  "Temperature",
  "Flash Storage",
  "New Module",              // ← 新增
};

// 在 ui_task switch 中添加对应状态的处理
case UI_STATE_MODULE_NEW:
  // 新模块的显示和交互逻辑
  break;
```

不需要修改 ISR、EC11 驱动或 RTOS 配置。
