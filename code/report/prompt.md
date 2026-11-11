
## 2026-11-11 Lab4 功能模块一：完整用户提示词

[PROMPT]

任务：
在 /home/student/oslab/lab4-work 中完成 Lab4 功能模块一：
虚拟内存页映射的建立、更新与撤销。

本轮允许修改的源码：
kern/mm/pmm.c

操作要求：
- 先读取真实源码、相关头文件和框架测试，再修改实际文件。
- 在 LAB4 function1 YOUR CODE 标记处补齐实现，保留该标记。
- 保留已完成的 page_init() 和其他现有逻辑。
- 不修改 get_pte()、框架检查函数、评分测试、注入程序和 Makefile。
- 不实现进程创建、调度或 switch_to。
- 不修改参考目录。
- 不提交或推送 Git。

输出要求：
- 说明实际实现及引用计数、TLB 的处理方式。
- 如实报告实际验证结果。
- 将本轮完整提示词追加到 report/prompt.md，
  将真实进度和问题追加到 report/progress.md，不覆盖历史记录。


[RELY]

当前工程已经完成 Lab2/3 前置适配。
物理页管理器公开符号为 default_pmm_manager，
实际实现是 Best-Fit。

需要读取：
- kern/mm/pmm.c、pmm.h、memlayout.h、mmu.h
- libs/error.h
- tools/test_lab4.c 中的页映射测试

现有接口及数据：
- get_pte(pgdir, la, create)：定位页表项；
  create 为真时可能分配中间级页表页，失败返回 NULL。
- get_page()：查找映射对应的物理页。
- pte_create()、page2ppn()、pte2page()：构造与解析页表项。
- page_ref_inc()、page_ref_dec()：返回更新后的引用计数。
- free_page()：释放一页物理内存。
- tlb_invalidate(pgdir, la)：使相应地址翻译失效。
- E_NO_MEM：内存不足错误码。
- PTE_V 及权限位的定义以当前头文件为准。

当前检查代码会覆盖：
- 建立映射。
- 将同一物理页映射到多个地址。
- 用另一物理页替换已有映射。
- 同一页重复插入并更新权限。
- 删除映射与重复删除。

注意：
本模块处理框架支持的普通页映射，
不能将实现描述为支持任意层级的大页拆分或所有分页模式。


[GUARANTEE]

必须实现并保持公开签名：

int page_insert(pde_t *pgdir, struct Page *page,
                uintptr_t la, uint32_t perm);

void page_remove(pde_t *pgdir, uintptr_t la);

允许在本模块新增必要的 static 辅助函数，
例如用于移除指定页表项的内部函数。
不得新增与本模块无关的公开接口。


[SPECIFICATION]

一、page_insert()

Pre-Condition：
- pgdir 为有效页表根的内核虚拟地址。
- page 为调用者提供的有效物理页描述符。
- 目标地址位于当前框架支持的页表管理范围。
- 已有映射及引用计数保持一致。

Post-Condition：
- 成功返回 0，目标虚拟页映射到 page。
- 权限按调用者要求及框架的有效位约定设置。
- 引用计数与最终映射关系一致。
- 相应 TLB 翻译失效。
- 不能直接覆盖旧映射而遗漏引用计数处理。

Case 1：目标地址没有有效映射
- 成功建立映射，目标物理页的引用计数增加一次。

Case 2：目标地址已经映射到同一物理页
- 更新权限，引用计数最终保持不变。
- 不得误释放该物理页。

Case 3：目标地址映射到其他物理页
- 正确撤销旧页的这一份引用。
- 旧页引用计数降到零时释放旧页。
- 建立新映射，新页引用计数增加一次。

Case 4：无法取得目标页表项
- 返回 -E_NO_MEM。
- 不增加目标物理页引用计数。
- 不安装目标叶子映射，不破坏原有有效映射。

Requirements：
- 检查真实 get_pte() 的失败行为。
- 如果其分配中间页表的过程中存在保留已分配页表页等情况，
  在结果中明确说明，不能声称已验证完整资源回滚。
- 不为通过测试而修改框架检查或测试代码。


二、page_remove()

Pre-Condition：
- pgdir 有效，地址位于当前框架支持的范围。

Post-Condition：
- 目标地址有有效映射时，该映射被撤销。
- 被映射物理页的引用计数减少一次。
- 引用计数降到零时释放物理页。
- 清除旧页表项，并使相应 TLB 翻译失效。

Case 1：映射存在
- 完成引用计数、物理页释放、页表项和 TLB 的一致性维护。

Case 2：页表项不存在或无有效映射
- 安静返回，不分配新的页表页。
- 不修改其他映射和物理页引用计数。
- 对同一未映射地址重复删除仍应安全。

Requirements：
- 不要求在本接口中自动回收所有空的中间级页表；
  保持与框架检查和已有页表生命周期一致。


三、验证要求

- 用真实工具链编译修改后的文件。
- 检查同页重复插入的引用计数不会变化。
- 检查不同页替换、重复删除和失败返回路径。
- 若没有实际运行测试，不得把代码审查写成动态测试通过。
- 如执行整工程 make，预期仍可能缺少 proc_init、
  cpu_idle 等后续接口；明确区分这些阶段性缺失和本轮错误。
- 不通过临时空桩、删减启动流程或修改测试来凑链接成功。
- 当前不要求执行完整 QEMU 或 make grade。

## 2026-11-11 Lab4 功能模块二：完整用户提示词

[PROMPT]

任务：
在 /home/student/oslab/lab4-work 中完成 Lab4 功能模块二：
进程创建及初始化。

本轮允许修改的源码：
kern/process/proc.c

操作要求：
- 先读取工程真实代码，再修改实际文件。
- 在 LAB4 function2 YOUR CODE 标记处实现本模块。
- 保留该标记，暂不填写负责人学号。
- 保留已有 get_pid、hash_proc、find_proc、setup_kstack、
  put_kstack、forkret、init_main、do_exit 等框架代码。
- 不实现 LAB4 function3 的调度功能。
- 不修改其他源码、评分脚本、测试、注入程序或 Makefile。
- 不修改已有 Lab2/3 参考目录。
- 不执行 Git 提交或推送。

输出要求：
- 说明初始化、资源分配、首次运行现场及失败清理的实现。
- 将本轮完整提示词追加到 report/prompt.md。
- 将实际验证和待完成事项追加到 report/progress.md。
- 不覆盖历史记录，不把静态检查写成动态测试通过。


[RELY]

当前状态：
- Lab2/3 前置代码已完成适配。
- page_insert、page_remove 已实现并编译通过。
- 线程创建与调度尚未实现，当前整工程仍不能完整运行。

必须读取：
- kern/process/proc.c、proc.h、entry.S、switch.S
- kern/trap/trap.h、trapentry.S
- kern/mm/pmm.h、memlayout.h、kmalloc.h、vmm.h
- kern/schedule/sched.c、sched.h
- kern/sync/sync.h
- libs/riscv.h、error.h、list.h
- tools/test_lab4.c 中的进程创建和调度测试

以真实声明为准使用：
- kmalloc、kfree、memset、memcpy
- setup_kstack、put_kstack
- get_pid、hash_proc、find_proc、set_proc_name
- wakeup_proc、list_init、list_add
- bootstack、boot_pgdir_pa
- forkret、forkrets、kernel_thread_entry
- current、idleproc、initproc、proc_list、
  hash_list、nr_process
- PROC_UNINIT、PROC_RUNNABLE
- CLONE_VM、SSTATUS_SPP、SSTATUS_SPIE、SSTATUS_SIE

当前实验仅创建内核线程：
- 它们共享内核页表。
- mm 表示用户地址空间，当前应为 NULL。
- 每个新建线程具有自己的内核栈。
- idleproc 复用已有启动栈和当前执行流。
- 当前框架 do_exit() 为 panic 占位，不在本轮扩展。


[GUARANTEE]

必须实现：

static struct proc_struct *alloc_proc(void);

void proc_init(void);

int kernel_thread(int (*fn)(void *),
                  void *arg, uint32_t clone_flags);

int do_fork(uint32_t clone_flags,
            uintptr_t stack, struct trapframe *tf);

static void copy_thread(struct proc_struct *proc,
                        uintptr_t esp, struct trapframe *tf);

static int copy_mm(uint32_t clone_flags,
                   struct proc_struct *proc);

允许新增必要的 static 辅助函数或前置声明。
保持公开接口签名，不添加调度功能或用户进程功能。


[SPECIFICATION]

一、alloc_proc()

Pre-Condition：
- 内核动态内存分配器已初始化。

Post-Condition：
- 成功返回已完成基础初始化的 PCB。
- state 为 PROC_UNINIT，pid 为 -1。
- runs、kstack、need_resched、flags 为零。
- parent、mm、tf 为 NULL。
- context 和 name 初始化为零。
- 链表节点初始化为合法状态。
- pgdir 初始化为 boot_pgdir_pa，明确记录共享的内核页表。
- 分配失败返回 NULL，不使用未初始化内存。


二、proc_init()

Pre-Condition：
- 物理内存、页表和动态内存分配器可用。
- 尚未开始正常线程调度。

Post-Condition：
- 初始化进程链表和 PID 哈希表。
- 将当前执行流登记为 idleproc：
  pid=0，state=PROC_RUNNABLE；
  使用 bootstack，need_resched=1；
  parent、mm、tf 为 NULL；
  runs=0，context 保持初始状态。
- current 指向 idleproc。
- 正确维护 nr_process。
- idleproc 不作为普通线程加入 proc_list，
  调度器后续可以单独用它保底。
- 通过 kernel_thread() 创建执行 init_main 的 initproc。
- 初始顺序下 initproc 的 PID 为 1，
  parent 为 idleproc，状态为 PROC_RUNNABLE。
- 为 init_main 传入框架预期的 "Hello world!!" 参数，
  将线程名设置为 "init"。
- 创建失败时明确报错，不继续解引用无效指针。


三、kernel_thread()

Pre-Condition：
- fn 是有效线程函数入口，current 已设置。

Post-Condition：
- 构造初始化完成的临时 trapframe。
- 按 kernel_thread_entry 的约定设置函数地址和参数。
- 首次执行位置为 kernel_thread_entry。
- status 设置为经 sret 进入 S 态，
  返回后允许相应中断，恢复过程中 SIE 保持关闭。
- 通过 do_fork() 创建线程并返回 PID 或负错误码。
- 内核线程使用共享地址空间语义。


四、copy_thread()

Pre-Condition：
- proc 已拥有合法内核栈。
- tf 指向有效的初始 Trap frame。

Post-Condition：
- 将 Trap frame 复制到新线程自己的内核栈高地址区域，
  不保留指向调用者临时栈变量的指针。
- 布局、边界和栈对齐符合当前 RV64 工程。
- 子线程保存的 a0 为 0。
- esp 为零时设置适合内核线程运行的栈指针；
  非零时按接口约定保存该值。
- context.ra 指向 forkret，
  context.sp 指向新线程的 Trap frame。
- 首次运行链路与
  forkret → forkrets → __trapret → sret
  保持一致。


五、copy_mm()

Pre-Condition：
- 当前阶段只支持内核线程，current->mm 应为 NULL。

Post-Condition：
- 保持 proc->mm 为 NULL，成功返回 0。
- 不创建或复制用户地址空间。
- 对不支持的用户地址空间情况进行明确检查，
  不静默假装已经完成复制。


六、do_fork()

Pre-Condition：
- current、进程管理结构和内存分配器可用。
- tf 为有效初始现场。

Post-Condition：
- 成功返回新线程 PID。
- 完成 PCB、父线程关联、内核栈、
  地址空间处理接口、初始现场和 PID 分配。
- 新线程正确加入 PID 哈希表和 proc_list。
- nr_process 正确增加一次。
- 完整初始化并登记后，通过 wakeup_proc()
  将其置为 PROC_RUNNABLE。
- PID 分配、登记、计数和发布为 runnable 的关键过程
  在适当的关中断临界区内完成，并恢复原中断状态。

Case：资源不足或达到线程数量上限
- 返回对应负错误码。
- 释放此前已经成功申请的资源。
- 不留下未完整初始化的可运行线程。
- 不遗留错误的链表、哈希登记或 nr_process 计数。


七、验证要求

- 使用真实工具链编译修改后的文件。
- 静态核对 PCB 字段、内核栈布局、context 偏移、
  status 位和首次运行链路。
- 核对失败清理路径以及框架进程创建测试的字段约定。
- 如执行整工程构建，准确报告剩余缺失接口。
- 当前调度模块尚未实现，不添加空桩来通过链接。
- 不声明线程已实际运行或 make grade 已通过。

## 2026-11-11 Lab4 功能模块三：完整用户提示词

[PROMPT]

任务：
在 /home/student/oslab/lab4-work 中完成 Lab4 功能模块三：
线程调度、运行切换和寄存器上下文切换。
完成后进行完整构建、评分和默认运行验证。

本轮允许修改的源码：
1. kern/process/proc.c：仅模块三实现位置及必要的声明。
2. kern/schedule/sched.c。
3. kern/process/switch.S。

操作要求：
- 先读取真实源码和测试，再修改实际文件。
- 保留 LAB4 function3 YOUR CODE 标记，暂不填写学号。
- 保留已完成的模块一、模块二及 Lab2/3 适配代码。
- 不修改评分脚本、注入程序、测试文件或 Makefile。
- 不添加用户进程、时间片抢占或完整进程退出回收功能。
- 不修改参考目录。
- 不执行 Git 提交或推送。

输出要求：
- 说明调度选择、页表切换、上下文保存恢复的实现。
- 保存真实构建、评分和默认 QEMU 运行记录及退出方式。
- 将完整提示词追加到 report/prompt.md。
- 将真实结果和问题追加到 report/progress.md。
- 不覆盖历史记录。


[RELY]

当前状态：
- 前置代码、模块一和模块二均已实现并完成编译检查。
- 线程初始化时 pgdir 已设置为 boot_pgdir_pa。
- idleproc 不作为普通线程加入 proc_list。
- 新线程已通过 context.ra、context.sp 和 Trap frame
  准备好首次运行链路。
- 当前整工程尚缺 cpu_idle 等模块三实现。

必须读取：
- kern/process/proc.c、proc.h、switch.S、entry.S
- kern/schedule/sched.c、sched.h
- kern/sync/sync.h
- kern/mm/pmm.h、memlayout.h
- kern/trap/trapentry.S
- libs/riscv.h、list.h
- Makefile
- tools/grade.sh、inject.py、test_lab4.c

关键接口与数据：
- current、idleproc、proc_list
- PROC_RUNNABLE、need_resched、runs
- local_intr_save、local_intr_restore
- list_next、le2proc
- write_csr、flush_tlb
- struct context：ra、sp、s0～s11，共 14 个 uintptr_t
- switch_to 的 a0 指向旧 context，a1 指向新 context

工程注意：
- 本工程是 RV64 Sv39。
- 核对 libs/riscv.h 中 lsatp() 的实际实现，
  不直接使用其中可能遗留的 Sv32 写法。
- pgdir 保存根页表物理地址，需要构造成正确 satp 值。
- 当前所有内核线程共享内核页表，
  但 proc_run 应使用目标线程的 pgdir 字段。
- 框架 do_exit() 当前执行 process exit!!. 的 panic；
  本轮不扩展它，也不删除它。
- 评分测试会创建额外线程，并通过主动调用 schedule()
  检查多个线程能否轮流运行。


[GUARANTEE]

必须实现：

void cpu_idle(void);

void schedule(void);

void proc_run(struct proc_struct *proc);

void switch_to(struct context *from, struct context *to);

保持已有接口及属性声明。
允许新增必要的模块内 static 辅助函数或常量，
不得新增无关公开接口。


[SPECIFICATION]

一、cpu_idle()

Pre-Condition：
- 进程系统已初始化，current 指向 idleproc。
- 启动流程已按框架完成中断和时钟初始化。

Post-Condition：
- 持续执行空闲循环，不返回。
- 检测到 current->need_resched 时调用 schedule()。
- 不自行修改其他线程的状态或实现新的调度策略。


二、schedule()

Pre-Condition：
- current、idleproc 和 proc_list 有效。

Post-Condition：
- 在适当的关中断临界区中执行调度选择。
- 清除当前线程的 need_resched 标记。
- 按链表循环顺序寻找下一个 PROC_RUNNABLE 线程。
- current 为 idleproc 时从普通线程链表头开始查找。
- 找不到可运行线程时，以 idleproc 保底。
- 根据框架约定更新被选线程的 runs。
- 目标不是 current 时调用 proc_run()。
- 恢复调用前的中断开关状态。

Case 1：存在其他 runnable 线程
- 按循环顺序选择，而非总从头选中同一线程。

Case 2：只有 current 自身仍可运行
- 允许重新选择 current，不执行无意义的上下文切换。
- 不应因为没有其他线程就强制放弃仍可运行的 current。

Case 3：没有 runnable 普通线程
- 选择 idleproc。

Requirements：
- 有限遍历，不能在链表上无限循环。
- 不把链表头当作 PCB。
- 不声称已实现定时器抢占式时间片 RR。


三、proc_run()

Pre-Condition：
- proc 指向合法可运行线程。
- 其 context 和 pgdir 已正确初始化。

Post-Condition：
- proc 与 current 相同时不进行上下文切换。
- 切换时保护关键区域，避免当前线程、页表、
  实际执行现场出现不一致。
- 保留旧线程指针，更新 current。
- 用目标线程 pgdir 构造 RV64 Sv39 的 satp，
  不截断地址或使用 Sv32 模式位。
- 完成必要的地址翻译同步。
- 调用 switch_to(&prev->context, &next->context)。
- 旧线程以后被切回时，能够继续执行调用后的代码，
  并恢复它原有的中断状态。

Requirements：
- 不把“switch_to 返回”解释成一定仍在原线程立即执行；
  要结合线程被切走、未来切回的过程说明。


四、switch_to()

Pre-Condition：
- from、to 为有效且不同的 context。
- context 布局与当前 RV64 结构体一致。
- 由上层保护切换临界区。

Post-Condition：
- 将旧线程 ra、sp、s0～s11 保存到 from。
- 从 to 恢复目标线程对应的 14 个寄存器。
- 通过恢复后的 ra 返回。
- 新线程首次运行时，进入预先设置的 forkret。
- 已运行线程被切回时，从其上次保存的位置继续。

Requirements：
- 明确设置代码段并导出 switch_to 符号。
- 偏移量按真实 struct context 核对，不能错位。
- 基址使用 from/to 指针，切换过程中不能提前破坏
  仍然需要的寻址寄存器。
- 本函数不选择线程、不修改进程链表、不决定调度策略。
- 说明 ra 在 ABI 中属于 caller-saved，
  此处保存它是为了恢复线程的继续执行位置。
- 不直接照搬 Trap 的 36 格保存恢复逻辑。


五、整体验证

执行前：
- 检查是否已有 QEMU 进程或调试端口占用。
- 不结束不属于本轮验证的进程。
- 各项验证顺序执行，避免 QEMU 或评分临时目录冲突。

1. 完整构建
- 清理后执行完整构建。
- 保存输出和真实退出码。

2. 评分验证
- 执行工程真实 make grade。
- 核对 check PMM、check Process Create、
  check Schedule 和总分，目标 30/30。
- 保存实际输出和退出码。
- 不修改测试或伪造 PASS 输出。

3. 默认运行验证
- 确保没有注入评分测试，运行默认 make qemu。
- 核对页管理、页表及 VMM 检查结果。
- 核对 initproc 的 PID、名称和 Hello world 等输出。
- 根据真实调用链识别 init_main 返回后
  process exit!!. 的框架 panic。
- 只能将这一条已确认的路径视为当前框架预期，
  不能将任意 panic、死循环或异常都视为成功。
- 不添加自动关机来改变框架行为。
- 如需结束默认 QEMU，正常使用其退出方式，
  记录为手动退出，不能称作自动关机。
- 若使用超时结束，只能记录为超时终止，
  不把超时退出码当作正常运行成功的证据。

4. 记录与范围
- 保存日志到 validation/，清楚区分构建、
  评分和默认运行记录。
- 准确说明静态检查与动态测试分别覆盖了什么。
- 如出现错误，先定位并报告。
  可以修正本轮三份源码中的问题；
  若涉及前轮实现或框架其他文件，先说明具体位置、
  原因和建议，不擅自扩展修改范围。

## 2026-11-11 Lab4 Challenge 分析与正式报告：完整用户提示词

[PROMPT]

任务：
依据 /home/student/oslab/lab4-work 的真实源码、提示词和验证日志，
完成 Lab4 的两项 Challenge 分析，并生成正式 Markdown 实验报告。

本轮只修改或新增文档，不修改任何源码、测试、脚本或 Makefile。
不提交或推送 Git，不重新运行已有且已通过的测试。

操作要求：
- 先读取当前代码、report/ 和 validation/。
- report/prompt.md、report/progress.md 已有内容必须保留。
- 正式报告写入 report/report.md。
- 若已有正式报告，应先读取再完善，不丢弃真实历史记录。
- 模块负责人的对应关系尚待确认，不能自行分配职责，
  不能替换源码 YOUR CODE。
- 本轮课程题目与报告要求见下文，不假设能访问 Mac 上的 PDF。

输出要求：
- 说明生成或更新了哪些文档。
- 列出两项 Challenge 是否覆盖全部子问题。
- 核对正式报告引用的源码、提示词和日志路径是否存在。
- 报告结论必须区分真实运行证据、代码分析和未验证内容。


[RELY]

当前已完成：
- Lab2/3 前置适配。
- 模块一：page_insert、page_remove。
- 模块二：alloc_proc、proc_init、kernel_thread、
  do_fork、copy_thread、copy_mm。
- 模块三：cpu_idle、schedule、proc_run、switch_to。

当前验证记录：
- 完整构建退出码 0。
- make grade 三项 OK，总分 30/30，退出码 0。
- 默认 QEMU 的页管理、页表、VMM 检查和 init 输出通过。
- 据本轮记录，默认运行沿框架
  do_exit → panic → kmonitor → sbi_shutdown 结束，
  退出码 0，未手动或超时终止，未新增关机代码。
- module3 验证日志位于 validation/module3。
- 首次评分受工具环境套接字权限限制失败的记录已保留。
- 正式撰写前须根据真实日志和源码核对以上表述。

已有记录：
report/prompt.md
report/progress.md
validation/ 下各验证记录

已确认的小组成员：
母焯行（2513960）
罗熙媛（2514039）
谢北宸（2513513）
三个模块的具体负责关系待确认。

重要工程事实：
- default_pmm_manager 是保留的公开符号，
  实际物理页分配算法为 Best-Fit。
- 当前只实现内核线程，共享内核页表。
- 当前 copy_mm 不复制用户地址空间。
- context 保存 ra、sp、s0～s11，共 14 个寄存器。
- 调度采用按链表循环寻找 runnable 线程的选择方式，
  不是完整的定时器抢占式时间片 RR。
- get_pte 分配失败可能保留已分配的中间页表；
  不得声称已经验证完整资源回滚。


[GUARANTEE]

必须交付：
report/report.md

正式报告必须覆盖：
1. 本章整体逻辑线。
2. 各核心函数及模块的理解。
3. 各练习 spec 与 prompt 的设计说明，并关联完整原文。
4. AI 辅助实现过程中的真实问题及解决过程。
5. 实验知识点与 OS 原理的对应、联系和差异。
6. 重要但未在本实验中实现的 OS 原理。
7. 实际构建、评分和默认运行结果。
8. 两项 Challenge 的全部子问题。
9. 小组成员、分工待确认信息及提交检查状态。

可追加本轮文档整理记录，
但不能篡改既有提示词、验证日志或历史结果。


[SPECIFICATION]

一、整体流程和实现说明

说明：
kern_init → pmm_init → 中断相关初始化 → vmm_init
→ proc_init → 时钟及中断开启 → cpu_idle
→ schedule → proc_run → switch_to。

实际顺序以当前 init.c 为准。

分别解释：
- page_insert、page_remove 的映射、引用计数和 TLB 一致性。
- alloc_proc 的初始字段。
- idleproc 如何复用启动执行流。
- do_fork 如何分配资源、登记线程、处理失败。
- kernel_thread、copy_thread 如何构造首次运行现场。
- copy_mm 在当前内核线程阶段的范围。
- schedule 如何循环选择以及何时重选自身或回退 idle。
- proc_run 如何切换 current 和目标页表。
- switch_to 如何保存与恢复 context。

完整说明新线程首次运行链路：
switch_to → forkret → forkrets → __trapret
→ sret → kernel_thread_entry → init_main。

区分：
- context 与 trapframe。
- ret 与 sret。
- 共享地址空间与独立内核栈。
- 轮转选择与定时器抢占式调度。


二、Challenge 1

题目：
说明 local_intr_save(intr_flag);
...
local_intr_restore(intr_flag);
如何实现开关中断。

回答必须包括：
- 根据真实 sync.h、intr.c 和 riscv.h 解释实现。
- sstatus.SIE 与独立 CSR sie 的区别。
- 保存原开关状态，原来开启才关闭，原来关闭保持关闭。
- 恢复时不能无条件开启。
- 解释 do { ... } while (0) 宏写法。
- 说明正确嵌套、逆序恢复的例子。
- 限定作用范围：当前 CPU、相应 S 态中断；
  不屏蔽同步异常，不阻止其他 CPU 并发访问，
  不保证屏蔽 M 态中断。
- 结合本实验 do_fork、schedule 或 proc_run 的临界区说明用途。


三、Challenge 2

题目一：
get_pte 中两段形式类似的代码，结合 Sv32、Sv39、Sv48，
解释为什么相似。

回答必须包括：
- 按真实 get_pte 说明两段代码分别处理哪个中间层。
- 非叶子项定位下一级页表，必要时分配、清零并安装页表项。
- 比较分页级数、普通页大小、每级索引位数和页表项大小：
  Sv32 两级、每级 10 位索引、4 字节 PTE；
  Sv39 三级、Sv48 四级，通常每级 9 位索引、8 字节 PTE；
  普通页均为 4 KiB。
- 区分虚拟地址有效位宽与实际寄存器宽度。
- 说明普通 4 KiB 映射路径下，相似中间层处理分别为
  一段、两段、三段，但实现也可通过循环抽象。
- 高层叶子项可表示大页，不能把所有有效项都当成非叶子。
- 本工程 get_pte 的支持范围以真实实现为准，
  不能宣称它已支持任意大页处理。

题目二：
get_pte 把查找和页表页分配合并，是否合理？是否需要拆开？

回答必须包括：
- create=0 与 create=1 的语义。
- 合并的便利与代码复用。
- 分配副作用、失败处理和资源管理复杂度。
- 拆成只查找接口和保证路径存在接口的可选设计。
- 给出适合当前教学工程的权衡结论。
- 这是分析题，不需要实际修改 get_pte。


四、验证证据和问题记录

- 从真实日志提取评分项、总分和退出方式。
- 默认运行与注入测试分开描述。
- 解释特定 process exit!!. panic 的调用链，
  不把任意 panic 都视为预期。
- 首次工具权限失败与代码错误区分。
- 既有警告、资源回滚范围等按真实情况说明，
  不夸大为全部路径均已通过。
- 不编造运行中发生过的 bug。
- 静态发现并修正的问题可以记录，
  但必须标为静态检查发现，不能写成运行报错。


五、提交状态

- 当前尚未确认模块负责人，分工处写“待确认”。
- 当前尚未执行 Git 提交或推送，不能写成已上传完成。
- 保留完整提示词记录，并使用相对链接关联。
- 只引用实际存在的日志和文件。
- 完成文档引用与内容一致性检查。
