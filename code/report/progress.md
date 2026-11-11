
## 2026-11-11 Lab4 功能模块一

已先完整读取 pmm.c、pmm.h、memlayout.h、mmu.h、libs/error.h 和 tools/test_lab4.c。
仅在 pmm.c 的 LAB4 function1 YOUR CODE 区域新增 page_insert、page_remove 和 static page_remove_pte。
保留标记、page_init、get_pte、所有框架检查和原 tlb_invalidate；未修改其他源码、测试、注入程序、Makefile 或参考目录；未执行 Git 提交或推送。

实现：取得 PTE 成功后才增加目标页引用；同页重复插入抵消新增引用，仅更新权限；不同页替换撤销旧引用，计数归零释放；安装时使用 pte_create，按现有约定自动带 PTE_V。移除时先清 PTE 并调用 tlb_invalidate，再减少引用，归零释放。不存在的映射安静返回，get_pte(create=0) 不分配页表。中间级页表不自动回收。

实际验证：
- make obj/kern/mm/pmm.o：成功，使用工程 riscv64-unknown-elf-gcc。
- 对修改前 /tmp/lab4-module1-before.c 的文本比较：标记前和 tlb_invalidate 起至文件末尾逐字未变，改动限定在指定区域。
- nm 检查：目标文件导出 page_insert 和 page_remove，内部辅助函数为 static（可能被优化内联）。
- 源码路径审查（非动态测试）：同页引用先加后减不释放；不同页通过辅助函数撤销旧引用；重复删除遇到无效 PTE 不更改引用；get_pte 返回 NULL 时在引用增加和叶子 PTE 写入之前返回 -E_NO_MEM。
- make -j2：链接失败，仅报告缺少 proc_init、cpu_idle；此前 page_insert/page_remove 缺失已消除。未添加空桩或删减启动流程。

已知限制：真实 get_pte 在已分配并挂接第一张中间页表后，若下一张页表分配失败，会保留前一张页表及其引用。本轮不修改 get_pte，不声称完整资源回滚。
未运行 tools/test_lab4.c 的动态测试、QEMU 或 make grade；编译及源码审查不等同于运行测试通过。功能范围仅限当前框架支持的普通页映射，不提供大页拆分或任意分页模式支持。后续进程创建与调度模块仍待实现。

## 2026-11-11 Lab4 功能模块二

已读取 proc.c/proc.h、entry.S/switch.S、trap.h/trapentry.S、pmm.h/memlayout.h/kmalloc.h/vmm.h、sched.c/sched.h、sync.h、riscv.h/error.h/list.h 及框架进程创建、调度测试。
仅在 proc.c 的 LAB4 function2 YOUR CODE 标记后新增六个指定函数；保留标记且未填写学号。框架原函数和 LAB4 function3 区域均未修改，其他源码、测试、Makefile 和参考目录未修改。未提交或推送 Git。

实现：
- alloc_proc 分配并清零 PCB，设 UNINIT、pid=-1、共享 boot_pgdir_pa，自初始化两个链表节点。
- proc_init 初始化进程列表和哈希表；idle 使用 bootstack 和当前执行流，计入总数但不登记到普通进程链表和哈希表；创建 PID 1 的 init，父线程为 idle，参数为 Hello world!!。失败明确 panic。沿用 set_proc_name；为其固定长度复制提供补零名称数组。
- kernel_thread 清零临时 tf，s0/s1 保存入口及参数，epc 指向 kernel_thread_entry；status 继承当前值后置 SPP/SPIE、清 SIE；携 CLONE_VM 调 do_fork。
- copy_thread 将 tf 复制到独立内核栈顶部以下 288 字节处（16 字节对齐），a0=0；esp=0 时恢复 sp 为内核栈顶，否则保存调用者 esp；context.ra=forkret，context.sp=tf。静态链路核对为 forkret -> forkrets -> __trapret -> sret -> kernel_thread_entry -> fn；fn 返回后仍进入原有 do_exit panic。
- copy_mm 对 current->mm 非 NULL 返回 -E_INVAL，不伪装支持用户地址空间。
- do_fork 依次分配 PCB 和内核栈，处理地址空间及现场；在关中断区重新检查上限，再分配 PID、登记哈希与列表、计数加一并 wakeup，最后恢复中断状态。
- 静态失败路径审查：初始上限失败无分配；PCB 失败返回 -E_NO_MEM；栈失败释放 PCB；copy_mm 或发布前上限失败释放栈和 PCB。发布前未登记，因而不需撤销哈希/列表/计数；临界区上限失败先恢复中断再清理。

实际验证：
- make obj/kern/process/proc.o：真实 riscv64-unknown-elf-gcc 编译成功。
- 临时布局检查 /tmp/lab4-module2-layout.c 以真实工具链 -Wall -Werror 编译成功：RV64、context 大小及 ra/sp/s0/s11 偏移、trapframe=288 字节、内核栈容量及 16 字节对齐、SPP/SPIE 与 SIE 位互不重叠。
- 文本比较 /tmp/lab4-module2-before.c：function2 标记前所有框架代码及 function3 起至文件尾均逐字未变。
- 对照 tools/test_lab4.c 静态核对 idle/init 的字段约定；没有动态运行该测试。
- make -j2：链接失败，仅剩 cpu_idle 未定义；proc_init 缺失已消除。没有使用空桩或删减启动流程。

待完成：模块三调度、proc_run/cpu_idle/switch_to 等仍保持原状。switch.S 当前仅有 function3 标记，无法据此声称实际切换已验证。未运行 QEMU、动态线程创建/失败注入/调度测试或 make grade，不声明线程已实际运行。do_exit 仍是框架 panic 占位。

## 2026-11-11 Lab4 功能模块三及整体验证

源码变更限定为 kern/process/proc.c 模块三、kern/schedule/sched.c、kern/process/switch.S。三个 LAB4 function3 YOUR CODE 标记保留，未填学号。已读取本轮指定真实源码、接口及评分脚本、注入逻辑和测试；未修改其他源码、Makefile、测试或参考目录；未提交或推送 Git。

实现：
- cpu_idle 持续检查 need_resched，必要时调用 schedule。
- schedule 保存中断状态并关中断，清当前 need_resched，从当前普通线程后继开始循环查找（idle 则从普通列表头开始）。跳过哨兵；一圈内无其他候选时仍可选自身，全部不可运行时使用 idle。选中者 runs 加一，仅不同线程调用 proc_run，返回后恢复原中断状态。这是主动让出 CPU 的循环调度，未添加定时器抢占。
- proc_run 同线程直接返回，否则关中断，保留 prev 并更新 current；从目标 pgdir 构造 ((uintptr_t)SATP_MODE_SV39 << 60) | (pgdir >> 12)，写 satp 并 flush_tlb，使用编译器内存屏障保护同步边界。未调用 riscv.h 内以 unsigned int/SATP32_MODE 实现的旧 lsatp。switch_to 返回后的代码，是旧线程未来被再次选中时在原内核栈上继续执行；嵌套临界区分别恢复各自保存的中断状态。
- switch_to 的 a0/a1 始终用作 from/to 基址，按 0、8、…、104 字节保存/恢复 ra、sp、s0～s11，然后 ret。ra 虽为 ABI caller-saved，此处保存它以恢复线程继续执行位置；首次 ret 进入 forkret。未保存 Trap 的 36 个槽位，未在汇编中选择线程。

验证日志均位于 validation/module3/，未覆盖既有 report 历史：
1. preflight.log：验证前未发现已有 QEMU/评分进程；ss 受沙箱 netlink 限制，改读 /proc/net/tcp、tcp6 核对 1234 监听。没有结束其他任务进程。各验证顺序执行。
2. clean.log/clean.exit：make clean 退出 0；build.log/build.exit：随后 make -j2 完整构建退出 0。保留原 kmalloc.c 的整数转指针警告，本轮未扩展修改范围。
3. grade.log/grade.exit：首次真实 make grade 在沙箱内退出 2，0/30，三项均 no $qemu_out。gdb-port-probe.log 记录 QEMU 无法建立 GDB socket，Operation not permitted；端口探测在启动失败后退出 1，不是测试超时。
4. 申请并获准沙箱外执行同一真实 make grade，grade-retry.log/grade-retry.exit 记录退出 0；check PMM、check Process Create、check Schedule 全部 OK，Total Score: 30/30。grade-retry-qemu.log 为评分临时树串口输出的实时副本，包含三条测试 PASS 及 init 输出。评分脚本本身负责 QEMU/GDB 生命周期，未修改脚本，未伪造输出。
5. 评分清理了本地 obj/bin，随后重新 make -j2，default-build.log/default-build.exit 退出 0。检查本地没有 kern/test 或 test_lab4.h 注入，不使用 DEBUG_GRADE 构建。默认 make qemu 的 default-qemu.log/default-qemu.exit（script 记录）和复核 default-qemu-pty.log/default-qemu-pty.exit（独立 PTY，保持输入端打开）均记录退出 0。
6. 两次默认运行均实际输出 check_alloc_page、check_pgdir、check_boot_pgdir、kmalloc_init、check_vma_struct、check_vmm 成功；输出 initproc pid=1/name=init，Hello world!! 和 Bye 文本。随后在未修改的 proc.c:208 do_exit 中触发 process exit!!.。真实调用链为 kernel_thread_entry 调 init_main，返回后 jal do_exit -> panic -> kmonitor。进一步读取 kern/debug/panic.c、kmonitor.c 确认已有 kmonitor 在打印欢迎语后无条件 sbi_shutdown()，因此默认 QEMU 自行退出，并非等待人工退出。第二次 PTY 复核证明退出不是终端输入 EOF 导致。未发送 Ctrl-A X、未超时或信号终止默认运行；也未新增关机代码。default-exit-explanation.txt 记录此退出方式。仅将这一条已核对的框架 panic 路径识别为预期，未把任意 panic 视为成功。
7. layout-check.log：真实 RV64 编译器 -Wall -Werror 静态断言 context 共 112 字节、全部 14 个成员偏移检查通过。switch-disassembly.log 保存真实目标文件反汇编，与字段布局吻合。
8. scope-check.log：对本轮前源码/受保护文件哈希清单核对，仅三份获准源码改变；proc.c 原有内容完整保留。最终检查 1234 无监听。本轮动态评分覆盖基础映射、创建字段、额外线程主动轮流运行；单线程重选、空列表保底、关中断恢复分支另经静态审查，未声称这些均有独立动态测试。

范围与后续：未实现用户进程、时间片抢占或完整退出回收。do_exit 仍是框架 panic，kmonitor 的既有关机调用保持原样；若以后需要交互调试，应单独授权讨论该框架行为，本轮不改。此前 get_pte 部分分配失败不完整回滚等记录仍然有效，30/30 不表示所有资源耗尽或分页模式都已验证。

## 2026-11-11 Challenge 分析与正式 Markdown 报告整理

本轮只整理文档，未重新运行构建、评分、QEMU 或已通过测试，未执行 Git 提交或推送。开始时 report/report.md 不存在，已新增正式报告；report/prompt.md 追加本轮完整提示词，原三个模块提示词逐字保留；本文件仅追加记录。

正式报告覆盖整体启动依赖、三个模块及核心函数、首次运行链路、spec/prompt 设计、真实问题过程、OS 原理联系与未实现范围、运行证据、小组及提交状态。成员姓名学号按已确认信息记录，所有本轮模块负责人均写待确认，未修改源码 YOUR CODE。

Challenge 覆盖核对：
- Challenge 1：真实 sync.h/intr.c/riscv.h 调用、sstatus.SIE 与 sie、原状态保存和条件恢复、do/while 宏、独立变量逆序嵌套、当前 hart/S 态中断边界、同步异常/M 态/多核限制及本实验临界区用途全部覆盖。
- Challenge 2 题目一：真实 L2/L1 两段处理、分配清零安装、Sv32/Sv39/Sv48 级数/索引/PTE/页大小/有效地址位宽比较、一/二/三段与循环抽象、高层叶子及当前不支持任意大页处理的边界全部覆盖。
- Challenge 2 题目二：create=0/1、复用优点、副作用及失败资源、查找与保证路径接口拆分方案、当前教学工程权衡全部覆盖，仅分析不改实现。

证据核对：读取真实 build/grade/default 日志及各 .exit；首次评分退出2/0分与 socket Operation not permitted 记录保留，重试退出0/30分；默认独立 PTY 日志包含所有关键检查和 init 输出，结合 proc.c/entry.S/panic.c/kmonitor.c/libs/sbi.h 核实原有框架关机路径。报告将运行输出与调用链分析分开，不将退出码0或任意 panic 当作充分成功依据；未验证项单列。kmalloc.c:92 原警告保留。已有提示词归档从模块一开始，不伪称前置适配完整提示词已在文件中归档。

文档检查实际结果：report/report.md 的 57 处 Markdown 相对链接均指向存在的源码、提示词、进度或日志文件；核对三项评分 OK、Total Score: 30/30 和默认输出与报告一致。以本轮开始的哈希/长度清单核对，源码、测试、脚本、Makefile、validation 原日志均未改；prompt 原内容保持原字节前缀。追加本记录后再次检查 progress 原字节前缀及文档链接。上述为文档/静态一致性检查，不是重新执行实验测试。

待确认事项：三个模块负责人、后续正式提交/推送或上传。当前报告不宣称这些已完成。

## 2026-11-11 分工确认与源码署名收尾

用户已明确确认：模块一母焯行（2513960），模块二罗熙媛（2514039），模块三谢北宸（2513513）。更新 report/report.md 的成员职责表、署名说明及提交检查状态，并新增署名收尾记录；此前“待确认”的历史提示词和本进度历史均保留，不回写历史。

实际读取并更新五处源码注释：
- kern/mm/pmm.c:285：LAB4 function1 2513960。
- kern/process/proc.c:221：LAB4 function2 2514039。
- kern/process/proc.c:362：LAB4 function3 2513513。
- kern/schedule/sched.c:13：LAB4 function3 2513513。
- kern/process/switch.S:1：LAB4 function3 2513513。

验证：对比本轮修改前的完整源码，确认四份源码的变化严格等于上述五处注释中 YOUR CODE 替换为学号，函数、算法及其他内容逐字未变。差异保存于 validation/signoff/source-comments.diff。扫描 kern/ 中 YOUR CODE，剩余0处。

实际执行 make，退出码0；日志 validation/signoff/make.log，退出码 validation/signoff/make.exit。该次为署名变更后的增量构建，不称为重新清理全量构建，也未重复执行 QEMU 或 make grade。既有评分及默认运行记录保持原样。

重新核对 report/report.md 共61处 Markdown 相对链接，均指向实际存在的文件。哈希核对确认 report/prompt.md、既有 validation 日志、测试、脚本、Makefile 和其他源码未改变。本文件只追加，历史内容保留。

尚未完成的提交事项：Git 提交、Git 推送及正式上传均尚未执行；本轮没有执行这些操作。三个模块负责人现已确认，不再作为待确认事项。
