# Wi-Fi 网络时钟：屏幕配网版

## 本次改了什么

上一版把 Wi-Fi 名称和密码写在 main/wifi_config.h 中，换网络需要重新编译。本次改成屏幕输入、连接成功后保存、重启自动连接。

| 文件 | 具体改动 |
| --- | --- |
| main/main.c | 主界面右上角增加 Wi-Fi 按钮；计时回调刷新配网页状态，并提示首次配网或连接失败。保留已修改的标题、加 10 和 Reset 按钮。 |
| main/wifi_setup.c / wifi_setup.h | 新增配网页：SSID 输入框、隐藏密码输入框、屏幕键盘、Connect 和 Back 按钮、连接及保存状态提示。 |
| main/wifi_clock.c / wifi_clock.h | 删除对 wifi_config.h 的依赖；通过队列接收屏幕输入。独立任务管理停止旧连接、切换网络、重试、校时与 NVS 读写；界面只读取不含密码的状态快照。 |
| main/wifi_credentials.c / wifi_credentials.h | 新增输入校验：SSID 按 UTF-8 字节计数，不超过 32 字节；密码为 8–63 字节，或 64 位十六进制 PSK；空密码表示开放网络。 |
| main/CMakeLists.txt | 加入配网页及校验源文件，并显式声明网络、定时器与 NVS 组件依赖。 |
| tests/wifi_credentials_test.c | 增加名称长度、UTF-8 字节数、空密码、短密码、64 位 PSK 等边界测试。 |
| README.md | 更新为屏幕配网的入口和操作说明。 |

旧的 main/wifi_config.h 和 wifi_config.example.h 留在目录中，但新程序不再包含、读取它们。已经在旧头文件中填写的内容不会自动迁移到设备，请在新界面重新配置一次。后续换 Wi-Fi 无须改代码或重新编译。

## 现在怎么操作

1. 在 VS Code 打开原来的 s31_display 工程，选择已有的 ESP-IDF master 环境。
2. 在旧串口监视器按 Ctrl + ] 退出，释放 COM11。
3. 在本工程 ESP-IDF 终端运行：

~~~powershell
idf.py -p COM11 build flash monitor
~~~

4. 在开发板主界面点击右上角 **Wi-Fi**。
5. 点击 SSID 输入框，使用屏幕键盘填写 2.4 GHz Wi-Fi 名称。
6. 点击 Password 输入框并填写密码，再点击 **Connect**。
7. 看到 **Connected and saved** 后，点击 **Back** 返回时钟。网络校时完成后显示北京时间，每秒刷新。
8. 按板上的 RESET 重启，检查是否自动连接刚才保存的网络。

键盘上的确认键在填写 SSID 时切到密码框，在密码框时提交连接。Back 返回主界面；已经提交的连接请求仍在后台继续。再次打开配网页会显示当前目标网络名称，密码框保持空白；重新点击 Connect 需要重新输入该网络密码。

## 保存和失败处理

- **首次上电**：没有已保存网络时，主界面提示点击 Wi-Fi 进行配置。
- **保存时机**：新网络认证成功并取得 IP 地址后，才把名称和密码保存到 NVS。按 Connect 时不立即覆盖旧记录。
- **输错密码或找不到热点**：新网络最多重试 3 次，或约 30 秒超时；界面提示修改后重试。上一次成功保存的网络记录不被替换。
- **已保存网络暂时断开**：后台约每 5 秒发起重连；界面与计数按钮继续运行。
- **切换网络**：先停止旧连接，等待停止事件后再设置新网络，避免旧的断线/IP 事件误触发保存。
- **存储失败**：显示 Connected, but saving failed，表示本次已连上但未成功保存；可重新输入并点击 Connect 重试。
- **重新启动**：从 NVS 读取网络名称和密码，自动联网并开始校时。
- **校时成功后断网**：系统时钟继续走；完全断电后重新校时。
- 普通烧录保留现有 NVS；整片 erase-flash 或清空 NVS 会删除已保存的网络。

NVS 使用默认 nvs 分区，命名空间为 s31_wifi，键名为 station，保存带版本号的一个 blob。程序不会在初始化失败时自动整片擦除，也不会把密码写进日志或返回给界面状态查询。此学习工程未启用 NVS 加密。

应用分区仍为上一课设置的 3 MB；NVS 与 phy_init 的地址不变。

## 这一课理解什么

1. 屏幕输入框负责获取名称、密码；输入校验负责检查格式。
2. 队列把连接请求交给网络任务，界面回调不等待连接完成。
3. 网络事件推动“连接中 → 已连接 → 已保存”的状态变化。
4. NVS 将配置保留到下次启动。
5. time() 读取系统时间，localtime_r() 转换成本地日期，strftime() 格式化，LVGL 定时器每秒刷新标签。

网络校时使用 SNTP，服务器在 wifi_clock.c 中配置为 pool.ntp.org，时区为北京时间 UTC+8。如果已连接但持续等待校时，检查互联网连接和 NTP（UDP 123）是否可用。


## 从第一课到现在：完整改动顺序

这部分按我们实际完成练习的顺序整理。复习时先看一行，再打开对应源码确认效果，不必一次读完网络任务的全部代码。

| 阶段 | 原来怎样 | 改在哪里、改成什么 | 学到什么 |
| --- | --- | --- | --- |
| 0. 建立完整工程 | 单独打开桌面 BSP 包，扩展无法识别完整应用 | 新建根 CMakeLists.txt、main 目录和配置文件；主程序通过 BSP 使用屏幕 | 区分“板级组件”和“完整应用工程” |
| 1. 修改标题 | 显示 Hello ESP32-S31! | main.c 的 create_screen() 中改为 My first screen | 修改字符串、保存、重新编译与烧录 |
| 2. 改变步长 | 点击按钮加 1 | on_button_clicked() 改成 tap_count += 10，同时按钮文字改为 Tap me +10 | 变量是实际数据，按钮文字只是提示 |
| 3. 增加清零 | 只有加数按钮 | 新增 on_reset_clicked() 与 Reset 按钮，并把原按钮向左移 | 两个事件回调操作同一份状态 |
| 4. 自动更新 | 只有点击时改变界面 | 新增 LVGL 定时器，最初显示 lv_tick_get() / 1000 得到的运行秒数 | 定时回调与时间单位 |
| 5. 联网时钟 | 显示的是运行时长 | 加入 Wi-Fi、SNTP 和时区设置；on_timer() 改用系统时间 | 设备运行时长和真实日期时间的区别 |
| 6. 屏幕配网 | 名称、密码写进头文件，换网需要重新编译 | 新增输入框、屏幕键盘和网络任务；连接成功后保存 NVS | 运行时配置、任务通信和持久存储 |

### 工程文件一览：除了 C 代码，还改了什么

~~~text
s31_display/
├─ CMakeLists.txt                 完整工程入口，目标芯片 esp32s31
├─ sdkconfig.defaults             初始配置模板
├─ sdkconfig                      本机当前生成的配置
├─ partitions.csv                 分区表
├─ dependencies.lock              构建时锁定的实际依赖版本
├─ .gitignore                     忽略构建目录和旧私有配置文件
├─ .vscode/settings.json          本机 IDF、串口及目标配置
├─ main/
│  ├─ CMakeLists.txt              本应用需要编译的源文件
│  ├─ idf_component.yml           BSP、LVGL 和 IDF 版本要求
│  ├─ main.c                     主界面、按钮与时钟显示
│  ├─ wifi_setup.c / .h           屏幕配网页
│  ├─ wifi_clock.c / .h           网络后台任务、校时和保存
│  ├─ wifi_credentials.c / .h     输入校验和配置数据结构
│  ├─ wifi_config.h               上一版的私有头文件，已停用
│  └─ wifi_config.example.h       上一版的示例头文件，已停用
├─ tests/wifi_credentials_test.c  本机输入校验测试
├─ README.md                      工程入口说明
└─ WIFI_CLOCK.md                  本文件：改动与学习笔记
~~~

工程外的 s31_display.code-workspace 是 VS Code 工作区入口，它指向 s31_display 文件夹。它不是固件的一部分。

| 文件或设置 | 当前作用 | 为什么需要 |
| --- | --- | --- |
| 根 CMakeLists.txt | 设置目标 esp32s31，引入 IDF 构建系统，启用最小依赖构建，声明工程名 s31_display | 让 VS Code 和 IDF 识别完整应用，不再把 BSP 组件当成应用 |
| main/idf_component.yml | 要求 IDF >= 6.1，使用 BSP 1.0.1 和 LVGL 9.3.0 | 组件管理器可以自动获取屏幕驱动与图形库 |
| dependencies.lock | 记录本次实际解析到的组件版本和哈希 | 方便复现当前构建环境 |
| sdkconfig.defaults | 固定 S31，启用 PSRAM、200 MHz PSRAM 时钟、双帧缓冲、防撕裂、1000 Hz FreeRTOS tick 和所需字体 | 屏幕帧缓冲需要外部内存，字体与显示功能需要在配置中启用 |
| .vscode/settings.json | 选择本机 master 环境、COM11、UART 下载和 S31 调试配置 | 减少每次打开工程时的重复选择 |
| .gitignore | 忽略 build、managed_components、生成的 sdkconfig 和旧 wifi_config.h | 将手写源码与本机缓存、旧私有文件区分开 |
| main/CMakeLists.txt | 编译 main.c、wifi_clock.c、wifi_setup.c、wifi_credentials.c，并声明 Wi-Fi、事件、网络、定时器、NVS 依赖 | 只创建 .c 文件还不够，必须告诉构建系统把它编译进去 |

sdkconfig 是本机生成的实际配置，sdkconfig.defaults 是新建配置时使用的模板。已有 sdkconfig 时，改 defaults 通常不会覆盖已有选项；日常可通过 idf.py menuconfig 修改实际配置。

### 分区表为什么从默认应用分区改为 3 MB

原先最小屏幕程序可以放进默认的 1 MB 应用分区。加入 Wi-Fi 后，本机生成的应用固件已经超过 1 MB，因此增加了 partitions.csv，并把配置切换到自定义分区表。

| 名称 | 起始地址 | 大小 | 保存什么 |
| --- | --- | --- | --- |
| nvs | 0x9000 | 0x6000 | Wi-Fi 配置等持久数据 |
| phy_init | 0xf000 | 0x1000 | 射频初始化数据 |
| factory | 0x10000 | 3 MB | 当前应用固件 |

这次扩大的是应用区域，NVS 起始地址和大小沿用原工程。应用程序、NVS 配置和 RAM 变量是三类不同内容；重新刷应用代码不等于自动删除 NVS。

## 建议按这个顺序读源码

1. main/main.c：先理解屏幕上每个对象从哪里创建。
2. main/wifi_setup.c：再看一次 Connect 点击怎样读出输入内容。
3. main/wifi_credentials.c：理解输入如何被判断为有效或无效。
4. main/wifi_clock.h：先认识网络模块公开给界面的接口。
5. main/wifi_clock.c：沿一次连接流程读函数，不必从第一行逐字读到底。
6. tests/wifi_credentials_test.c：通过输入与预期结果检查自己的理解。

### main.c：入口、按钮和定时器

app_main() 的调用顺序是：

~~~text
bsp_display_start()
    ↓ 初始化显示、触摸和 LVGL
bsp_display_lock(0)
    ↓ 在启动任务中安全地创建界面
create_screen()
bsp_display_unlock()
    ↓ 创建结束
wifi_clock_start()
    ↓ 初始化网络模块并启动后台任务
app_main() 返回，LVGL 和网络任务继续工作
~~~

app_main() 返回后屏幕仍然运行，因为 LVGL 和网络逻辑都由各自任务持续处理，不需要自己写一个 while(1) 把 app_main() 挡住。

主要对象和变量：

| 名称 | 类型或作用 | 理解方式 |
| --- | --- | --- |
| tap_count | uint32_t，当前计数 | 程序真正保存的数值 |
| count_label | lv_obj_t 指针，计数标签 | 用来找到并更新屏幕上的文字对象 |
| title | 标题标签 | 改文字不改变计数 |
| subtitle | 时间或网络提示标签 | 交给定时器每秒更新 |
| button / reset_button | 加数和清零按钮 | 创建按钮之后，还要绑定点击事件 |
| wifi_button | 右上角配网入口 | 点击时调用 wifi_setup_open() |
| footer | 底部操作提示 | 引导用户点击 Wi-Fi |

加数回调的关键两步是：

~~~c
tap_count += 10;  // 先改变数据
lv_label_set_text_fmt(count_label, "Count: %" PRIu32, tap_count);
~~~

清零回调同样要改变数据并刷新界面：

~~~c
tap_count = 0;
lv_label_set_text(count_label, "Count: 0");
~~~

如果只把标签改成 Count: 0，而不把 tap_count 置零，下次加数会继续使用旧变量值。

绑定事件这句：

~~~c
lv_obj_add_event_cb(button, on_button_clicked, LV_EVENT_CLICKED, NULL);
~~~

四个参数依次是：哪个对象、执行哪个函数、响应哪类事件、附带什么自定义数据。最后的 NULL 表示本例没有附加数据。

### on_timer() 为什么换了写法

早先使用 lv_tick_get() / 1000，得到的是毫秒计数转换出的运行秒数。现在需要真实时间，使用：

~~~c
time_t now = time(NULL);
struct tm current_time;
localtime_r(&now, &current_time);

char text[32];
strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &current_time);
lv_label_set_text(label, text);
~~~

| 函数或字段 | 在这里做什么 |
| --- | --- |
| time(NULL) | 读取系统当前时间；这一步本身不会访问互联网 |
| struct tm | 把日期拆成年、月、日、时、分、秒等字段 |
| localtime_r() | 按已设置的时区转换时间，并写入调用者提供的结构体 |
| strftime() | 将结构体中的日期时间格式化成字符串 |
| %Y-%m-%d %H:%M:%S | 显示格式，例如“年-月-日 时:分:秒” |
| tm_year | 从 1900 开始计算的年数，因此比较年份时需要减 1900 |
| lv_timer_get_user_data() | 取回创建定时器时传入的 subtitle 标签 |
| wifi_setup_refresh() | 每秒同时刷新配网页的连接状态 |

当前代码把年份小于 2024 的时间视为尚不可用，先显示配网或校时提示，避免把 1970 年当作当前日期显示。

时区在网络模块启动时设置：

~~~c
setenv("TZ", "CST-8", 1);
tzset();
~~~

POSIX 时区字符串里的符号方向与直觉相反：这里 CST-8 表示 UTC+8。后续 localtime_r() 已经完成时区转换，不需要自己再加 8 小时。

### 为什么启动时加锁，点击回调中没有再次加锁

app_main() 和 LVGL 是不同任务。启动时创建对象，需要用 BSP 的显示锁保护。按钮、键盘和 LVGL 定时器的回调原本就在 LVGL 任务中执行，本例在这些回调内直接更新对象。

网络任务不直接调用 lv_label_set_text()，它只更新网络状态快照，再由 LVGL 定时器读取显示。这是本工程避免两个任务同时修改界面的方式。

## wifi_setup.c：屏幕配网页怎样工作

| 函数 | 做了什么 | 需要关注的地方 |
| --- | --- | --- |
| wifi_setup_open() | 在当前屏幕上创建全屏设置层、输入框、按钮和键盘 | 若设置层已打开，重复点击不会再创建第二份 |
| make_button() | 创建按钮及其居中的文字 | 把重复的创建过程放进函数 |
| on_input() | 让屏幕键盘切换到当前点击或获得焦点的输入框 | 同一个键盘可以为不同输入框输入文字 |
| connect_clicked() | 读取输入、校验、提交连接请求 | 只提交请求，不等到连接完成才返回 |
| keyboard_event() | 处理键盘确认和退出事件 | SSID 确认后转向密码，密码确认后提交 |
| wifi_setup_refresh() | 读取网络状态并更新提示 | 在 LVGL 任务中刷新，不由后台任务直接改标签 |
| close_setup() | 解绑键盘、清空密码输入框并延后删除设置层 | 清掉全局对象指针，避免定时器继续访问已删除对象 |

重要的 LVGL 用法：

~~~c
lv_textarea_set_one_line(s_ssid, true);
lv_textarea_set_password_mode(s_password, true);
lv_keyboard_set_textarea(s_keyboard, s_ssid);
~~~

第一句将名称输入框设为单行，第二句让密码以隐藏形式显示，第三句决定键盘当前向哪个输入框写入文字。

输入框设置了字符数上限，但最终校验仍然按字节数进行。因为一个中文汉字通常占多个 UTF-8 字节，“32 个字符”和“32 个字节”不一定相同。

s_local_error 的作用是保留本地输入错误提示。否则定时器每秒刷新一次，就会立即把“密码太短”等提示覆盖掉。用户继续编辑或重新提交后，才恢复正常状态显示。

close_setup() 使用 lv_obj_delete_async() 延后删除整个设置层，因为关闭动作本身也是这个界面的一次事件回调。删除前先将键盘与输入框解绑，再把保存的界面对象指针设为 NULL。

## wifi_credentials.c：输入校验与 C 字符串

配置结构体包含：

~~~c
typedef struct {
    char ssid[33];
    char password[65];
} wifi_credentials_t;
~~~

SSID 最长 32 字节，密码最长 64 字节；数组多出的一个位置用来保存 C 字符串结束符 \0。

wifi_credentials_error() 返回：

- NULL：输入通过校验。
- 一个文字指针：具体错误说明，界面可以直接显示。

例如：

~~~c
const char *error = wifi_credentials_error(ssid, password);
if (error != NULL) {
    lv_label_set_text(s_message, error);
    return;
}
~~~

bounded_length() 最多读取指定数量的字符。它的作用是检查上限时不无止境向后寻找结束符。后续只有输入通过校验，才会复制进固定大小的配置数组。

64 字节密码需要单独检查：它必须是十六进制 PSK，不能把任意 64 字节字符串都当作普通口令交给驱动。

## wifi_clock.c：一次连接请求走过哪些函数

~~~mermaid
flowchart TD
    A[屏幕点击 Connect] --> B[校验名称和密码]
    B --> C[请求复制到队列]
    C --> D[网络后台任务]
    D --> E[连接热点并获取 IP]
    E --> F[保存配置到 NVS]
    E --> G[启动 SNTP 校时]
    G --> H[更新系统时间]
    H --> I[LVGL 定时器刷新标签]
    F --> J[下次开机读取并自动连接]
~~~

### 对外接口：先看 wifi_clock.h

| 接口 | 谁调用 | 返回或产生什么 |
| --- | --- | --- |
| wifi_clock_start() | app_main() 调用一次 | 完成初始化并创建网络后台任务，不等待连接完成 |
| wifi_clock_connect(ssid, password) | 配网页的 Connect 回调 | 校验并复制输入，向队列提交请求；ESP_OK 只代表提交成功 |
| wifi_clock_get_info(&info) | 主界面和配网页 | 获取连接状态、是否保存、SSID、错误码；不返回密码 |
| wifi_clock_is_connected() | 可供其他模块使用 | 读取是否已经获取 IP 的状态，当前保留为便捷接口 |

“请求提交成功”“Wi-Fi 连接成功”“保存成功”“网络校时成功”是不同阶段，界面应按真实阶段显示状态。

### 启动过程

wifi_clock_start() 依次处理：

1. 设置时区。
2. 初始化 NVS、网络接口和默认事件循环。
3. 创建 STA 网络接口。STA 表示开发板作为客户端连接路由器。
4. 创建消息队列。
5. 初始化 SNTP，但先不启动校时。
6. 初始化 Wi-Fi 驱动，选择 STA 模式，并注册网络事件回调。
7. 创建 wifi_worker() 后台任务。

后台任务启动后先调用 read_saved_network()。读到有效配置就自动连接；没有配置就进入 WIFI_CLOCK_SETUP_REQUIRED，等待屏幕输入。

### 为什么要用队列复制输入

wifi_clock_connect() 没有把输入框内部的文字指针直接交给后台任务，而是把名称和密码复制进 wifi_command_t，再将整条消息交给队列。

原因是界面可能被关闭、输入框也可能被删除。如果后台保留的是输入框内部地址，稍后再读取就可能访问失效内存。复制消息后，后台任务持有自己的数据副本。

同时，界面点击函数提交消息后就返回，连接等待和重试不会堵住触摸响应。

### 网络事件也交给同一个后台任务

on_wifi_event() 将 ESP-IDF 网络事件转成内部消息：

| ESP-IDF 事件 | 内部消息 | 后台做什么 |
| --- | --- | --- |
| WIFI_EVENT_STA_START | DRIVER_STARTED | 发起连接 |
| WIFI_EVENT_STA_STOP | DRIVER_STOPPED | 旧连接停完后，开始使用新配置 |
| WIFI_EVENT_STA_DISCONNECTED | DRIVER_DISCONNECTED | 决定重试或提示失败 |
| IP_EVENT_STA_GOT_IP | DRIVER_GOT_IP | 验证当前目标、保存配置、启动校时 |

主界面提交的 COMMAND_CONNECT 和这些驱动事件都在 wifi_worker() 中串行处理，避免多个任务同时修改连接过程。

### 关键函数各负责什么

| 函数 | 职责 |
| --- | --- |
| request_connection() | 接收新的网络配置；若旧 Wi-Fi 正在运行，先停止并等停止事件 |
| start_candidate() | 把候选名称与密码交给驱动，启动 STA，并设置连接超时 |
| connect_now() | 发起一次实际连接请求 |
| fail_attempt() | 结束当前尝试；新配置提示失败，已验证网络安排后续重连 |
| handle_connected() | 检查当前连接确实对应目标 SSID，成功后保存并启动校时 |
| read_saved_network() | 读取并检查已保存记录的版本、长度和字符串格式 |
| save_network() | 写入一个包含名称和密码的记录，并提交 NVS |
| set_info() | 更新不含密码的连接状态快照 |

switching 标志用于处理换网时的中间阶段。停止旧 Wi-Fi 后，不能立刻把旧断线事件当成新网络失败，也不能把旧 IP 事件当成新网络连接成功。因此等待 DRIVER_STOPPED 后才应用新配置，收到 IP 时再检查实际连接的 SSID。

### controller_t 里的状态变量

| 字段 | 意义 |
| --- | --- |
| credentials | 当前正在尝试或使用的名称与密码 |
| running | STA 驱动是否已经启动 |
| switching | 是否正在等待旧 STA 停止，以便换网 |
| auto_reconnect | 配置来自已保存记录，或本次已经成功连接，可以持续重连 |
| save_pending | 新配置尚未成功保存 |
| attempt_active | 当前尝试是否仍应处理结果 |
| retries | 新网络已经进行的重试次数 |
| deadline_us | 当前尝试的截止时间，单位微秒 |
| retry_at_us | 下次发起重连的时间，单位微秒 |

这些变量表达的是“连接过程走到哪一步”。如果只用一个 connected 布尔值，就无法同时表达“正在换网”“已连接但没保存”“输入的新密码错误”等情况。

### 界面看到的状态

| 状态 | 含义 |
| --- | --- |
| WIFI_CLOCK_STARTING | 网络模块正在初始化 |
| WIFI_CLOCK_SETUP_REQUIRED | 没有可用的已保存配置，等待屏幕输入 |
| WIFI_CLOCK_CONNECTING | 正在尝试连接或重连 |
| WIFI_CLOCK_CONNECTED | 已经获取 IP，配置已保存或来自已保存记录 |
| WIFI_CLOCK_CONNECT_FAILED | 新配置连接失败，可以修改后重试 |
| WIFI_CLOCK_SAVE_FAILED | 已连接，但写入 NVS 没成功 |

s_info 用临界区保护。后台更新它时不会同时被界面读到一半；界面取走的是一份完整快照。_Atomic 队列句柄则用于安全发布初始化后的队列地址。

## NVS：为什么驱动用 RAM，配置还能在重启后恢复

代码仍然调用：

~~~c
esp_wifi_set_storage(WIFI_STORAGE_RAM);
~~~

这句话只决定 Wi-Fi 驱动自己的配置保存方式。本工程选择自行管理持久保存：

~~~text
屏幕输入新配置
    ↓
先只交给 RAM 中的驱动配置
    ↓
认证成功并得到 IP
    ↓
save_network()
    ↓
nvs_set_blob() + nvs_commit()
~~~

如果让驱动在设置配置时直接保存，新密码还没验证就可能替换旧记录。现在由应用决定保存时机，因此输错的新密码不会覆盖上一次成功记录。

NVS 读写过程：

~~~text
nvs_open("s31_wifi", ...)
    ↓ 打开本工程的命名空间
nvs_get_blob(..., "station", ...) 或 nvs_set_blob(...)
    ↓ 读取或写入一个完整记录
nvs_commit(...)
    ↓ 写操作提交
nvs_close(...)
~~~

名称、密码和版本号作为一个 blob 保存，而不是分别保存两个互不关联的字段。read_saved_network() 会检查版本、大小及字符串结束符，避免把不符合当前格式的内容当作配置。

这里“连接成功”的标准是取得 IP 地址，不要求 NTP 服务器已经回复。所以 Connected and saved 之后，时钟仍可能短暂显示等待校时，这是正常的两个阶段。

## 三种时间单位，复习时不要混淆

| 位置 | 单位 | 本工程用途 |
| --- | --- | --- |
| lv_timer_create(..., 1000, ...) | 毫秒 | 每秒刷新一次界面 |
| time(NULL) | 秒 | 读取系统日期时间 |
| esp_timer_get_time() | 微秒 | 计算 30 秒连接超时和约 5 秒重试间隔 |

wifi_worker() 每次等待队列最多约 200 毫秒，随后检查截止时间。它没有在界面事件里睡眠 30 秒，用户仍可操作屏幕。

## 二次学习建议：每次只做一个小实验

| 实验 | 改或操作什么 | 预期结果 |
| --- | --- | --- |
| 1. 数据与文字 | 只把按钮文字改成 +100，不改 tap_count += 10 | 实际仍加 10，说明文字不决定数据 |
| 2. 更新周期 | 将 LVGL 定时器周期暂时改为 2000 | 屏幕约两秒刷新一次，系统时间本身没有停止 |
| 3. 本地校验 | 输入 7 字节密码后点 Connect | 立即提示格式问题，不必等 Wi-Fi 超时 |
| 4. 正确保存 | 输入可用网络，看到 Connected and saved 后重启 | 自动连接该网络 |
| 5. 错误配置保护 | 成功保存后，尝试一个错误密码，失败后重启 | 仍使用之前成功保存的配置 |
| 6. 异步界面 | 连接过程中回到主界面，点击加数和清零 | 按钮仍可响应，网络在后台继续处理 |
| 7. 校时与连接 | 观察 Connected and saved 与时间出现的先后 | 理解取得 IP 与收到校时响应是不同阶段 |

实验后把不想保留的改动恢复，再进行下一项。先掌握“对象、事件、状态、定时器”，再逐渐阅读“任务、队列、持久存储”。


## 已验证与待实机确认

- 已在本电脑的 ESP-IDF master（v6.2.0）上编译通过并生成固件。
- 本机 GCC 输入边界测试通过：空值、32/33 字节 SSID、UTF-8 超长、开放网络、7/8/63 字节密码、64 位十六进制 PSK、65 字节密码等。
- 尚未替用户烧录，也未使用真实 Wi-Fi 密码验证设备联网；实机仍需按上面的步骤测试触摸键盘、成功保存、重启恢复及错误密码处理。

## 当前范围

配网页需要手动输入名称，尚未加入自动扫描热点列表。屏幕键盘提供英文字母、数字和符号，尚未加入中文输入法及完整中文字库。当前支持常见 WPA2/WPA3 个人网络和开放网络，不包含企业网络或需网页认证的热点。

## GitHub 同步准备改了什么

为建立 ESP-S31 仓库，额外整理了以下文件；这些改动不改变板上功能：

- `.gitignore`：增加忽略本机 `.vscode/settings.json`、固件和测试可执行文件。原有 `build`、`managed_components`、生成的 `sdkconfig`、旧私有 `main/wifi_config.h` 继续排除。
- `.gitattributes`：源码和文档在 Git 中统一采用 LF 换行，方便跨 Windows/Linux 复现。
- `.vscode/settings.example.json`：新增不含本机安装路径和串口号的配置参考。本机实际设置仍保留在电脑上。
- `README.md`：补充在新电脑打开工程的方法和无需开发板即可运行的输入校验测试命令。
- 上传范围为源码、配置模板、分区表、依赖锁文件、测试和学习文档；不上传构建缓存、固件或旧密码文件。

本机 IDF 环境和串口仍通过 ESP-IDF 扩展选择；复制到其他电脑后，需要按该电脑的实际安装情况重新选择。

## 官方资料

- [S31 Wi-Fi API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/network/esp_wifi.html)
- [S31 NVS 持久存储](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/storage/nvs_flash.html)
- [系统时间与 SNTP 校时](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/system/system_time.html)
