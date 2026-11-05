# Lab3 提示词与规格记录

前两节依据本次模块实现时使用的任务、接口和约束整理，供说明规格设计；它们不是聊天记录的逐字导出。第三节保留本次补齐自动关机和整理源码时发送给 Codex 的完整四段式提示词。两个模块先完成并通过评分，随后依据第三节补充修改并重新验证，最终结果见 [report.md](report.md) 和 [验证汇总](../validation/results.txt)。

## 一、Trap 入口与上下文恢复：设计整理版

```text
[PROMPT]
任务：在 kern/trap/trapentry.S 的两处 Lab3 标记位置补全
RESTORE_ALL 与 __alltraps。
操作要求：读取实际文件并修改工程，保留 SAVE_ALL、__trapret、sret
以及 Trap frame 布局，仅补全待完成部分。
输出要求：说明恢复顺序及传参方式，报告实际执行的检查结果。

[RELY]
工程为 RV64，REGBYTES=8，LOAD 对应 ld，STORE 对应 sd。
SAVE_ALL 在栈上建立 36 格 Trap frame，布局与 trap.h 一致：
0～31 格对应 x0～x31；32 格为 sstatus；33 格为 sepc；
34、35 格为异常附加信息和原因；第 2 格保存陷入前的 sp。
void trap(struct trapframe *tf) 按调用约定通过 a0 接收第一个参数。
已有 __trapret 执行 RESTORE_ALL 和 sret。

[GUARANTEE]
完成 .macro RESTORE_ALL 的宏体。
完成 __alltraps 中保存现场后调用 trap(tf) 的逻辑。
保持 SAVE_ALL、struct trapframe 和 trap() 的接口。

[SPECIFICATION]
RESTORE_ALL：
Pre-Condition：sp 指向按现有布局保存的有效 Trap frame。
Post-Condition：sstatus、sepc 和除 x0 外的通用寄存器从该帧恢复，
处理器状态可供后续 sret 返回。
Requirements：所有依赖当前栈基址的读取完成后才恢复 sp；x0 无需恢复；
不要求把异常附加信息和原因写回 CSR。

__alltraps：
Pre-Condition：处理器按 stvec 进入汇编入口，当前执行流使用有效内核栈。
Post-Condition：现场已保存，Trap frame 地址正确传给 trap(tf)；
trap() 返回后可继续进入 __trapret。
Requirements：遵守 RISC-V 调用约定，保留后续恢复所需的栈基址。
```

## 二、中断与异常处理：初版设计整理版

```text
[PROMPT]
任务：在 kern/trap/trap.c 的 Lab3 标记位置实现两个处理函数。
操作要求：直接修改工程文件，保留已有分发、打印和返回逻辑。
输出要求：说明各事件分支行为，并报告实际构建与验证情况。

[RELY]
struct trapframe 中 cause 对应 scause，epc 对应 sepc。
可依赖 IRQ_S_TIMER、IRQ_S_SOFT、CAUSE_ILLEGAL_INSTRUCTION、
CAUSE_BREAKPOINT、ticks、TICK_NUM=100、clock_set_next_event()、
print_ticks()、print_trapframe() 和 cprintf()。
本实验测试使用 4 字节非法指令和 2 字节压缩断点指令。
宏、变量类型和函数声明均以工程源码为准。

[GUARANTEE]
实现 void interrupt_handler(struct trapframe *tf);
实现 void exception_handler(struct trapframe *tf);
保留 trap_dispatch()、trap() 及公开接口。

[SPECIFICATION]
interrupt_handler：
Pre-Condition：tf 为有效中断现场，已完成 Trap 类型分发。
Post-Condition：按原因完成对应处理，并可接入统一返回路径。
Case 1：时钟中断重新预约事件，计数增加一次，每满 100 次打印统计信息。
Case 2：软件中断输出可识别提示。
Case 3：未知中断输出去掉中断标志后的原因编号，并打印 Trap frame。
Requirements：时钟中断不修改 epc。

exception_handler：
Pre-Condition：tf 为有效异常现场，已完成 Trap 类型分发。
Post-Condition：要求处理的异常输出地址，并设置正确的测试返回位置。
Case 1：实验中的 4 字节非法指令输出地址，epc 增加 4。
Case 2：实验中的 2 字节压缩断点输出地址，epc 增加 2。
Case 3：其他异常打印 Trap frame。
Requirements：不把本次测试的两个步长推广为所有异常指令的通用长度。
```

初版完成后，还将未知中断提示修正为 `Unknown interrupt: cause <编号>`，保留现场打印，以符合工程测试的输出判据。初版提示词没有覆盖指导书文字中的关机行为，因此后续使用第三节补齐。

## 三、自动关机与提交标记：实际发送的补充提示词

```text
[PROMPT]

任务：
在 /home/student/oslab/lab3-work 中完善 Lab3 的时钟处理与提交前源码标记。

需要修改的文件：
1. kern/trap/trap.c：
   补充达到指定统计输出次数后自动关机的行为。
2. kern/init/init.c：
   清理重复的 Trap 初始化调用及临时说明文字。
3. kern/trap/trapentry.S：
   按已确认的负责人学号更新两处 Lab3 标记。

操作要求：
- 读取工程真实源码后直接修改实际文件。
- 保留已通过测试的 Trap 保存、恢复、分发和异常处理逻辑。
- 保留 lab3 function1、lab3 function2 注释。
- 不修改评分脚本、注入程序和测试文件。
- 不执行 Git 提交或推送。

输出要求：
- 说明实际修改的文件与行为。
- 实际构建、评分并运行默认 QEMU，保存验证结果。
- 只有观察到对应结果后，才能声明通过。


[RELY]

当前工程：
- RV64 ucore，已有 Trap 模块的 make grade 结果为 100/100。
- interrupt_handler(struct trapframe *tf) 位于 kern/trap/trap.c。
- kern_init(void) 位于 kern/init/init.c。

时钟处理相关信息：
- trap.c 已定义 TICK_NUM 为 100。
- ticks 是 kern/driver/clock.c 中定义的全局时钟计数。
- IRQ_S_TIMER 表示 S 态时钟中断。
- clock_set_next_event() 用于预约下一次时钟事件。
- print_ticks() 打印一行 "100 ticks"。
- 当前时钟分支每收到一次中断增加一次 ticks，
  每累计 TICK_NUM 次调用一次 print_ticks()。
- libs/sbi.h 已声明 void sbi_shutdown(void)，
  libs/sbi.c 已提供实现。

工程差异：
指导书要求累计输出达到 SHUTDOWN_AFTER 后关机，
但当前起始工程没有定义该常量，也没有给出具体数值。
本组选择默认打印 10 次统计信息后关机。
10 是本组选择的参数，不是指导书指定的数值。

初始化现状：
kern_init() 在 pmm_init() 前后各调用了一次 idt_init()。
保留 pmm_init() 前的初始化调用即可覆盖后续启动流程。

已确认分工：
- trapentry.S 两处 lab3 function1：2513960。
- trap.c 一处 lab3 function2：2514039。


[GUARANTEE]

必须修改的接口：

1. void interrupt_handler(struct trapframe *tf);
   补充时钟统计达到阈值后的自动关机行为。

2. int kern_init(void);
   清理重复的 idt_init() 调用和临时说明文字，
   保持正常初始化流程。

必须更新的源码标记：

- 两处 #lab3 function1 YOUR CODE：
  将 YOUR CODE 替换为 2513960。
- 一处 //lab3 function2 YOUR CODE：
  将 YOUR CODE 替换为 2514039。

允许新增本模块内部所需的宏和静态状态；
如需辅助函数，应设为 static。
保持现有公开函数签名。


[SPECIFICATION]

一、interrupt_handler()

Pre-Condition：
- tf 指向已建立的有效 Trap frame。
- Trap 原因已经被分发为中断。
- 时钟模块已初始化。

Post-Condition：
- 当前中断按其类型处理。
- 正常时钟处理中，下一次事件已预约，时钟计数增加一次。
- 每累计 100 次时钟中断输出一次统计信息。
- 默认累计输出 10 次统计信息后，请求系统关机。
- 时钟分支不修改 tf->epc。

Case 1：时钟中断，尚未达到关机阈值
- 完成下一次时钟事件预约与计数。
- 达到统计输出间隔时，调用已有的 print_ticks()。
- 返回统一 Trap 返回路径，使内核可以继续接收时钟中断。

Case 2：时钟中断，达到关机阈值
- 先完成本次应有的统计输出。
- 再调用框架已有的 sbi_shutdown()。
- 阈值按统计信息的输出次数计算：
  默认 10 次输出对应 1000 次时钟中断。

Case 3：软件中断或未知中断
- 保留现有提示和 Trap frame 输出行为。

Requirements：
- 定义清晰的 SHUTDOWN_AFTER 默认值。
- 保留已有 print_ticks() 中的 DEBUG_GRADE 条件逻辑。
- 不改动 exception_handler() 中已通过测试的异常处理。


二、kern_init()

Pre-Condition：
- 内核进入 C 初始化阶段。

Post-Condition：
- Trap 入口在 pmm_init() 前完成初始化。
- 时钟初始化与全局中断开启顺序保持正确。
- idt_init() 只调用一次。
- 删除注释中的临时编辑备注，保留有意义的技术说明。


三、验证要求

1. 清理后完整构建，确认编译和链接成功。
2. 执行 make grade，核对三项评分及总分。
3. 运行默认 make qemu：
   - 观察时钟统计连续输出；
   - 核对默认输出 10 次；
   - 验证随后通过 sbi_shutdown() 自动退出，
     不将手动终止或超时视为自动关机成功。
4. 检查三处 Lab3 标记均已替换为对应学号。
5. 保存实际验证输出，并报告尚未解决的问题。
```
