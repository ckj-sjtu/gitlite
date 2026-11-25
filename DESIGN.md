# gitlite 设计文档

> SJTU CS1965-01 2026Fall Proj.3
> 本项目用 C++17 实现了一个 Git 子集：`init` / `add` / `commit` / `rm` / `log` / `global-log` / `tag` / `status` / `branch` / `rm-branch` / `checkout` / `reset` / `merge`，以及 Bonus-1 的远程功能（`add-remote` / `rm-remote` / `push` / `fetch` / `pull`）。

---

## 1. 代码结构

```text
.
├── main.cpp                  命令解析、通用校验、分发（评测接口，不可修改）
├── include/
│   ├── SomeObj.h             命令门面（公开签名即评测接口）
│   ├── Commit.h              提交对象（值语义）
│   ├── ObjectStore.h         对象库：按 SHA-1 存取
│   ├── Index.h               暂存区
│   ├── Refs.h                引用：HEAD、分支、标签
│   ├── Revision.h            revision 解析（标签 / 完整 id / id 前缀）
│   ├── IgnoreRules.h         .gitliteignore 解析与匹配
│   ├── WorkTree.h            工作目录只读视图
│   ├── Status.h              status 的五段数据与输出
│   ├── Checkout.h            工作区快照切换（含原子检查）
│   ├── Merge.h               三方合并
│   ├── Remote.h              远程登记与 push / fetch
│   ├── Repository.h          仓库元数据目录名
│   ├── Utils.h               骨架提供的文件 / SHA-1 工具
│   └── GitliteException.h
└── src/                      与 include 一一对应
```

分层：

```text
main.cpp
   │  参数数量 / 名称格式 / 是否在仓库中
   ▼
SomeObj（命令门面）
   │  业务校验 + 编排
   ▼
Commit / ObjectStore / Index / Refs / Revision / IgnoreRules
WorkTree / Status / Checkout / Merge / Remote
   │  只做读写与算法
   ▼
.gitlite/ 与工作目录
```

`main.cpp` 与 `SomeObj.h` 的公开签名是评测接口，未作修改；其余类可以自由设计。

---

## 2. 设计原则

### 2.1 每个命令是一个独立进程

`gitlite add a.txt` 与 `gitlite commit "x"` 是两个进程，内存不共享。因此：

- **所有状态必须落盘**，命令结束时内存即被销毁；
- 每个命令重新构造所需的对象，从磁盘读入、改完写回。

### 2.2 构造函数无副作用，I/O 显式化

所有门面类的构造函数只记录“我管哪块磁盘”，不做任何读写：

```cpp
Refs         refs{".gitlite"};            // HEAD 与 refs/
ObjectStore  store{".gitlite/objects"};   // 对象库
Index        idx{".gitlite/index"};       // 暂存区
WorkTree     wt{"."};                     // 工作区普通文件
Remote       rem{".gitlite"};             // 远程登记表
```

只有 `Index` 需要显式 `load()` / `save()`，因为它在内存中被多处修改后整体写回。这样设计的好处是：任何命令都可以安全地构造这些对象，不会因为“构造时的写操作”破坏已有数据。

### 2.3 检查先于写入，指针最后移动

涉及多个文件的命令（`checkout`、`reset`、`merge`）统一遵循：

```text
① 所有可能失败的检查（只读）
② 修改工作区（可重做）
③ 最后才移动 HEAD / 分支指针 / 清空暂存区（不可回退）
```

失败时前两步都不执行，保证工作区原封不动。

### 2.4 用有序容器保证确定性

提交快照、暂存区均使用 `std::map` / `std::set`：

- 序列化顺序确定 → 相同内容永远得到相同字节 → 相同 SHA-1；
- 初始提交因此能在所有仓库中得到同一个 ID。

若使用 `std::unordered_map`，迭代顺序不确定，同一份快照会算出不同 ID。

---

## 3. 持久化设计

### 3.1 目录布局

```text
.gitlite/
├── HEAD                        "ref: refs/branches/master\n"
├── index                       暂存区（文本，逐行）
├── objects/                    对象库：文件名 = 内容 SHA-1
│   ├── 2367c0e6…              初始提交
│   ├── 5c48b886…              普通提交
│   └── f572d396…              blob（文件内容原文）
├── refs/
│   ├── branches/
│   │   ├── master              "<40 位 sha>\n"
│   │   └── R1/                 远程跟踪分支（嵌套目录）
│   │       └── master          "<40 位 sha>\n"
│   └── tags/
│       └── v1.0                "<40 位 sha>\n"
└── remotes/
    └── R1                      "../D1/.gitlite\n"
```

所有路径都是**相对当前工作目录**的；命令必须在仓库根目录执行（由 `main.cpp` 的 `checkCWD()` 保证）。

### 3.2 commit 对象

文件名 = `sha1(下面这段字节)`，内容为：

```text
commit
parent <40 位 sha1>          ← 每个父一行；初始提交没有；合并提交两行
timestamp <Unix 秒>
message <提交信息>

<文件名> <blob 的 sha1>       ← 每行一个，按文件名升序（std::map）
<文件名> <blob 的 sha1>
```

要点：

- 头尾之间有一行**空行**，分隔“元数据”和“快照”；
- 快照是**全量**，不是增量——每次提交都列出当时所有被跟踪的文件；
- 初始提交严格为 `"commit\ntimestamp 0\nmessage initial commit\n\n"`（43 字节），因此 ID 恒为 `2367c0e645007eb8cf7e8402178ff127f13bc623`；
- 提交对象不可变，修改任何内容都会得到新的 ID。

反序列化时按行解析：先读元数据直到空行，再把剩余行按“末尾 40 位是 sha、其余是文件名”切开。

### 3.3 index（暂存区）

纯文本，每行一条，两种前缀：

```text
add <文件名> <blob 的 sha1>
remove <文件名>
```

- `add` 行在前、`remove` 行在后，各自按文件名升序；
- 解析时**从行尾取 40 位 sha**，前面的部分即文件名——这样文件名里带空格也不会切错；
- 空暂存区 = 0 字节文件。

### 3.4 引用

| 文件 | 内容 |
|---|---|
| `HEAD` | `ref: refs/branches/<当前分支>` |
| `refs/branches/<name>` | `<40 位 sha>\n` |
| `refs/tags/<name>` | `<40 位 sha>\n` |
| `refs/branches/<remote>/<branch>` | `<40 位 sha>\n`（远程跟踪分支） |

远程跟踪分支的名字里带 `/`，在磁盘上自然成为嵌套目录。写入时 `Utils::writeContents` 会自动创建父目录。

### 3.5 远程登记表

```text
remotes/<远程名>    →    "<路径>\n"
```

路径**原样保存**（可能是相对路径），只在 `push` / `fetch` / `pull` 时才解析；`add-remote` 不检查路径是否存在。

### 3.6 对象库

- **blob**：文件内容的原始字节，文件名 = `sha1(内容)`；
- **commit**：见 3.2，文件名 = `sha1(序列化字节)`；
- 两者混放在 `objects/` 下，通过**内容首行是否为 `commit`** 区分类型；
- 写入采用“已存在则跳过”，天然去重。

---

## 4. 各类设计

### 4.1 `Commit`

**职责**：表示一次提交，并提供序列化 / 反序列化 / 日志渲染。

**成员**：

| 成员 | 作用 |
|---|---|
| `std::vector<std::string> parents` | 父提交 id，0 / 1 / 2 个 |
| `long long timestamp` | Unix 秒 |
| `std::string message` | 提交信息 |
| `std::map<std::string,std::string> snapshot` | 文件名 → blob sha1，全量快照 |

**要点**：

- **值语义**：`next.snapshot = parent.snapshot` 是深拷贝，因此修改新提交不会影响父提交，天然保证历史不可变；
- `serialize()` 的字节即 ID 的输入，必须严格确定；
- `logEntry(id)` 负责单条日志的格式（`===` / `commit` / 可选 `Merge:` / `Date:` / 消息 / 空行），被 `log` 与 `global-log` 共用；
- 时间用 `std::localtime` + `strftime("%a %b %d %H:%M:%S %Y %z")` 渲染，`%d` 自带补零。

### 4.2 `ObjectStore`

**职责**：按 SHA-1 存取对象。

| 方法 | 作用 |
|---|---|
| `writeBlob(content)` | 算 sha1、写入 `objects/<sha>`（已存在则跳过）、返回 sha |
| `writeCommit(serialized)` | 同上 |
| `readObject(sha)` | 读出对象原始字节 |
| `listObjects()` | 列出全部对象名（已排序） |
| `importFrom(src)` | 把另一个对象库的全部对象复制过来，已存在则跳过 |

`readObject` 不校验 sha 是否为提交，调用方负责判断类型（见 `Revision`）。

### 4.3 `Index`

**职责**：内存中的暂存区模型 + 持久化。

| 成员 | 作用 |
|---|---|
| `std::map<std::string,std::string> adds_` | 待添加：文件名 → blob sha |
| `std::set<std::string> removes_` | 待删除的文件名 |

**不变式**：同一个路径不可能同时在两张表里，由 `setAdd` / `setRemove` / `erase` 共同维护。

关键方法：

- `load()`：先清空内存，再从磁盘重建（幂等）；
- `save()`：把两张表整体拼成文本覆盖写盘；
- `setAdd(f, blob)`：写入并取消待删除；
- `setRemove(f)`：标记删除并取消待添加；
- `erase(f)`：两边都删（用于“内容回到 HEAD”时清空一切）；

暂存区只记录**相对 HEAD 的差异**，不保存完整快照。

### 4.4 `Refs`

**职责**：引用层——HEAD、本地分支、标签，以及远程跟踪分支。

| 方法 | 作用 |
|---|---|
| `currentBranch()` | 读 HEAD，剥前缀得到分支名 |
| `headCommit()` | 当前分支指向的提交 |
| `setHeadBranch(name)` | 切换当前分支（改 HEAD 一行） |
| `branchExists` / `getBranch` / `setBranch` / `deleteBranch` | 分支文件读写删 |
| `tagExists` / `getTag` / `setTag` / `listTags` | 标签文件读写 |
| `listBranches()` | **递归**列出所有分支名（含 `R1/master`） |

`listBranches` 之所以需要自己递归，是因为骨架的 `plainFilenamesIn` 只列一层且只收普通文件，会把 `R1/` 这样的目录整个跳过。实现上用一个匿名命名空间的 `collectBranches`：遇到文件收名字、遇到目录带前缀递归，最后统一排序。

### 4.5 `Revision`

**职责**：把用户写的 revision 翻译成唯一的 40 位提交 id。

解析规则（与规范一致）：

```text
① 先精确匹配标签名        → 命中即返回
② 再按提交 ID 前缀匹配     → 多个命中取字典序最小
③ 都不行                  → 返回 ""（调用方报错）
```

关键点：`objects/` 里 blob 和 commit 混放，因此前缀命中后必须**读内容确认以 `commit\n` 开头**，否则可能把一个 blob 当成提交。

### 4.6 `IgnoreRules`

**职责**：解析 `.gitliteignore` 并判断文件名是否被忽略。

构造时把每一行“编译”成 `Rule { bool negated; std::string pattern; }`，按文件顺序存入 `vector`（顺序有意义）。

支持的语法：

| 写法 | 含义 |
|---|---|
| 空行 | 忽略 |
| `#...` | 注释 |
| `!pat` | 反选 |
| `\x` | 转义，`x` 按字面匹配 |
| `/pat` | 锚定根目录（本项目扁平，等价于 `pat`） |
| `*` / `?` | 任意长度 / 任意单字符 |

`isIgnored` 遍历**全部**规则并不断覆盖结果——因为“最后一条匹配的规则生效”。

匹配由 `matchRec` 递归实现：双指针 + 对 `*` 回溯，要求模式与名字同时走到结尾（完全匹配，不是前缀匹配）。

### 4.7 `WorkTree`

**职责**：工作目录的只读视图。

| 方法 | 作用 |
|---|---|
| `listFiles()` | 列出根目录普通文件（已排序） |
| `exists(f)` | 是否为普通文件 |
| `sha1Of(f)` | 内容哈希 |

没有任何写方法——`status` / `checkout` 需要读工作区，而“改工作区”由 `Checkout` 显式完成。

### 4.8 `Status`

**职责**：计算并输出 `status` 的五段内容。

构造时一次性算好五个 `vector`，`print()` 只负责格式化输出。

五段的判定：

| 段 | 判定 |
|---|---|
| Branches | `listBranches()`，当前分支前加 `*` |
| Staged Files | `idx.adds()` 的键 |
| Removed Files | `idx.removes()` |
| Modifications Not Staged | 对 `HEAD ∪ adds` 的每条路径：已标删则跳过；基准 = 暂存版本（若有）否则 HEAD 版本；文件不存在 → `(deleted)`，哈希不同 → `(modified)` |
| Untracked Files | 工作区文件里排除已暂存、已跟踪的；被 ignore 的也排除，但“删后重生”的文件例外 |

“删后重生”的例外对应规范：由当前提交跟踪、已暂存待删除、随后又在工作区重建的文件，仍算未跟踪，即使匹配 ignore 规则也必须显示。

### 4.9 `Checkout`

**职责**：把工作区同步成某个提交的样子，并被 `checkout` / `reset` / `merge`（快进）复用。

| 方法 | 作用 |
|---|---|
| `toSnapshot(target)` | 整份切换：检查挡路 → 删除 `cur − target` → 写入 `target` |
| `fileTo(commit, f)` | 用某个提交里的版本覆盖单个文件 |
| `untrackedInTheWay(cur, target)` | 私有检查：目标路径上是否有版本库不认识的未跟踪文件 |

“挡路”的判定：目标快照里的路径，若工作区存在、当前提交不跟踪、未暂存、未被忽略，则拒绝覆盖。

`toSnapshot` **不改暂存区、不改指针**——这些由调用方在成功之后完成，从而保证“失败不留下半成品”。

### 4.10 `Merge`

**职责**：把给定分支合进当前分支。

三种结局：

```text
分割点 == 给定头  → "Given branch is an ancestor..."（不动工作区）
分割点 == 当前头  → 快进：整份检出 + 移指针
否则             → 三方合并 → 可能冲突 → 双父提交
```

核心是三态比较：对每条路径取 `base` / `cur` / `giv` 三个版本，用两个布尔量决定动作：

| `curChanged` | `givChanged` | `c == g` | 动作 |
|---|---|---|---|
| — | false | — | 保持当前 |
| false | true | — | 取给定（有则写，无则删） |
| true | true | true | 保持当前（两边同改） |
| true | true | false | 冲突 |

这一张表覆盖了规范里的八条文件规则。

### 4.11 `Remote`

**职责**：远程登记表 + `push` / `fetch` 两个跨仓库操作。

| 方法 | 作用 |
|---|---|
| `path(name)` | 读出登记的路径（未登记返回 `""`） |
| `exists(name)` | 名字是否登记过 |
| `add` / `remove` | 写 / 删 `remotes/<name>` |
| `resolve(name, &dir)` | 名字 → 可用目录；失败时打印 `Remote directory not found.` |
| `push(name, branch)` | 本地 HEAD → 远程分支 |
| `fetch(name, branch)` | 远程分支 → 本地 `<name>/<branch>` |

`push` 的两条关键规则：

1. 若远程分支已存在，其头提交**必须**是本地 HEAD 的祖先，否则拒绝（`Please pull down remote changes before pushing.`）；
2. 先复制对象、再移动指针。

`fetch` 则先搬对象、再创建或更新远程跟踪分支，不改 HEAD、不改工作区、不动暂存区。

---

## 5. 关键算法

### 5.1 合并分割点（merge base）

```text
① 从当前分支头出发，沿"全部父引用"收集所有祖先（含自身）→ 集合 A
② 从给定分支头开始 BFS（含自身），入队顺序 = parents 的存储顺序
③ BFS 中第一个落在 A 里的提交即为分割点
```

第一段只求集合，用 DFS 即可；第二段用 BFS 保证“最近”，且“按 parents 顺序入队 + 第一个命中即返回”让 criss-cross 图也有确定结果。

### 5.2 三方合并的逐路径判定

见 4.10 的表格。实现上：

```cpp
const bool curChanged = !same(c, b);
const bool givChanged = !same(g, b);
if (!givChanged) continue;          // 保持当前
if (!curChanged) { 取给定 }
if (same(c, g)) continue;           // 两边同改
// 否则冲突
```

`same()` 把“两边都不存在”也视为相同，因此“两边都删除”自然落到“无动作”。

### 5.3 冲突文本

```text
<<<<<<< HEAD
<当前分支内容>
=======
<给定分支内容>
>>>>>>>
```

- 三条标记行各占一行；
- 某一侧被删除时该侧内容为空；
- 若某侧内容不以换行结尾，先补一个换行；
- 整个文件以 `>>>>>>>` 后的换行结束。

冲突文件会被写入工作区、存入对象库，并进入合并提交（本项目不做逐行合并）。

### 5.4 revision 解析

见 4.5。三个要点：标签优先、前缀取字典序最小、必须校验对象类型是提交。

### 5.5 ignore 通配符匹配

递归双指针：`*` 处枚举“吃掉 0~n 个字符”，任一分支成功即返回；结束时要求模式和名字同时耗尽。

---

## 6. 错误处理与顺序

所有错误信息严格照规范输出，**不以句号以外的任何字符结尾**，且不打印任何多余内容。

检查顺序在两个层面固定：

**全局（`main.cpp`）**：无参数 → 未知命令 → 是否在仓库中 → 操作数数量 / 格式。

**命令内部**（示例）：

| 命令 | 顺序 |
|---|---|
| `commit` | 空消息 → 暂存区为空 |
| `rm-branch` | 分支不存在 → 是当前分支 |
| `checkout <branch>` | 分支不存在 → 是当前分支 → 有未跟踪文件挡路 |
| `checkout <rev> -- f` | revision 无法解析 → 该提交里没有此文件 |
| `merge` | 有暂存改动 → 分支不存在 → 与自己合并 → 挡路 |
| `push` | 远程目录不存在 → 远程头不是本地祖先 |
| `fetch` | 远程目录不存在 → 远程没有该分支 |

失败时一律“打印错误后立即返回”，不修改任何状态。

---

## 7. 测试

本地测试集与 ACMOJ 一致（77 个测试点），在 `testing/` 下运行：

```bash
cd testing
python3 tester.py samples/*.in            # 全部
python3 tester.py samples/3-status-10.in  # 单个（自动跑前置）
```

实现过程中按 Subtask 顺序推进，每完成一项立刻跑对应组：

```bash
python3 tester.py samples/1-*.in
python3 tester.py samples/2-*.in
...
```

当前结果：

```text
Basic（Subtask 1–5 + Complement）: 全通过
Bonus-1（remote）               : 全通过
Bonus-2（diff / show）          : 未实现
```

---

## 8. 已知限制

1. **工作区扁平**：只处理根目录普通文件，不递归子目录（与规范一致）；
2. **不回收不可达对象**：`reset`、`rm-branch` 后失去引用的提交仍留在 `objects/`，本项目不实现 GC；
3. **不做逐行合并**：冲突时整文件替换为带标记的版本；
4. **未实现 diff / show**（Bonus-2），因此 `revision` 解析目前只服务于 checkout / reset / tag / merge；
5. **标签为轻量标签**：只有“名字 → 提交”的映射，不包含作者、时间与说明。
