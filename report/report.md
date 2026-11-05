# Lab3 实验报告：断，都可以断

## 一、基本信息与分工

| 项目 | 内容 |
| --- | --- |
| 小组成员 | 母焯行（2513960）、罗熙媛（2514039）、谢北宸（2513513） |
| 模块一 | 母焯行（2513960）：`RESTORE_ALL`、`__alltraps` |
| 模块二 | 罗熙媛（2514039）：`interrupt_handler()`、`exception_handler()` |
| 实验代码 | RISC-V 64 位 ucore Lab3，Ubuntu 20.04、QEMU `virt` |
| 验证日期 | 2026 年 11 月 5 日 |

本报告依据本组当前的 Lab3 源码，以及实际运行 `make clean && make`、`make qemu`、`make grade` 的终端结果整理。最终清理构建、评分和默认 QEMU 运行均记录退出码 0；`make grade` 的结果为 **100/100**，默认运行累计打印 10 次后自动关机。验证记录见 [结果汇总](../validation/results.txt)。

## 二、实验目标与整体执行流

本实验建立一条完整的 Trap 处理路径：处理器发生中断或异常后进入 `stvec` 指向的汇编入口，保存被打断时的寄存器状态，调用 C 语言处理函数，随后恢复现场并通过 `sret` 返回。时钟中断用于检验这条路径能否反复正确执行；非法指令和断点测试用于检验异常处理后返回地址是否正确。

```text
kern_init()
  ├─ idt_init()：将 stvec 指向 __alltraps
  ├─ pmm_init()：完成 Lab2 物理内存管理前置初始化
  ├─ clock_init()：预约第一次时钟事件，允许时钟中断源
  └─ intr_enable()：打开 S 态全局中断

Trap 到来
  → __alltraps / SAVE_ALL：在栈上形成 trapframe
  → trap(tf) / trap_dispatch(tf)：依据 scause 分流
  → interrupt_handler(tf) 或 exception_handler(tf)
  → __trapret / RESTORE_ALL / sret：恢复现场并返回
```

`kern/trap/trapentry.S` 负责入口与恢复；`kern/trap/trap.c` 负责按原因处理事件。前者必须与 `kern/trap/trap.h` 中的 `struct trapframe` 布局一致，后者必须正确处理时钟重预约和异常返回地址。实验代码中的 `pmm_init()` 依赖 Lab2 的 `page_init()`；本组从已完成的 Lab2 代码移植了物理内存初始化和 Best-Fit 模块，使 Lab3 工程能够构建。当前 Lab3 运行时选择的是 `default_pmm_manager`，虽然 Best-Fit 文件参与编译，但本次 Trap 测试没有切换到 Best-Fit。

## 三、模块一：Trap 入口与上下文恢复

### 3.1 已有框架与实现

`idt_init()` 将 `stvec` 设为 `__alltraps`。Trap 到来后，`SAVE_ALL` 在当前栈上预留 `36 × 8 = 288` 字节：第 0～31 格对应通用寄存器，第 32 格保存 `sstatus`，第 33 格保存 `sepc`，第 34、35 格记录异常附加信息和原因。保存 `x2/sp` 时，代码先通过 `sscratch` 取得陷入前的栈指针，再放入第 2 格，因此不能把“当前用于访问 trapframe 的 `sp`”误当成陷入前的 `sp`。

本组在 `__alltraps` 中执行 `SAVE_ALL`，再用 `mv a0, sp` 将 trapframe 的地址作为第一个参数传给 `trap(struct trapframe *tf)`。`trap()` 返回后，控制流顺序进入 `__trapret`。`RESTORE_ALL` 从第 32、33 格恢复 `sstatus` 和 `sepc`，再恢复通用寄存器；`x2/sp` 最后恢复，因为此前的读取仍要用当前 `sp` 访问栈上的各槽。`x0` 恒为零，不需要恢复。最后由 `sret` 按恢复的状态返回。

### 3.2 提示词与规格的写法

模块一的提示词按课程的四段结构组织，完整的整理版见 [prompt.md](prompt.md)：

- `[PROMPT]` 指定只修改 `kern/trap/trapentry.S` 的两个 Lab3 标记位置，保留已有的 `SAVE_ALL` 与 `__trapret`。
- `[RELY]` 列出实际工程的 `REGBYTES`、`LOAD`、`STORE`、`struct trapframe`、`trap(tf)` 和 RISC-V 参数传递约定。
- `[GUARANTEE]` 列明必须完成 `RESTORE_ALL` 与 `__alltraps`，不改变公开接口。
- `[SPECIFICATION]` 把 trapframe 槽位、CSR 恢复、`sp` 最后恢复、`a0` 传参以及 `sret` 返回写成可核查的前置条件和完成条件。

这样写规格的关键是依据现有 `SAVE_ALL` 的真实布局确定恢复顺序，而不是凭函数名猜测栈结构。针对该模块，Codex 先完成了汇编修改和单文件汇编检查；此时尚未宣称整个 Lab3 已通过。

## 四、模块二：中断与异常处理

### 4.1 中断处理

`trap_dispatch()` 通过 `scause` 的最高位区分中断和异常。`interrupt_handler()` 去掉中断标志位后依据编号分支：

| 原因 | 当前实现 |
| --- | --- |
| S 态时钟中断 `IRQ_S_TIMER` | 调用 `clock_set_next_event()` 预约下一次时钟事件；`ticks` 加一；每累计 `TICK_NUM = 100` 次打印一次 `100 ticks`；累计打印 10 次后调用 `sbi_shutdown()`。不修改 `tf->epc`。 |
| S 态软件中断 `IRQ_S_SOFT` | 输出 `Supervisor software interrupt`。 |
| 其他中断 | 输出 `Unknown interrupt: cause <编号>`，再用 `print_trapframe(tf)` 打印现场。 |

时钟处理必须先安排下一次事件，否则当前中断处理完后不会持续产生后续时钟中断。时钟中断发生在指令边界，返回时沿用硬件保存的 `sepc`；这里不能像测试中的断点异常那样固定加 2 或加 4。

当前实现以静态变量 `tick_reports` 记录统计输出次数，该变量在启动清零 BSS 后从零开始。第 10 次调用 `print_ticks()` 后，计数达到 `SHUTDOWN_AFTER`，随后调用已有的 `sbi_shutdown()`。指导书要求达到阈值后关机，但起始工程没有给出该常量的值，因此本组定义 `SHUTDOWN_AFTER = 10`；这对应正常运行中的 1000 次时钟中断。原有 `DEBUG_GRADE` 条件逻辑保留；上述自动关机结果来自默认构建。

### 4.2 异常处理

`exception_handler()` 根据 `tf->cause` 处理两类练习要求的异常：非法指令输出异常地址并令 `tf->epc += 4`，断点输出异常地址并令 `tf->epc += 2`。这两个步长对应本实验测试中使用的 **4 字节非法指令** 和 **2 字节压缩断点指令**；它们不是所有非法指令或断点指令的通用固定长度。其他异常由 `print_trapframe(tf)` 输出现场，当前工程没有为其实现通用恢复策略。

`tf->epc` 决定 `sret` 后从哪里继续：测试中的这两类异常需跳过触发异常的指令；若不更新，返回后可能再次触发同一异常。

### 4.3 提示词与规格的写法

模块二同样采用 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]`。提示词明确目标文件与两个函数签名，提供 `scause`、`epc`、原因宏、`ticks`、`TICK_NUM`、`clock_set_next_event()` 与 `print_trapframe()` 等可依赖信息；在规格中逐一列出时钟、软件、未知中断以及非法指令、断点、其他异常的行为，并要求保留原有分发与返回路径。整理版见 [prompt.md](prompt.md)。

## 五、AI 辅助开发中的实际问题与处理

1. **Lab2 前置代码缺失导致首次整体构建失败。** 完成两个 Trap 模块后，`make` 编译到 `kern/mm/pmm.c` 时因 `page_init()` 缺少实现而失败。核对 Lab3 对 Lab2 的依赖后，本组从已完成的 Lab2 工程移植 `page_init()` 与 Best-Fit 代码，并按 Lab3 已有宏定义调整地址表达式；没有为此改动 Trap 测试或评分脚本。之后运行 `make clean && make`，全部源文件编译并成功链接 `bin/kernel`。
2. **未知中断提示文字需要与测试判据一致。** 初版分支输出 `Unknown interrupt: <编号>`。回读测试要求后，将其改为 `Unknown interrupt: cause <编号>`，并保留后续 `print_trapframe(tf)`。最终评分中的对应检查通过。
3. **补齐自动关机并清理提交标记。** 对照指导书发现初版时钟分支只有持续计数和打印，因此补充 `SHUTDOWN_AFTER = 10`、输出计数及 `sbi_shutdown()` 调用。同时将两处 `lab3 function1` 标记替换为 `2513960`、一处 `lab3 function2` 替换为 `2514039`，删除重复的 `idt_init()` 调用及临时编辑备注。修改后重新完成构建、评分和默认运行验证。
4. **先单独检查、再整体验证。** 模块一完成时只做了单文件汇编和反汇编核对；模块二与 Lab2 前置代码齐备后才进行完整构建、QEMU 运行及评分。报告中的“通过”均对应后续实际运行结果。

## 六、验证方法与结果

在 Ubuntu 的 `/home/student/oslab/lab3-work` 中实际执行：

```bash
make clean
make
make grade
make qemu
```

`make` 成功编译 `kern/trap/trap.c`、`kern/trap/trapentry.S`、物理内存管理等文件，并完成链接，退出码为 0，见 [构建日志](../validation/build.log)。默认 `make qemu` 输出 `memory management: default_pmm_manager`、`check_alloc_page() succeeded!` 和 `++ setup timer interrupts`，随后恰好输出 10 行 `100 ticks`，并在自动关机后退出。运行汇总记录退出码为 0，未使用手动终止或超时终止；源码的关机调用位于第 10 次打印之后。见 [QEMU 日志](../validation/qemu.log) 和 [运行结果汇总](../validation/results.txt)。

`make grade` 退出码为 0，完整记录见 [评分日志](../validation/grade.log)，结果如下：

```text
trap entry, register restore, and breakpoint EPC: PASS (35/35)
illegal instruction EPC advance and return: PASS (25/25)
re-armed supervisor timer and repeated trap return: PASS (40/40)
Total Score: 100/100
```

三项评分覆盖 Trap 入口与寄存器恢复、断点和非法指令的 EPC 更新、时钟事件重预约与重复返回。100/100 是**该评分脚本**的结果，不代表未列入测试的所有异常路径都经过动态验证。

## 七、实验知识点与操作系统原理

| 本实验中的机制 | 对应的 OS 原理与关系 |
| --- | --- |
| `stvec` 指向 `__alltraps` | Trap 入口：处理器将控制权交给内核规定的位置；本实验采用统一入口再分发。 |
| `trapframe`、`SAVE_ALL`、`RESTORE_ALL` | 上下文保存与恢复：把被打断执行流的寄存器状态保存到栈上，处理后恢复。本实验没有实现进程之间的上下文切换。 |
| `scause` 与 `trap_dispatch()` | 事件分类：最高位区分中断和异常，余下编号决定具体处理分支。 |
| `sepc`、`sstatus`、`sret` | Trap 返回：保存返回位置和状态；异常处理函数可按本次指令的语义调整 `epc`。 |
| `clock_set_next_event()`、`ticks` | 定时器：周期性事件是操作系统实现计时和调度的基础；本实验实现计数、打印和达到阈值后关机。 |
| `local_intr_save()`、`local_intr_restore()` | 临界区的中断保护：物理页分配器操作链表时暂时屏蔽本 hart 的中断；这不等同于多核互斥。 |

与本实验相关、但没有在当前代码中实现的 OS 原理还包括：进程与线程调度、完整的系统调用分发、用户态与内核态间的进程切换、外设驱动及中断分发、缺页后按需装页、多核中断同步与嵌套中断管理。时钟中断为未来的时间片调度提供基础，但当前没有调度器；`lab3_switch_test()` 仅在指导书讲解示例中出现，当前起始工程没有这项正式练习接口。

## 八、实验结论

本组完成了 Lab3 的 Trap 入口、上下文恢复、中断与异常处理，并补齐 Lab2 前置实现。最终版本完整构建成功，评分为 **100/100**；默认 QEMU 实测输出 10 次 `100 ticks` 后通过 SBI 自动关机。三处 Lab3 学号标记已更新，初始化流程中的重复调用已清理。评分脚本、注入程序和测试文件保持起始工程内容。

实验让我们把“硬件记录 Trap 原因与返回位置、汇编保存现场、C 处理事件、汇编恢复并返回”的执行链联系起来。提示词设计必须与真实接口和数据布局对应；完成生成后，还需要结合构建、评分和默认运行检查行为是否符合规格。
