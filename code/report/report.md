# Lab4 实验报告：虚拟页映射与内核线程管理

日期：2026-11-11。工程：`/home/student/oslab/lab4-work`，RV64、Sv39。

本报告依据当前源码、[完整提示词](prompt.md)、[实施进度](progress.md)及既有验证日志编写。文档整理阶段没有重新运行构建、评分或 QEMU。下文分别标明**运行证据**、**代码分析**和**未验证范围**；通过课程测试不等于完成通用操作系统。

## 1. 小组、范围与交付状态

| 成员 | 学号 | 本次模块职责 |
| --- | --- | --- |
| 母焯行 | 2513960 | 模块一：虚拟内存页映射 |
| 罗熙媛 | 2514039 | 模块二：进程创建及初始化 |
| 谢北宸 | 2513513 | 模块三：线程调度与上下文切换 |

三个模块负责人已由小组确认，如上表所示。五处源码署名分别更新为 `LAB4 function1 2513960`、`LAB4 function2 2514039` 和三处 `LAB4 function3 2513513`，保留 LAB4 function 标记；仅替换注释中的 YOUR CODE，不改变实现。此前分工待确认的过程记录保留在 progress.md。

实验已实现普通虚拟页映射、内核线程创建和主动调度切换。内核线程共享内核页表，但各新线程拥有独立内核栈；idle 例外，复用启动栈。未实现用户进程、时间片抢占和完整退出回收。当前没有执行 Git 提交或推送，不将本地完成写作已上传。

## 2. 本章整体逻辑线

按 [init.c](../kern/init/init.c) 的真实顺序，启动先清零 BSS、初始化 DTB 和控制台、打印内核信息，再进入：

```text
kern_init
  → pmm_init
  → pic_init → idt_init
  → vmm_init
  → proc_init
  → clock_init → intr_enable
  → cpu_idle → schedule → proc_run → switch_to
```

`pmm_init` 建立可分配物理页集合、确定启动页表并执行页表检查，最后初始化内核动态分配器。中断入口为后续异常保存现场，VMM 初始化后，`proc_init` 才能安全申请 PCB 和线程栈。时钟及中断开启后，idle 根据 `need_resched` 进入调度器。物理内存、页映射、线程状态、寄存器现场因此形成一条依赖链，而不是三个互不相关的练习。

前置适配见 [pmm.c](../kern/mm/pmm.c)、[default_pmm.c](../kern/mm/default_pmm.c)、[trap.c](../kern/trap/trap.c) 和 [trapentry.S](../kern/trap/trapentry.S)。`default_pmm_manager` 是兼容框架保留的公开符号，实际为 **Best-Fit**：选择最小可满足空闲块，分割后更新计数，释放时按地址插入并合并相邻块。`page_init` 从 DTB 获取范围，以 `npage - nbase` 而非绝对物理页号上界计算描述符数量，保留内核、描述符占用空间，仅登记剩余完整页。Trap 适配保留 Lab4 的 `forkrets`；时钟处理预约下一事件并累计 ticks，没有移植旧实验打印十次后关机的逻辑。

## 3. 模块一：页映射、引用计数与 TLB

实现位于 [pmm.c](../kern/mm/pmm.c)，接口和转换辅助函数见 [pmm.h](../kern/mm/pmm.h)，页结构见 [memlayout.h](../kern/mm/memlayout.h)，位定义见 [mmu.h](../kern/mm/mmu.h)。

### 3.1 建立与更新映射

`page_insert` 先调用 `get_pte(pgdir, la, 1)`。若失败，立即返回 `-E_NO_MEM`，尚未增加目标页引用，也没有安装目标末级映射。成功后先增加目标页引用，再处理已有项：

| 原映射 | 引用处理 | 最终行为 |
| --- | --- | --- |
| 无有效映射 | 新页引用加一 | 安装目标映射 |
| 映射到同一页 | 抵消刚增加的一次引用 | 仅更新权限，不释放该页 |
| 映射到另一页 | 撤销旧引用，归零释放旧页 | 保留新页增加的一次引用，替换映射 |

最后通过 `pte_create(page2ppn(page), perm)` 写项，按框架约定自动置 `PTE_V`，并调用 `tlb_invalidate`。同页更新不能先释放旧页再重新映射，否则可能把仍在使用的页交回分配器。

### 3.2 撤销映射

`page_remove` 使用 `create=0` 查询，不为删除分配页表。没有 PTE 或 PTE 无效时安静返回。内部 `page_remove_pte` 对有效项取出旧物理页，清零页表项、使对应地址 TLB 失效，然后减少引用，归零时调用 `free_page`。重复删除不会重复减引用。接口不自动回收空的中间页表，保持框架检查中显式释放这些页的生命周期。

**代码分析：**页表是内存中的权威数据，TLB 是硬件缓存，改前者后必须同步后者。当前 `tlb_invalidate` 实际执行 `sfence.vma la`，并未检查 `pgdir` 是否为当前页表；不能把其注释中的条件当成已实现的判断。当前作用限于本地地址翻译同步，不是多核 TLB shootdown。

**范围：**框架部分测试以 `perm=0` 检查 PTE 有效位、页号和引用关系，这不表示任意权限组合都形成硬件可访问的普通叶子映射。需要实际读写的映射设置相应 R/W 权限。实现遵循当前教学接口，不宣称支持任意大页拆分、全部分页模式或所有合法性检查。`get_pte` 的部分分配失败仍可能保留中间页表，详见 Challenge 2。

## 4. 模块二：进程创建与首次运行现场

主体见 [proc.c](../kern/process/proc.c)，PCB、context 和状态声明见 [proc.h](../kern/process/proc.h)。

### 4.1 基础 PCB 与 idle

`alloc_proc` 用 `kmalloc` 申请 PCB，失败返回 NULL；成功后整体清零，再设 `state=PROC_UNINIT`、`pid=-1`、`pgdir=boot_pgdir_pa`，两个链表节点分别 `list_init`。因此 runs、kstack、need_resched、flags 为零，parent、mm、tf 为 NULL，context 和 name 为零。

`proc_init` 初始化普通进程列表和 PID 哈希表。idle 不是重新启动一个线程，而是把当前启动执行流登记成 PCB：PID 0、RUNNABLE、kstack 指向 bootstack、need_resched=1、runs=0，context 仍为初始零值；`current=idleproc`。idle 计入 `nr_process=1`，但不加入普通线程列表或哈希表，供调度器单独保底。它第一次被切出时，实际继续执行位置才由 `switch_to` 保存。

随后通过 `kernel_thread(init_main, "Hello world!!", 0)` 创建 init。初始 PID 分配为 1，父线程为 idle，按 PID 查到后命名为 `init`；失败明确 panic，不解引用无效 PCB。框架 `set_proc_name` 固定复制 15 字节，调用时使用补零定长名称数组。

### 4.2 资源分配、发布与清理

`do_fork` 先检查数量上限，再申请 PCB、关联 parent、调用 `setup_kstack` 申请 `KSTACKPAGE=2` 个连续页，即 8 KiB 内核栈；然后执行 `copy_mm`、`copy_thread`。完整初始化后，在关中断区再次检查上限，再依次分配 PID、登记哈希与 `proc_list`、增加一次计数、`wakeup_proc` 置 RUNNABLE，最后恢复中断状态。

| 失败位置 | 返回与清理（代码分析） |
| --- | --- |
| 初始数量上限 | `-E_NO_FREE_PROC`，无资源申请 |
| PCB 分配 | `-E_NO_MEM` |
| 内核栈分配 | 释放 PCB，返回分配错误 |
| copy_mm 不支持当前地址空间 | 释放内核栈和 PCB，返回 `-E_INVAL` |
| 发布前再次达到上限 | 先恢复中断，再释放栈和 PCB，返回 `-E_NO_FREE_PROC` |

上述失败均发生在登记之前，因此不会留下哈希、列表或计数残余。这里是**静态路径审查**，不是所有资源耗尽情形均经失败注入测试。

`copy_mm` 当前只接受 `current->mm == NULL`，子线程 mm 保持 NULL；非空则明确报错。`CLONE_VM` 表示本阶段的共享语义，不代表已经实现用户地址空间共享或复制。

### 4.3 构造初始 Trap frame

`kernel_thread` 清零临时 Trap frame，令 s0 保存函数 fn、s1 保存 arg，epc 为 `kernel_thread_entry`。status 继承当前值后置 SPP、SPIE，清 SIE：恢复期间 S 态中断总开关关闭，`sret` 按 SPP 回到 S 态，并由 SPIE 恢复相应中断开关。

`copy_thread` 把临时 frame 复制到新线程自己的栈内，而不是保存指向调用者局部变量的指针。布局见 [trap.h](../kern/trap/trap.h)：RV64 frame 为 36×8=288 字节。新 frame 位于 `kstack + 8192 - 288`，保持 16 字节对齐；a0=0；esp 非零则保存给定值，零时令保存的 sp 指向新内核栈顶。设置 `context.ra=forkret`、`context.sp=proc->tf`。

完整首次运行链路为：

```text
switch_to
  → ret 到 forkret
  → forkrets(current->tf)
  → sp 指向 Trap frame，跳到 __trapret
  → RESTORE_ALL 恢复 sstatus、sepc、通用寄存器，最后恢复 sp
  → sret 到 kernel_thread_entry
  → a0=s1，jalr s0
  → init_main("Hello world!!")
```

对应 [entry.S](../kern/process/entry.S) 与 [trapentry.S](../kern/trap/trapentry.S)。fn 返回后，entry.S 执行 `jal do_exit`，本阶段没有正常退出回收实现。

## 5. 模块三：调度与上下文切换

### 5.1 调度选择

[sched.c](../kern/schedule/sched.c) 中 `schedule` 在保存中断状态并关中断后清除当前 need_resched。current 是 idle 时从列表头之后开始；否则从当前节点之后开始，沿环形列表查找 RUNNABLE，跳过哨兵，不将列表头转换为 PCB。

遍历最多一圈：其他线程可运行时按循环顺序选择；仅 current 可运行时最终重新选择自身；无普通 runnable 时保底 idle。选中者 runs 增加一次，同线程不执行实际切换。这是选择次数的记录，不是 CPU 运行时长。`cpu_idle` 持续检查 need_resched，必要时调用 schedule。

调度测试通过主动调用 schedule 驱动轮转；[trap.c](../kern/trap/trap.c) 的时钟处理没有分配时间片或调用抢占调度，故不能称为完整的定时器抢占式时间片 RR。

### 5.2 current、页表与执行现场

[proc.c](../kern/process/proc.c) 中 `proc_run` 对同线程直接返回；否则在关中断区保留 prev、更新 current，使用目标 PCB 的物理 pgdir 构造：

```c
((uintptr_t)SATP_MODE_SV39 << 60) | (proc->pgdir >> RISCV_PGSHIFT)
```

写入 satp 后全局刷新本地 TLB，并在边界使用编译器内存屏障，然后调用 `switch_to(&prev->context, &proc->context)`。所有当前内核线程的根页表相同，但实现仍使用目标字段。没有调用 [riscv.h](../libs/riscv.h) 中参数为 unsigned int、使用 SATP32_MODE 的旧 `lsatp`，避免地址截断及模式错误。

切走后，旧线程不会立即继续。它未来重新被选择时，才在原栈上从 switch_to 调用之后继续，恢复自身 proc_run 保存的中断状态，再返回 schedule，恢复 schedule 保存的外层状态。新线程首次运行则走预先构造的 forkret 链路。

### 5.3 context 与 Trap frame、ret 与 sret

| 比较项 | context | trapframe |
| --- | --- | --- |
| 用途 | 内核 C 调用边界处切换线程的继续执行位置 | 保存陷阱现场，或人工构造首次进入现场 |
| 内容 | ra、sp、s0～s11，14 个 uintptr_t，112 字节 | 32 个通用寄存器槽及 status、epc、badvaddr、cause，288 字节 |
| 保存/恢复 | switch_to | SAVE_ALL / RESTORE_ALL |
| 控制转移 | ret 使用恢复后的 ra | sret 使用 sepc 并调整特权/中断状态 |

[switch.S](../kern/process/switch.S) 以 a0/a1 为 from/to 基址，偏移 0、8、…、104 字节保存与恢复 14 个寄存器；不破坏仍需使用的寻址基址。ra 在 ABI 中属于 caller-saved，但这里必须保存它来恢复线程的继续执行位置。其他 caller-saved 寄存器由 C 调用约定处理；这不是异步陷阱的完整现场保存。ret 本身不恢复 sstatus 或切换特权级，sret 才执行陷阱返回语义。

共享页表只表示地址空间共享，不代表执行栈共享。独立内核栈让每个线程保留自己的函数调用链、局部变量和陷阱现场。

## 6. 各练习 spec 与 prompt 的设计

完整原文按阶段保存在 [prompt.md](prompt.md)，同一阶段包含 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]`，不能用本节概述代替原文。

| 练习 | 完整原文位置（prompt.md 的标题） | 设计重点与验证边界 |
| --- | --- | --- |
| 模块一 | 2026-11-11 Lab4 功能模块一：完整用户提示词 | 前置限定有效页表/描述符；后置限定映射、引用、TLB 一致性；枚举新建、同页、替换、失败、重复删除。显式禁止改 get_pte/测试，要求暴露部分分配不回滚的边界 |
| 模块二 | 2026-11-11 Lab4 功能模块二：完整用户提示词 | 逐字段规定 PCB 初态，明确共享页表/独立栈、发布临界区、失败逆序释放；按真实 entry.S 约定设计 s0/s1 和首次 frame，拒绝假实现用户 mm |
| 模块三 | 2026-11-11 Lab4 功能模块三：完整用户提示词 | 给出有限遍历、自身重选、idle 保底及 RV64 布局条件；区分协作轮转与抢占；要求顺序执行真实构建、评分及默认运行，记录真实退出原因 |

这些提示先要求读取代码和框架测试，再将接口签名、前置条件、后置不变量、边界情形和允许修改范围写明，防止 AI 用覆盖旧文件、空桩或改测试解决阶段性链接失败。模块一、二只要求各自编译和静态审查，完整动态验证留到模块三，避免在调度尚缺时误报全部完成。

前置适配的代码已核对，但当前 `prompt.md` 的历史归档从模块一开始；本报告不声称该文件已有前置适配轮次的完整原始提示词。三个 Lab4 功能练习的原文均实际存在。文档整理要求也追加到该文件，既有内容不重写。

## 7. AI 辅助实现中的真实问题与处理

过程依据 [progress.md](progress.md)、当前代码和验证日志。以下区分静态发现、分阶段缺项与实际运行问题，不虚构故障。

| 问题与证据类别 | 处理过程和结论 |
| --- | --- |
| 前置适配：不存在的 best_fit_pmm.h；参考尾部插入缺少退出（前轮对话及静态核对） | 改用真实 default_pmm.h；插入后 break，防止重复插入。现代码与结论吻合，但没有单独保存该轮运行故障日志，不描述成动态测试曾触发此 bug |
| 描述符数量和结构布局（代码分析） | 使用 npage-nbase 及当前 sizeof(Page)，保留范围/容量/对齐检查，没有整体覆盖 Lab4 文件 |
| 模块一/二阶段链接缺少后续接口（进度记录） | 随模块推进逐步补齐，未加空桩。早期描述是当时状态，不能覆盖最终整工程构建成功的证据 |
| 同页更新可能误释放（静态设计） | 引用先加后抵消，不走旧页释放路径；评分随后覆盖同页权限更新和空闲页数不变 |
| set_proc_name 固定复制 15 字节（静态发现） | 提供补零定长数组，不修改框架函数；没有声称曾发生运行越界报错 |
| lsatp 遗留 Sv32 实现（静态发现） | proc_run 自行构造完整 RV64 Sv39 satp；不是先运行失败再修复 |
| 首次评分无串口输出（真实验证失败） | 原 make grade 退出 2、0/30；独立探测确认 GDB socket 创建受沙箱拒绝。获准沙箱外重跑同一评分命令后 30/30，没有改脚本或伪造 PASS |
| 默认运行很快退出（真实观察及调用链分析） | 独立 PTY 保持输入打开仍退出；读取原 kmonitor 发现 sbi_shutdown，解释为既有框架退出路径，未添加自动关机 |
| kmalloc.c:92 类型转换警告（真实构建输出） | 构建成功但警告仍在，不在授权源码范围内，保留并报告，不称零警告构建 |

## 8. Challenge 1：保存与恢复中断开关

### 8.1 真实实现及两个 CSR 的区别

[sync.h](../kern/sync/sync.h) 的 `__intr_save` 用 `read_csr(sstatus) & SSTATUS_SIE` 检测原状态：若原来开启，调用 intr_disable 并返回 1；原来关闭则返回 0，不再改变状态。`__intr_restore(flag)` 仅在 flag 非零时 intr_enable。

[intr.c](../kern/driver/intr.c) 分别调用 `set_csr(sstatus, SSTATUS_SIE)` 与 `clear_csr(sstatus, SSTATUS_SIE)`；[riscv.h](../libs/riscv.h) 中 read/set/clear 宏对应 csrr/csrrs/csrrc。这里保存的是开关布尔值，不是整个 sstatus，恢复不会覆盖其他位。

`sstatus.SIE` 是 S 态相应中断的全局使能位；独立 CSR `sie` 是逐类中断的使能集合，例如 [clock.c](../kern/driver/clock.c) 设置其中的 STIE（代码通过 MIP_STIP 位掩码）。开启总开关不意味着所有中断种类都被允许；仍取决于单类使能、pending 及特权/委托条件。这两个同名相关概念不能混淆。

### 8.2 为什么必须恢复原状态

临界区入口不能只“关中断”，出口也不能无条件“开中断”。调用者可能本就处在外层临界区；无条件开启会提前打破外层保护。正确嵌套使用不同局部变量、按逆序恢复：

```c
bool outer, inner;
local_intr_save(outer);  // 原 SIE=1：outer=1，SIE→0
local_intr_save(inner);  // 当前 SIE=0：inner=0，仍为0
/* 内层临界区 */
local_intr_restore(inner); // flag=0，不开启，仍为0
/* 外层临界区继续 */
local_intr_restore(outer); // flag=1，恢复到1
```

若最初为 0，两份记录均为 0，结束仍关闭。该推导要求配对、逆序使用，且受保护代码不擅自重新开启中断：实际 restore(0) 是不操作，并不是强制清位。不能复用同一个保存变量导致外层记录丢失。

`local_intr_save(x)` 的 `do { ... } while (0)` 将宏体封装成只执行一次的单条语句，使调用者可自然加分号并安全置于 `if/else` 中，局部语句扩展也不会破坏语法；它不是自旋循环。当前 restore 宏只是函数调用，不能凭惯例说它也使用了 do/while。

### 8.3 本实验用途与边界

`do_fork` 在 PID 分配、哈希/列表登记、计数和 runnable 发布期间保护不变量；`schedule` 保护选择和状态更新；`proc_run` 保护 current、satp 和实际寄存器现场切换。schedule 内再调用 proc_run 时形成正确嵌套：内层见中断已关，不会提前开启；旧线程未来切回后再分别恢复内外层状态。首次线程则由自己的初始 status 和 sret 进入可接收相应中断的状态。

这种方法只影响**当前 CPU/hart 的相应 S 态中断**。它不屏蔽同步异常，不阻止其他 CPU 同时访问共享数据，也不保证屏蔽 M 态中断；多核系统仍需锁、内存顺序控制及必要的跨核同步。它不能替代全部互斥机制，也不是“停止 CPU 的所有异常”。本节为代码与体系结构分析，没有额外执行中断嵌套压力测试。

## 9. Challenge 2：get_pte 的相似代码与接口设计

### 9.1 两段代码为何相似

[pmm.c](../kern/mm/pmm.c) 的 get_pte 是固定三级路径。按 Sv39 从高到低称 L2、L1、L0：第一段用 `pgdir[PDX1(la)]` 取得根表 L2 项 pdep1（VA[38:30]），需要时分配一页作为 L1 表；第二段通过 `KADDR(PDE_ADDR(*pdep1))` 定位 L1 表，用 PDX0（VA[29:21]）选项 pdep0，需要时分配 L0 表；最后 PTX（VA[20:12]）返回 L0 中的目标项指针，低 12 位为页内偏移。

两段都检查 V 位；路径缺失且 create 允许时 alloc_page、设页表页引用为 1、清零整页，再安装指向下一层的项。相似性来自多级页表各中间层重复的“索引—检查—创建—下钻”结构，并不是把同一层误写两次。当前代码安装的是 `PTE_U | PTE_V`，没有 R/W/X；本报告陈述真实实现，不把它推广成生产级 PTE 位合法性校验。

| 模式 | 适用寄存器宽度 XLEN | 有效虚拟地址位宽 | 级数 | 每级索引 | PTE 大小 | 每页条目 | 普通页 | 普通页路径的中间处理段数 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Sv32 | 32 | 32 位 | 2 | 10 位 | 4 字节 | 1024 | 4 KiB | 1 |
| Sv39 | 64 | 39 位 | 3 | 9 位 | 8 字节 | 512 | 4 KiB | 2 |
| Sv48 | 64 | 48 位 | 4 | 9 位 | 8 字节 | 512 | 4 KiB | 3 |

Sv39/Sv48 的名字不意味着寄存器只有 39/48 位：地址仍装在 RV64 寄存器中，合法虚拟地址需满足高位的规范扩展要求。Sv39 是位 63:39 与位 38 一致，Sv48 是位 63:48 与位 47 一致。位宽比较说的是虚拟地址规则，不应混为物理地址宽度。

普通 4 KiB 页需走到末级，因此 N 级分页重复 N−1 次中间处理；实现可以改用按层数循环，而不必分别展开一、二、三段。每级页表恰占 4 KiB，由 PTE 宽度决定一页容纳的项数及索引位数。

**高层叶子与工程边界：**有效项不一定指向下一层。一般 R 或 X 置位时可能是叶子（W 等位组合还须合法），较高层叶子表示相应对齐的大页。Sv39 可在 L1/L2 表示 2 MiB/1 GiB 映射；处理它们时须辨识叶子、检查对齐、决定停止或拆分，不能直接把其物理页当作页表。

当前 get_pte 只检查 V 并固定下钻，没有任意大页识别、拆分和通用模式遍历。启动 [entry.S](../kern/init/entry.S) 中确有高层启动映射，这不等于 get_pte 能管理该高层叶子内部的任意地址。模块一的承诺仅限框架支持的普通页管理路径。Sv32/Sv48 比较是分析，不是本工程已经运行这两种模式的证据。

### 9.2 查找与页表分配合并是否合理

`create=0` 是只查路径：任一中间表缺失就返回 NULL，不分配；若路径存在，即使末级项无效，也可返回该项指针。`create=1` 允许建立缺失的中间表，最终返回可写目标项；它并不替调用者分配目标数据页或建立目标页的映射。

合并接口的优点是页表遍历代码只写一次，插入、查询、删除使用统一规则。调用者通过一个参数表达意图，适合本教学工程的固定三级实现。缺点是名字像查询却可能产生内存和页表修改副作用，调用者必须关注失败点、锁和页表页的归属。

真实失败边界为：第一张中间表分配并挂接成功、第二张分配失败时，get_pte 返回 NULL，但第一张表、其引用和根项仍保留。page_insert 此时尚未改变目标数据页引用或安装末级映射，但不能声称整条路径回滚。page_remove 也不负责回收所有空表；二者是不同的资源管理决策。

可选拆分为“只查找项”的接口与“保证路径存在”的接口，后者显式负责分配，记录本次新建表以约定失败回滚或保留策略；内部仍复用按层遍历辅助逻辑。优点是副作用更清晰、便于错误注入和资源审计；代价是接口与清理协议增多，若不共享实现还可能重复遍历逻辑。

**权衡结论：**当前可保留带 create 的教学接口，同时明确只查/可创建语义和部分失败保留行为。若扩展到多种分页模式、大页、多核或严格内存耗尽处理，再考虑拆分并定义回滚契约。本题仅做分析，未修改 get_pte，也未为它添加新接口或测试。

## 10. 实际验证证据

全部使用已保存记录，本轮没有重跑测试。原命令和评分规则见 [Makefile](../Makefile)、[grade.sh](../tools/grade.sh)、[inject.py](../tools/inject.py)、[test_lab4.c](../tools/test_lab4.c)。make grade 在临时树中注入测试，不能把该镜像与默认镜像混为一谈。

| 阶段 | 日志 / 退出码文件 | 实际结果 |
| --- | --- | --- |
| 清理 | [clean.log](../validation/module3/clean.log) / [clean.exit](../validation/module3/clean.exit) | 0 |
| 完整构建 | [build.log](../validation/module3/build.log) / [build.exit](../validation/module3/build.exit) | 0；保留 kmalloc.c:92 警告 |
| 首次评分 | [grade.log](../validation/module3/grade.log) / [grade.exit](../validation/module3/grade.exit) | 2；0/30；三项无 QEMU 输出 |
| 权限定位 | [gdb-port-probe.log](../validation/module3/gdb-port-probe.log) | `Failed to create a socket: Operation not permitted`，环境限制 |
| 原命令重试评分 | [grade-retry.log](../validation/module3/grade-retry.log) / [grade-retry.exit](../validation/module3/grade-retry.exit) | 0；30/30 |
| 默认镜像重建 | [default-build.log](../validation/module3/default-build.log) / [default-build.exit](../validation/module3/default-build.exit) | 0，未注入测试 |
| 默认运行 | [default-qemu.log](../validation/module3/default-qemu.log) / [default-qemu.exit](../validation/module3/default-qemu.exit) | 0，script 记录 |
| 保持输入打开的 PTY 复核 | [default-qemu-pty.log](../validation/module3/default-qemu-pty.log) / [default-qemu-pty.exit](../validation/module3/default-qemu-pty.exit) | 0，同一框架退出路径 |

评分日志实际为：

```text
-check PMM:             OK
-check Process Create:  OK
-check Schedule:        OK
Total Score: 30/30
```

[评分串口副本](../validation/module3/grade-retry-qemu.log) 实际包含三项 `[Test ...] PASS`。PMM 测试覆盖基础映射、替换、同页更新及重复删除；创建测试检查 idle/init 字段；调度测试创建两个附加线程，主动 schedule 后检查二者都执行过。不能由这些断言推出严格公平性、全部寄存器压力测试或多核正确性。

默认日志实际包含 `check_alloc_page()`、`check_pgdir()`、`check_boot_pgdir()`、`kmalloc_init()`、`check_vma_struct()`、`check_vmm()` 成功，及：

```text
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
kernel panic at kern/process/proc.c:208:
    process exit!!.
```

默认运行没有评分 PASS 注入文本。以上输出是运行证据；退出原因还需结合代码：init_main 返回后，entry.S 调 do_exit，原有 do_exit 调 panic，[panic.c](../kern/debug/panic.c) 关闭中断后进入 [kmonitor.c](../kern/debug/kmonitor.c)，后者在打印欢迎信息后无条件调用 [sbi_shutdown](../libs/sbi.h)。[退出说明记录](../validation/module3/default-exit-explanation.txt) 与两个退出码文件一致：没有 Ctrl-A X，没有超时或信号终止，没有为本实验新增关机代码。评分注入增加包含行等会导致 panic 行号与默认镜像不同，这不构成另一个故障。

只把这条已确认的 `process exit!!.` 路径视为当前框架终点；退出码 0 本身不足以证明内核正确，其他 panic 也不能照此解释。

[布局检查日志](../validation/module3/layout-check.log) 记录既有静态编译检查退出 0，[switch 反汇编](../validation/module3/switch-disassembly.log) 可核对 14 个偏移，[修改范围检查](../validation/module3/scope-check.log) 记录模块三仅改获准源码。[环境预检](../validation/module3/preflight.log) 保留了 netlink 权限限制及 /proc TCP 检查，不把受限工具调用写作完整进程/端口探测保证。

**未独立动态验证：**内存耗尽注入、get_pte 完整回滚、全部嵌套中断状态、单线程重选/空列表的独立用例、多核并发与远端 TLB、不同用户页表切换、Sv32/Sv48、任意大页处理、浮点/向量扩展现场。代码分析和现有 30/30 不能替代这些验证。

## 11. 与操作系统原理的联系、差异及未实现内容

| OS 原理 | 本实验对应 | 教学实现的差异或边界 |
| --- | --- | --- |
| 虚拟内存与间接寻址 | 多级页表、page_insert/remove、TLB 失效 | 实现普通路径，非通用大页/全部权限校验器 |
| 对象所有权和资源生命周期 | Page 引用计数，PCB/栈失败逆序清理 | 计数不是多核锁；页表部分创建不完整回滚 |
| 进程与线程 | PCB 描述状态和继续执行位置，parent 建立关联 | 实际仅内核线程；mm=NULL，没有用户隔离 |
| 机制与策略分离 | switch_to 提供切换机制，schedule 选择线程 | 主动轮转，没有时间片、公平性保证或优先级策略 |
| 中断与临界区 | 保存/恢复当前 SIE，保护发布和切换 | 单 hart 局部保护，不能替代 SMP 锁与跨核同步 |
| 特权级与 ABI | context 遵守调用边界，trapframe 支持 sret | 未覆盖用户入口、系统调用及扩展寄存器完整保存 |

重要但尚未实现的原理包括：用户地址空间建立与 fork/exec、写时复制和按需分页、缺页恢复及页面置换；进程退出后的资源回收、僵尸态与 wait；阻塞/唤醒的完整等待队列；抢占式调度、时间片和调度公平性；SMP 自旋锁、内存一致性及跨核 TLB shootdown；用户权限隔离和更完整的异常处理。结构体或框架注释中出现有关概念，不意味着相应功能已经完成。

## 12. 提交检查与 Challenge 覆盖核对

| 项目 | 状态 |
| --- | --- |
| 三个功能模块与已有动态证据 | 已核对，评分 30/30；不扩大覆盖范围 |
| Challenge 1 | 已覆盖真实实现、SIE/sie 区别、状态保存恢复、宏语法、逆序嵌套、作用域与临界区用途 |
| Challenge 2 题目一 | 已覆盖真实层级、三级模式比较、有效位宽、段数、循环抽象、大页及工程边界 |
| Challenge 2 题目二 | 已覆盖 create 语义、合并利弊、部分失败资源、副作用、可选拆分及教学权衡 |
| 提示词与历史 | 三个模块原文已关联；prompt/progress 原有内容保留 |
| 署名收尾代码与测试 | 仅更新五处源码署名注释；执行 make，结果见进度记录；未重跑 QEMU 或评分，未修改测试、脚本或 Makefile |
| 模块负责人 | 已确认：模块一母焯行（2513960）、模块二罗熙媛（2514039）、模块三谢北宸（2513513）；五处署名已更新 |
| Git 提交、推送、上传 | 尚未执行，不能登记为完成 |
| 引用与一致性 | 本轮文档检查核对本报告相对链接及其证据内容；结果追加于 progress.md |


## 13. 分工与源码署名收尾

小组已确认模块一母焯行（2513960）、模块二罗熙媛（2514039）、模块三谢北宸（2513513）。本次只更新上述五处源码注释及报告分工，历史提示词和验证日志保持原样，操作追加至 [progress.md](progress.md)。

本轮实际执行 `make`，退出码 0，见 [构建日志](../validation/signoff/make.log) 和 [退出码](../validation/signoff/make.exit)。[源码差异](../validation/signoff/source-comments.diff) 仅含五处署名替换；扫描 kern/ 未发现剩余 YOUR CODE。QEMU 和评分不重复执行，第 10 节仍引用此前真实记录。Git 提交、推送与正式上传尚未执行。
