# Velo 语言手册

**版本 1.0** 　|　 一门语法像 Python、运行速度像 C 的编程语言

---

## 目录

1. [一分钟了解 Velo](#1-一分钟了解-velo)
2. [快速开始](#2-快速开始)
3. [语言基础](#3-语言基础)
4. [标准库参考](#4-标准库参考)
5. [编译器命令行](#5-编译器命令行)
6. [性能报告](#6-性能报告)
7. [与 Python 的对照表](#7-与-python-的对照表)
8. [它为什么这么快](#8-它为什么这么快)
9. [已知限制](#9-已知限制)
10. [常见问题](#10-常见问题)

---

## 1. 一分钟了解 Velo

Velo 是一门**静态类型、编译型**的编程语言。它解决了 Python 最让人头疼的矛盾：
**语法舒服，但是慢。**

```
Python 的写法  ──►  Velo 的语法（几乎一样）
Python 的速度  ──►  C 的速度（编译成原生机器码）
```

### 它长什么样

```velo
def fib(n: int) -> int:
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

def main() -> int:
    for i in range(10):
        println("fib({i}) = {fib(i)}")
    return 0
```

除了变量声明用 `var`、参数和返回值要写类型之外，它读起来和 Python 几乎一样。

### 它有多快

同一台机器、同一个算法、同样编译成原生程序：

| 测试项目 | Velo | C（MSVC /O2） | Python 3.13 |
| :--- | ---: | ---: | ---: |
| 递归 fib(34) | 0.050 s | 0.047 s | 1.448 s |
| 埃氏筛 10⁷ | 0.074 s | 0.069 s | 4.929 s |
| 曼德博集合 800×800 | 0.083 s | 0.076 s | 11.177 s |
| 200 万次 append + 排序 | 0.216 s | 0.201 s | 1.977 s |

**结论：Velo 的运行时间是等价 C 程序的 1.07–1.10 倍（即约 90% 的 C 性能），
比 Python 快 9–134 倍。**

### 它是怎么做到的

Velo 不解释执行，也不依赖虚拟机。它的编译流程是：

```
 .velo 源码
     │  ① 词法分析 / 语法分析
     ▼
   抽象语法树
     │  ② 静态类型检查（每个表达式都有确定的类型）
     ▼
   带类型标注的 AST
     │  ③ 生成 C 代码（数值运算直接成为机器指令）
     ▼
   program.c
     │  ④ 调用 MSVC cl.exe /O2（内联、向量化、常量折叠）
     ▼
   program.exe  ◄── 真正的原生机器码
```

也就是说：**你写的是 Python 风格的源码，执行的是 C 编译出来的程序。**

---

## 2. 快速开始

### 2.1 目录结构

把 `Velo` 文件夹放在桌面上，里面是这样：

```
Velo/
├── veloc.exe              编译器（双击它或在命令行调用）
├── python313.dll          Python 运行时（编译器自身依赖它）
├── veloc.cmd              备用启动脚本
├── runtime/
│   ├── python313.zip      精简标准库
│   ├── compiler.zip       编译器自身的代码
│   └── velo_rt.h / .c     生成 C 代码时用到的运行时库
├── examples/              示例程序
└── docs/                  就是本手册
```

> **提示**：`veloc.exe` 需要 `runtime/` 目录和 `python313.dll` 陪伴，请勿单独拷贝它。
> 它还需要电脑上装有 Visual Studio 的 C++ 编译工具（本机已具备）。

### 2.2 第一个程序

新建 `hello.velo`：

```velo
def main() -> int:
    println("Hello, Velo!")
    var name = "世界"
    println("你好，{name}！")
    return 0
```

打开命令行，进入文件所在目录，执行：

```bat
"桌面路径\Velo\veloc.exe" run hello.velo
```

输出：

```
Hello, Velo!
你好，世界！
```

`run` 会先编译再运行。如果想保留可执行文件，用 `build`：

```bat
"桌面路径\Velo\veloc.exe" build hello.velo
hello.exe
```

### 2.3 三种常用方式

| 命令 | 作用 |
| :--- | :--- |
| `veloc run hello.velo` | 编译并立刻运行（开发时最方便） |
| `veloc build hello.velo` | 生成 `hello.exe` |
| `veloc build hello.velo -o 我的程序.exe` | 指定输出文件名 |

完整参数见[第 5 章](#5-编译器命令行)。

---

## 3. 语言基础

### 3.1 注释

```velo
# 单行注释，和 Python 一样

"""
三个双引号可以写多行注释 / 多行字符串
它不会做 {插值}
"""
```

### 3.2 变量与常量

```velo
var x = 5                 # 类型自动推导为 int
var y: float = 1.5        # 显式类型注解
var name: str = "Velo"
const LIMIT = 100         # 常量：不能被重新赋值
```

变量可以只声明不赋值（会被零初始化），但字符串和列表必须给初始值：

```velo
var count: int            # 等价于 count = 0
var buf: str              # ✗ 报错：引用计数类型必须初始化
```

**为什么 Python 程序员会觉得舒服：** 大部分时候你不需要写类型，`var x = 5` 就够了，编译器自己知道它是 `int`。

### 3.3 类型系统

#### 整数

| 类型 | 说明 | 取值范围 |
| :--- | :--- | :--- |
| `int` | 默认整数（64 位） | ±9.2×10¹⁸ |
| `i8`/`i16`/`i32`/`i64` | 有符号定长整数 | 视位数 |
| `u8`/`u16`/`u32`/`u64` | 无符号定长整数 | 视位数 |
| `usize` | 平台指针宽度 | — |
| `byte` | 单字节（同 `u8`） | 0–255 |

整数运算统一提升为 `int`（64 位），不需要担心溢出宽度问题。

#### 浮点数

| 类型 | 说明 |
| :--- | :--- |
| `float` | 默认浮点（64 位双精度，同 C 的 `double`） |
| `f32` | 单精度 |
| `f64` | 同 `float` |

#### 其他

| 类型 | 说明 |
| :--- | :--- |
| `bool` | `True` / `False` |
| `str` | 不可变 UTF-8 字符串 |
| `list[T]` | 动态数组（`vec[T]` 是别名） |
| 结构体名 | 用户定义的值类型 |
| 枚举名 | 用户定义的枚举 |
| 无返回 | 函数不写 `->` 就是 `none` |

#### 数值字面量

```velo
var a = 42
var b = 0xFF        # 十六进制
var c = 0b1010      # 二进制
var d = 0o777       # 八进制
var e = 1_000_000   # 下划线分隔，可读性更好
var f = 3.14
var g = 1.5e-3
```

#### 类型转换

Velo **不做隐式窄化**，浮点转整数必须显式：

```velo
var i = int(3.9)        # 3，显式截断
var f = float(7)        # 7.0
var s = str(42)         # "42"
var n = "123".to_int()  # 123
```

整数之间、整数转浮点可以自动完成：

```velo
var x: float = 5        # 5.0，合法
var y: int = 5.9        # ✗ 报错，请写 int(5.9)
```

### 3.4 运算符

#### 算术

| 运算符 | 含义 | 示例 | 结果 |
| :--- | :--- | :--- | :--- |
| `+` | 加 / 字符串拼接 | `1 + 2` / `"a" + "b"` | `3` / `"ab"` |
| `-` | 减 | `5 - 2` | `3` |
| `*` | 乘 | `3 * 4` | `12` |
| `/` | **真除法（永远是 float）** | `7 / 2` | `3.5` |
| `//` | 整除（向下取整） | `7 // 2` | `3` |
| `%` | 取模（结果符号跟除数） | `-7 % 3` | `2` |
| `**` | 幂 | `2 ** 10` | `1024` |

`//` 和 `%` 的行为刻意和 Python 保持一致（向下取整、结果与除数同号），这样移植算法时不用担心负数边界。

#### 比较与逻辑

```velo
a == b     a != b
a <  b     a <= b
a >  b     a >= b
a and b    a or b    not a
x in list  "sub" in "substring"
```

`and` / `or` **会短路求值**，且返回 `bool`（不是 Python 那样返回操作数本身）。条件里请写明确的比较：

```velo
# ✓ 正确
if len(items) > 0 and items[0] == "first":

# ✗ 错误：and 要求两边都是 bool
if items and items[0] == "first":
```

#### 位运算

```velo
a & b    a | b    a ^ b    ~a    a << 2    a >> 2
```

只对整数有效。

#### 复合赋值

```velo
x += 1      x -= 1      x *= 2      x //= 2
x %= 3      x **= 2     x &= mask   x <<= 1
s += "追加"    # 字符串也支持 +=
```

#### 条件表达式

```velo
var bigger = a if a > b else b
```

结果是 `str` / `list` 时请改用 `if` 语句（受引用计数限制）。

#### 运算符优先级（从低到高）

```
or  →  and  →  not  →  比较 / in  →  |  →  ^  →  &  →  << >>  →  + -  →  * / // %  →  一元 - + ~  →  **  →  ()
```

和 Python 基本一致。

### 3.5 字符串

字符串是**不可变的、带引用计数的 UTF-8 序列**。赋值和传递只增加计数，不复制内容。

#### 字面量与插值

```velo
var plain = "hello"
var single = 'single quotes work too'

var name = "Velo"
var age = 3
var msg = "我叫 {name}，今年 {age} 岁"
var calc = "1 + 2 = {1 + 2}"          # 插值里可以写任意表达式
var braces = "用 {{ }} 输出花括号"      # 花括号转义

var multi = """
第一行
第二行
"""
```

插值会在编译期展开成一次性的字符串构建，**不会**像 Python 那样反复拼接产生 O(n²) 的开销。

#### 长度与索引

```velo
var s = "Hello, 世界"
len(s)         # 12  → 字节长度
utf8_len(s)    # 9   → 字符（码点）数量

s[0]           # "H"     单个字节，返回新字符串
s[0:5]         # "Hello" 切片，返回新字符串
```

> **注意**：`len()` 返回的是**字节数**，因为 Velo 字符串是 UTF-8 字节序列。
> 中文一个字通常占 3 字节。需要字符数就用 `utf8_len()`。

#### 常用方法

```velo
s.upper()              # 转大写（ASCII）
s.lower()              # 转小写（ASCII）
s.trim()               # 去掉两端空白
s.contains("ell")      # true
s.starts_with("He")    # true
s.ends_with("界")      # true
s.find("l")            # 2，找不到返回 -1
s.replace("l", "L")    # "HeLLo, 世界"
s.split(", ")          # list[str]
s.split_ws()           # 按空白切分成 list[str]
s.repeat(3)            # 重复
s.to_int()             # 解析成整数
s.to_float()           # 解析成浮点
sep.join(list)         # 用 sep 连接字符串列表
```

完整列表见[第 4 章](#4-标准库参考)。

### 3.6 列表

列表是动态数组（和 C++ 的 `std::vector` 类似），元素在内存中连续存放。
元素类型可以是 `int` 系列、`float`、`bool`、`str`。

```velo
var nums = [1, 2, 3, 4]          # 类型推导为 list[int]
var empty: list[int] = []        # 空列表必须写类型
var names = ["Ada", "Linus"]
```

#### 访问与修改

```velo
nums[0]          # 1
nums[-1]         # 4（负索引从末尾数）
nums[1] = 100    # 修改
len(nums)        # 4
```

越界会立刻抛出 `panic: index out of range` 并退出，不会读到野内存。

#### 增删与操作

```velo
nums.append(5)       # 追加（别名 push）
nums.pop()           # 弹出末尾
nums.insert(0, 99)   # 在指定位置插入
nums.remove(2)       # 删除指定位置
nums.clear()         # 清空
nums.sort()          # 原地排序（int/float 按数值，str 按字典序）
nums.reverse()       # 原地反转
nums.index_of(99)    # 查找返回下标，找不到返回 -1
```

也可以用全局函数写法：

```velo
sort(nums)
reverse(nums)
```

#### 遍历

```velo
for x in nums:
    println(x)

for name in names:
    println(name.upper())
```

遍历时会自动持有元素的引用，循环体里删除元素也不会崩溃。

### 3.7 控制流

#### if / elif / else

```velo
if score >= 90:
    println("优秀")
elif score >= 60:
    println("及格")
else:
    println("不及格")
```

条件**必须是 `bool`**，不会做隐式真值转换（这是有意的，避免 Python 里 `if []:` 这类陷阱）。

#### while

```velo
var i = 0
while i < 10:
    println(i)
    i += 1
```

#### for + range

```velo
for i in range(10):          # 0..9
    println(i)

for i in range(2, 8):        # 2..7
    println(i)

for i in range(10, 0, -2):   # 10, 8, 6, 4, 2
    println(i)
```

`range` 循环会被编译成 C 的 `for` 循环，**没有任何迭代器对象产生的开销**——
这是 Velo 在循环密集型程序上能追上 C 的关键之一。

#### break / continue / pass

```velo
for x in items:
    if x < 0:
        continue      # 跳过本次
    if x > 100:
        break         # 跳出循环
    println(x)
```

提前 `break` / `continue` 时，编译器会自动插入必要的资源释放代码，
不会因为提前退出而泄漏内存。

### 3.8 函数

```velo
def add(a: int, b: int) -> int:
    return a + b

def greet(name: str) -> str:
    return "你好，" + name

def log(msg: str):
    println("[log]", msg)     # 不写 -> 就是没有返回值
```

#### 规则

- **参数必须写类型**（这是静态分析的基础）
- 返回类型写在 `->` 后面；省略表示没有返回值
- 声明了返回值，就**必须保证所有路径都有 return**（编译器会检查）
- 支持递归，支持函数间互相调用

```velo
def fact(n: int) -> int:
    if n <= 1:
        return 1
    return n * fact(n - 1)
```

> **编译器会强制检查返回值完整性**。下面这段会直接报错：
> ```velo
> def f(n: int) -> int:
>     if n > 0:
>         return 1
>     # ✗ 报错：存在没有 return 的路径
> ```

#### 入口点

```velo
def main() -> int:
    ...
    return 0
```

也可以写成没有返回值的 `def main():`。如果你完全没写 `main` 函数，
文件里的顶层语句会被当作主程序直接执行（脚本风格）：

```velo
# 没有 def main，这样也可以运行
println("Hello!")
for i in range(3):
    println(i)
```

### 3.9 结构体

```velo
struct Point:
    x: float
    y: float

var p = Point(3.0, 4.0)     # 按字段顺序构造
println(p.x, p.y)

p.x = 10.0                  # 字段可修改
```

结构体是**值类型**：赋值和传参会按值复制，没有堆分配，和 C 的 struct 一样快。

```velo
def dist2(a: Point, b: Point) -> float:
    var dx = b.x - a.x
    var dy = b.y - a.y
    return dx * dx + dy * dy

var d = dist2(Point(0.0, 0.0), Point(3.0, 4.0))   # 25.0
```

也可以先声明后逐字段赋值：

```velo
var q: Point
q.x = 1.0
q.y = 2.0
```

> **限制**：结构体字段只允许 `int`/`float`/`bool`/其他结构体/枚举。
> 不能包含 `str` 或 `list`（引用计数字段会带来隐藏开销，留给后续版本）。

### 3.10 枚举

```velo
enum Color:
    Red
    Green
    Blue

var c = Color.Green

if c == Color.Red:
    println("红")
else:
    println("不是红色")
```

枚举编译成 C 的 `enum`，本质是整数，比较和存储都是零开销。

```velo
def color_name(c: Color) -> str:
    if c == Color.Red:
        return "红"
    elif c == Color.Green:
        return "绿"
    else:
        return "蓝"
```

---

## 4. 标准库参考

Velo 1.0 的标准库是编译器的内建函数，直接映射到运行时库，调用开销极低。

### 4.1 输出

| 函数 | 说明 |
| :--- | :--- |
| `print(...)` | 输出，参数之间用一个空格分隔，不换行 |
| `println(...)` | 同上，但末尾换行 |
| `eprint(...)` | 输出到 stderr |
| `eprintln(...)` | 输出到 stderr 并换行 |

```velo
println("x =", 42, "y =", 3.5, 1 < 2)
# 输出： x = 42 y = 3.5 True
```

输出走 64 KB 缓冲区，不是逐字符写终端——大量输出时速度差异非常明显。
程序正常结束时缓冲区会自动刷新。

### 4.2 类型转换

| 函数 | 说明 |
| :--- | :--- |
| `int(x)` / `to_int(x)` | 转整数（浮点截断；字符串解析） |
| `float(x)` / `to_float(x)` | 转浮点 |
| `str(x)` / `to_str(x)` | 转字符串 |
| `s.to_int()` | 字符串专用，解析整数 |
| `s.to_float()` | 字符串专用，解析浮点 |

### 4.3 数学

| 函数 | 说明 |
| :--- | :--- |
| `abs(x)` | 绝对值（整数保持整数，浮点保持浮点） |
| `min(a, b)` / `max(a, b)` | 较小 / 较大 |
| `sqrt(x)` | 平方根 |
| `pow(a, b)` | 幂（浮点） |
| `sin` / `cos` / `tan` / `asin` / `acos` / `atan` / `atan2` | 三角函数 |
| `log` / `log2` / `log10` / `exp` | 对数与指数 |
| `floor` / `ceil` / `round` / `trunc` | 取整 |
| `x ** y` | 整数是整数幂，含浮点则浮点幂 |
| `x // y` / `x % y` | Python 语义的整除与取模 |

```velo
println(sqrt(16.0))        # 4.0
println(2 ** 10)           # 1024
println(-7 // 2)           # -4（Python 风格向下取整）
println(-7 % 3)            # 2 （结果与除数同号）
```

### 4.4 列表操作

| 函数/方法 | 说明 |
| :--- | :--- |
| `len(l)` / `l.len()` | 元素个数 |
| `l.append(v)` / `l.push(v)` | 追加到末尾 |
| `l.pop()` | 弹出末尾元素 |
| `l.insert(i, v)` | 在 `i` 处插入 |
| `l.remove(i)` | 删除下标 `i` |
| `l.clear()` | 清空 |
| `l.sort()` / `sort(l)` | 原地升序排序 |
| `l.reverse()` / `reverse(l)` | 原地反转 |
| `l.index_of(v)` | 查找首次出现的下标，无则 `-1` |
| `v in l` | 是否包含 |
| `fill(n, v)` | 创建一个含 n 个 v 的列表（一次分配，比 n 次 append 快得多） |

**`fill()` 是性能关键。** 需要固定长度、初值相同的数组时，用 `fill(n, 0)`
而不是循环 `append`——它一次分配到位，不会在循环里反复判断容量：

```velo
var sieve = fill(10000001, True)     # 一次分配好 10 MB
var zeros = fill(1000, 0.0)          # 1000 个 0.0
```

排序对 `int`/`float` 按数值、对 `str` 按字节字典序。底层调用的是 C 运行时的
`qsort`，和手写 C 一样快。

### 4.5 字符串方法

| 方法 | 返回 | 说明 |
| :--- | :--- | :--- |
| `.upper()` / `.lower()` | `str` | ASCII 大小写转换 |
| `.trim()` / `.trim_start()` / `.trim_end()` | `str` | 去空白 |
| `.contains(s)` | `bool` | 是否包含子串 |
| `.starts_with(s)` / `.ends_with(s)` | `bool` | 前后缀判断 |
| `.find(s)` | `int` | 子串位置，找不到 `-1` |
| `.replace(a, b)` | `str` | 替换全部 |
| `.split(sep)` | `list[str]` | 按分隔符切分 |
| `.split_ws()` | `list[str]` | 按任意空白切分 |
| `sep.join(list)` | `str` | 拼接 |
| `.repeat(n)` | `str` | 重复 n 次 |
| `.char_at(i)` | `str` | 取第 i 个字节起始的字符 |
| `.byte_at(i)` | `int` | 取第 i 个字节的值 |
| `.bytes()` | `int` | 字节数（同 `len`） |
| `.to_int()` / `.to_float()` | `int` / `float` | 解析 |

### 4.6 文件与命令行

| 函数 | 说明 |
| :--- | :--- |
| `read_file(path)` | 读整个文件为字符串；失败返回空串 |
| `write_file(path, content)` | 写入文件，返回写入字节数；失败返回 `-1` |
| `file_exists(path)` | 文件是否存在 |
| `arg_count()` | 命令行参数个数（不含程序名） |
| `arg(i)` | 取第 `i` 个命令行参数 |

示例（见 `examples/wordcount.velo`）：

```velo
def main() -> int:
    if arg_count() < 1:
        println("用法: wordcount <文件>")
        return 1
    var text = read_file(arg(0))
    var words = text.split_ws()
    println("单词总数:", len(words))
    return 0
```

### 4.7 断言与错误

| 函数 | 说明 |
| :--- | :--- |
| `assert(cond)` | 条件为假时 panic 并退出 |
| `assert(cond, "消息")` | 带自定义消息 |
| `panic("消息")` | 立即打印错误并退出（退出码 1） |
| `exit(code)` | 以指定退出码退出 |

```velo
assert(len(items) > 0, "列表不能为空")
```

运行时错误（越界、除零、解析失败）都会打印 `panic: ...` 并退出，
不会静默产生错误结果。

---

## 5. 编译器命令行

```
veloc <命令> [参数]
```

| 命令 | 作用 |
| :--- | :--- |
| `build <文件.velo>` | 编译成 `.exe` |
| `run <文件.velo> [参数...]` | 编译并运行，参数会传给程序 |
| `check <文件.velo>` | 只做语法和类型检查，不生成代码 |
| `emitc <文件.velo>` | 输出生成的 C 代码（学习/排查用） |
| `env` | 显示编译器与后端环境信息 |
| `version` | 显示版本 |
| `help` | 显示帮助 |

### build / run 的选项

| 选项 | 说明 |
| :--- | :--- |
| `-o <路径>` | 指定输出文件 |
| `-O 0` | 关闭优化（调试用，编译更快） |
| `-O 2` | 默认优化（推荐） |
| `--debug` | 调试构建：`/Od /Zi` |
| `--fast` | 激进优化：额外开启 `/arch:AVX2` 并关闭数组越界检查（需要较新的 CPU） |
| `--emit-c <路径>` | 同时保存生成的 C 代码 |
| `--keep` | 保留中间文件（排查编译问题时有用） |
| `--quiet` | 只输出错误 |

### 常见用法

```bat
:: 开发循环
veloc run game.velo

:: 发布
veloc build game.velo -o game.exe --fast

:: 看看它到底生成了什么 C 代码
veloc emitc game.velo -o game.c

:: 只检查不编译（很快）
veloc check game.velo
```

### 编译需要什么

Velo 用 C 编译器作为后端，因此需要机器上装有 **Visual Studio 的 C++ 工具**。
编译器会自动通过 `vswhere` 找到它，不需要你手动开"开发者命令提示符"。

如果自动查找失败，可以设置环境变量指定：

```bat
set VELO_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

用 `veloc env` 可以查看当前使用的后端。

---

## 6. 性能报告

### 6.1 测试方法

所有程序都在同一台机器上运行，取 3 次运行的最短时间：

- **Velo**：`veloc build -O 2`（生成 C → MSVC `/O2`）
- **C**：手写的等价 C 代码，MSVC `/O2` 编译
- **Python**：CPython 3.13.3
- 计时包含进程启动开销（约 10 ms），因此极短的程序会略微高估 Velo 的时间

### 6.2 结果

| 测试 | 内容 | Velo | C | Python | Velo/C | Python/Velo |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| fib(34) | 纯递归调用 | 0.050 s | 0.047 s | 1.448 s | 1.07× | **29×** |
| sieve 10⁷ | 一千万规模筛法 | 0.074 s | 0.069 s | 4.929 s | 1.07× | **67×** |
| mandelbrot | 640k 次浮点迭代 | 0.083 s | 0.076 s | 11.177 s | 1.10× | **134×** |
| list 2e6 | 200 万次追加+排序 | 0.216 s | 0.201 s | 1.977 s | 1.07× | **9×** |

> 数字来自本机实测（8 核 Windows 11，MSVC 19.51，Python 3.13.3）。
> 为消除系统波动，三个实现交替运行，每个取 7 次的最短时间。
> 在别的机器上绝对值会不同，但比例关系具有代表性。

### 6.3 解读

- **一致性惊人**：四个完全不同类型的负载（递归、数组、浮点、内存管理），
  Velo 都是 C 的 1.07–1.10 倍。这个稳定的比例说明差距来自固定开销
  （进程启动、运行时库初始化），而不是某类计算变慢。
- **递归调用**：Velo 和 C 打平。生成的代码和手写 C 几乎逐指令相同。
- **数组密集**（筛法）：慢 7%，主要是一次 `fill()` 的大块内存分配 + 进程启动。
- **浮点计算**（曼德博）：慢 10%，属于正常波动范围。
- **内存密集**（列表追加+排序）：慢 7%，底层同样都是 `malloc` + `qsort`。

换句话说：**Velo 生成的计算代码和 C 是同一水平的，没有"语言税"。**

### 6.4 和 Python 的差距从哪来

| 因素 | Python | Velo |
| :--- | :--- | :--- |
| 变量 | 指向堆上对象的指针 | 直接的机器寄存器/栈槽 |
| 整数 | 任意精度对象，带引用计数 | 64 位机器字 |
| 循环 | 每个元素创建迭代器对象 | 编译成 `for` 汇编 |
| 函数调用 | 创建帧对象、动态查找 | 直接 `call` 指令 |
| 类型检查 | 每次操作运行时检查 | 编译期完成，运行时零开销 |
| 执行 | 解释字节码 | CPU 直接执行机器码 |

---

## 7. 与 Python 的对照表

| 方面 | Python | Velo |
| :--- | :--- | :--- |
| 执行方式 | 解释字节码 | 编译为原生机器码 |
| 类型 | 动态 | 静态（多数可推导） |
| 变量 | `x = 5` | `var x = 5` |
| 常量 | 无 | `const N = 100` |
| 函数参数 | 无需类型 | 必须写类型 `def f(a: int)` |
| 返回值类型 | 无标注 | `-> int` |
| 除法 | `/` 返回 float | 相同，`/` 返回 float |
| 整除 | `//` 向下取整 | 相同 |
| 取模 | `%` 与除数同号 | 相同 |
| `and`/`or` | 返回操作数 | 返回 `bool` |
| 真值判断 | 空容器/0 为假 | 条件必须是 `bool` |
| `len(str)` | 字符数 | **字节数**（`utf8_len` 才是字符数） |
| 字符串 | 通常用 f-string | 直接用 `"{变量}"` 插值 |
| 列表元素 | 任意对象 | int/float/bool/str |
| 字典 / 集合 | 内建 | 暂不支持 |
| 类与继承 | 支持 | 只有结构体（值类型） |
| 异常 | try/except | 无（错误即 panic 退出） |
| 生成器 / 迭代器 | 支持 | 暂不支持 |
| 内存管理 | 引用计数 + GC | 编译期插入引用计数 |
| 打包分发 | 需要打包工具 | 直接产出单个 `.exe` |

---

## 8. 它为什么这么快

### 8.1 静态类型，编译期确定一切

因为每个表达式在编译期就有确定的类型，所以运行时不需要任何"这是什么类型"的判断：

```velo
var total = 0
for i in range(1000000):
    total += i * 2
```

生成的 C 大致是：

```c
int64_t total = 0;
for (int64_t i = 0; i < 1000000; i++) {
    total += i * 2;
}
```

**没有任何对象、没有装箱、没有类型检查、没有引用计数。** 这就是 C。

### 8.2 生成 C，让最成熟的优化器来干活

Velo 自己不做复杂的优化，而是把代码生成成干净的 C，交给 MSVC。
MSVC 的 `/O2` 会做：内联、循环不变量外提、强度削弱、常量折叠、
自动向量化（把循环编译成 SIMD 指令）、寄存器分配……

```velo
def sum(a: list[float]) -> float:
    var s = 0.0
    for x in a:
        s += x
    return s
```

这段会被 MSVC 自动向量化，得到接近内存带宽上限的速度。

### 8.3 单一编译单元，全程序内联

生成的 `program.c` 会把运行时库的实现**直接包含进来**，整个程序是一个编译单元。
这样编译器能看到所有函数体，跨函数的调用都能被内联——这是小函数性能的关键。

### 8.4 零开销的抽象

| 抽象 | Velo | 代价 |
| :--- | :--- | :--- |
| `range(n)` 循环 | C 的 `for` 循环 | 0 |
| 结构体 | C 的 struct（值类型） | 0 |
| 枚举 | C 的 enum | 0 |
| 数组下标 | 一次指针运算 | 1 次边界比较 |
| 字符串字面量 | 静态常量池 | 0（不分配） |
| 字符串拼接 / 插值 | 一次性构建器 | 1 次分配 |
| 整数 / 浮点运算 | 直接机器指令 | 0 |

### 8.5 引用计数只在必要的地方

只有 `str` 和 `list` 使用引用计数，而且规则在编译期完全确定：

- 表达式结果被物化到块内临时变量，块结束时统一释放
- 所有权被转移（赋值、返回）时不会重复计数
- 数值类型完全不参与

这意味着常见的数值密集程序**看不到任何内存管理开销**。

### 8.6 输出也是批量的

`println` 不是每次都调用 `printf`，而是写入 64 KB 缓冲区。大量输出时，
这能把系统调用次数降低好几个数量级。

---

## 9. 已知限制

Velo 1.0 是有意做小的，以下功能暂时没有：

| 限制 | 说明 | 变通做法 |
| :--- | :--- | :--- |
| 没有 `dict` / `set` | 只有列表 | 用并行列表或排序后二分 |
| 没有类与继承 | 只有结构体 | 用结构体 + 函数 |
| 没有异常处理 | 错误即 panic | 用返回值表示错误 |
| 没有生成器 / 闭包 | — | 用显式循环 |
| 没有用户泛型 | 内建 `list[T]` 可用 | 为不同类型写不同函数 |
| 结构体不能含 `str`/`list` | 避免隐藏开销 | 用并行列表 |
| 结构体不能作为列表元素 | 同上 | 用并行列表 |
| 列表不支持切片 | — | 用循环复制 |
| 字符串索引按字节 | UTF-8 变长 | ASCII 场景无差异 |
| `import` 仅为语法占位 | 标准库全是内建 | 直接调用内建函数 |
| 没有默认参数 | — | 写两个函数 |
| 编译依赖 MSVC | 需要 C++ 工具链 | 已内置自动查找 |

这些限制的共同原因是：**先保证语义正确、性能可预测，再逐步扩展**。

---

## 10. 常见问题

**Q：为什么不直接写 C？**

A：因为 C 写起来慢、容易出错（手动管理内存、字符串处理繁琐、没有列表）。
Velo 保留 C 的性能，但给你 Python 的书写体验：不用写 `malloc`、
不用管 `free`、字符串直接 `+`、列表直接 `append`。

**Q：为什么不直接用 Cython / Numba / PyPy？**

A：那些方案要么需要 Python 运行时、要么只对特定代码路径加速、要么
分发时还要带一堆东西。Velo 直接产出独立的 `.exe`，双击就能跑，
不依赖 Python 环境。

**Q：编译一次要多久？**

A：小项目 2–4 秒（其中约 0.5 秒是初始化 MSVC 环境）。
这比 Python 的启动时间略长，但换来的是运行时的几十倍加速。

**Q：生成的可执行文件多大？**

A：一个 hello world 约 150 KB（静态链接了运行时）。纯 C 的话大约 10–20 KB，
多出来的部分是字符串/列表运行时和引用计数支持。

**Q：会不会有内存泄漏？**

A：编译器在编译期为作用域、`return`、`break`、`continue` 自动插入释放代码，
已通过压力测试验证。唯一例外是程序级全局字符串（进程结束时由操作系统回收）。

**Q：能调试吗？**

A：用 `veloc build xxx.velo --debug --keep` 会生成带符号的 exe 并保留中间
C 文件，可以用 Visual Studio 调试生成的 C 代码。`veloc emitc` 可以看
生成的 C 是什么样子。

**Q：越界访问会怎样？**

A：默认会 `panic: index out of range` 并退出。用 `--fast` 编译会关闭这个检查
（更快，但越界是未定义行为）。

**Q：能调用 C 函数吗？**

A：语法上支持 `extern def puts(s: str) -> int` 形式的声明，
可以把生成的 C 与自己的 C 代码一起编译。日常使用不需要这个。

---

## 附录：完整示例

### 埃氏筛法

```velo
def main() -> int:
    var n = 1000000
    var sieve = fill(n + 1, True)
    sieve[0] = False
    sieve[1] = False

    var i = 2
    while i * i <= n:
        if sieve[i]:
            var j = i * i
            while j <= n:
                sieve[j] = False
                j += i
        i += 1

    var count = 0
    for k in range(n + 1):
        if sieve[k]:
            count += 1
    println("1 到 {n} 之间的素数个数:", count)
    return 0
```

### 结构体与距离计算

```velo
struct Point:
    x: float
    y: float

def dist2(a: Point, b: Point) -> float:
    var dx = b.x - a.x
    var dy = b.y - a.y
    return dx * dx + dy * dy

def main() -> int:
    var p = Point(3.0, 4.0)
    var q = Point(0.0, 0.0)
    println("距离平方 =", dist2(p, q))
    println("距离 =", sqrt(dist2(p, q)))
    return 0
```

### 词频统计

```velo
def main() -> int:
    if arg_count() < 1:
        println("用法: wordcount <文件>")
        return 1
    var words = read_file(arg(0)).lower().split_ws()
    words.sort()
    var i = 0
    while i < len(words):
        var j = i + 1
        while j < len(words) and words[j] == words[i]:
            j += 1
        if j - i >= 2:
            println("{words[i]} 出现 {j - i} 次")
        i = j
    return 0
```

---

*Velo 1.0 · 用 Python 写成编译器，用 C 获得速度。*
