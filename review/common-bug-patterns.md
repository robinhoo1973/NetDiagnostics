# 常见 Bug 模式分类与提交前自检清单

> 从 20+ 个 (5WHY) 修复提交中提炼的系统性 bug 模式分类。
> **每次 `git commit` 前，逐条检查本次变更是否含有类似问题。**

---

## 目录

1. [A. QML 绑定依赖追踪断裂](#a-qml-绑定依赖追踪断裂)
2. [B. C++ 元对象属性访问错误](#b-c-元对象属性访问错误)
3. [C. C++ 类型系统与前置声明](#c-c-类型系统与前置声明)
4. [D. 数据迁移与默认值陷阱](#d-数据迁移与默认值陷阱)
5. [E. QML 文件结构性损坏](#e-qml-文件结构性损坏)
6. [F. 翻译/国际化缺陷](#f-翻译国际化缺陷)
7. [G. 构建管道文件缺失](#g-构建管道文件缺失)
8. [H. 代码质量退化](#h-代码质量退化)

---

## A. QML 绑定依赖追踪断裂

### 问题描述

QML 绑定引擎只能追踪通过 `QObject::property()` 元对象系统的属性读取。
当 C++ 代码直接读取成员变量（`m_xxx`）而非通过属性系统时，QML 绑定不会
在属性变更时重新求值，导致 UI 显示过期数据。

### 5WHY 根因链

```
直接成员访问 → QML 绑定引擎不可见 → NOTIFY 信号发出后绑定不重新求值
→ UI 停留在旧状态直到组件重建
```

### 出现场景

1. **C++ 上下文属性暴露给 QML**，其 `Q_INVOKABLE` 方法内部直接读取成员变量
2. **JS 函数调用链**中，QML 无法追踪 JS 函数内部的属性读取
3. **任何 C++ 对象作为 Q_PROPERTY 的 READ 访问器**，但被绕过直接访问成员

### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `f622395f` | `Translator::select()` 直接读取 `m_lang` | 语言切换后所有 `T.tr()` 绑定不更新 |
| `bbe1deaa` | `Tr.diagName()`/`Tr.groupName()` 等 JS 函数调用不被 QML 追踪 | 语言切换后诊断名/组名停留在旧语言 |

### 检测方法

```bash
# 检查 Q_INVOKABLE 方法中是否有直接读取 Q_PROPERTY 成员变量的情况
grep -n "Q_INVOKABLE\|Q_PROPERTY" src/ -r --include="*.h" | while read line; do
  # 确保对应的 .cpp 中使用 property() 而非直接成员访问
done
```

### 修复模式

```cpp
// ❌ 错误 — 直接成员访问绕过 QML 绑定引擎
QString MyClass::getLabel() const {
    return m_data[m_lang];  // m_lang 是 Q_PROPERTY，但直接读取
}

// ✅ 正确 — 通过属性系统读取
QString MyClass::getLabel() const {
    return m_data[property("lang").toInt()];  // QML 可追踪
}
```

### 提交前检查

- [ ] 所有 `Q_INVOKABLE` 方法是否通过 `property("xxx")` 读取 Q_PROPERTY？
- [ ] 新增的 C++ 上下文属性类中，READ 访问器中是否直接读取成员？
- [ ] QML 中是否有 JS 函数调用（非直接属性绑定），且调用内部依赖了 `lang` 或其他可变属性？

---

## B. C++ 元对象属性访问错误

### 问题描述

Qt 的元对象系统 API 中，`QObject::property()` 和 `QMetaObject::property()` 是
两个不同函数：
- `QObject::property(const char* name)` → 读取属性值，返回 `QVariant`
- `QMetaObject::property(int index)` → 获取属性元数据描述符，返回 `QMetaProperty`

混淆这两个 API 会导致编译错误 `cannot convert 'int' to 'const char*'`。

### 5WHY 根因链

```
QMetaObject::property(int) 返回 QMetaProperty → 错误地当作 QObject::property() 的重载
→ 传入 int 而非 const char* → 编译失败
```

### 出现场景

1. 任何涉及 Qt 属性系统的代码变更
2. 使用 `indexOfProperty()` 获取元属性索引后，误用该索引调用 `property()`
3. 缓存元属性索引以优化性能时

### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `e4d806dd` | `property(kLangPropIdx)` 传入 int，但只有 `property(const char*)` 重载 | 全部 8 个平台编译失败 |

### 检测方法

```bash
# 检查是否有 int 变量传给 property()
grep -rn "property(" src/ --include="*.cpp" | grep -v '"'
```

### 修复模式

```cpp
// ❌ 错误 — QObject 没有 property(int) 重载
static const int idx = MyClass::staticMetaObject.indexOfProperty("lang");
QVariant v = property(idx);  // 编译错误！

// ✅ 方式 1 — 使用 const char* 重载（简洁，性能足够）
QVariant v = property("lang");

// ✅ 方式 2 — 使用 QMetaProperty::read()（正确使用元属性索引）
static const int idx = MyClass::staticMetaObject.indexOfProperty("lang");
QVariant v = metaObject()->property(idx).read(this);
```

### 提交前检查

- [ ] 是否向 `QObject::property()` 传递了整数？
- [ ] 是否缓存了 `indexOfProperty()` 的结果？如果是，是否正确使用了 `QMetaProperty::read()`？

---

## C. C++ 类型系统与前置声明

### 问题描述

在 C++ 类体内部写 `class Foo;` 是**嵌套类型的前置声明**（`Outer::Foo`），
而非命名空间作用域的 `::Foo`。在类体外部使用 `new Foo(this)` 时，编译器
看到的是不完整的嵌套类型。

### 5WHY 根因链

```
class Foo; 放在 class Outer { ... }; 内部
→ 声明的是 Outer::Foo，不是 ::Foo
→ Outer.cpp 中 new Foo(this) 尝试实例化 Outer::Foo（不完整类型）
→ 编译错误
```

### 出现场景

1. 在头文件中向现有类添加新成员指针，前置声明写在类体内部
2. 从其他文件复制前置声明时，未注意声明位置（命名空间作用域 vs 类作用域）
3. 快速添加成员变量后未编译验证

### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `3ad875d9` | `class Translator;` 写在 `class AppState { };` 内部 | `new Translator(this)` 类型不完整，编译失败 |

### 检测方法

```bash
# 检查类体内部的前置声明 — 应该在命名空间作用域
grep -n "class.*;" src/ -r --include="*.h" | grep -v "^\s*//"
```

### 修复模式

```cpp
// ❌ 错误 — 前置声明在类体内部
class AppState : public QObject {
    class Translator;  // 这是 AppState::Translator！
    Translator* m_translator;
};

// ✅ 正确 — 前置声明在命名空间作用域
class Translator;
class AppState : public QObject {
    Translator* m_translator;
};
```

### 提交前检查

- [ ] 前置声明是否在命名空间作用域（文件顶部，类体外部）？
- [ ] 新增的指针成员是否在类体内部写了 `class Foo;`？
- [ ] 编译是否通过？（至少运行本地构建）

---

## D. 数据迁移与默认值陷阱

### 问题描述

数据迁移逻辑在以下情况下会错误触发：
- 迁移版本号（schema version）默认为旧值
- 新安装没有保存的数据，但 QSettings 仍返回默认值
- 默认值恰好是旧版索引，被迁移逻辑错误转换

### 5WHY 根因链

```
QSettings.value("key", defaultVal) 在新安装时返回 defaultVal
→ schemaVersion 检查 < currentVersion 为真（因为默认值是旧版）
→ defaultVal 被迁移函数错误转换，变成另一个值
→ 用户看到完全错误的配置
```

### 出现场景

1. **QSettings 迁移逻辑**：`s.value("key", default)` + `schemaVersion < current`
2. **枚举重排序**：旧版 `0=EN` 变成 `0=ZH_CN`，但保存的数据仍是旧索引
3. **新字段**：添加带默认值的新 QSettings 键，默认值恰好落在迁移范围内

### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `3ad875d9` | 迁移门控 `schemaVer < 2` + `s.value("language", 7)` → 新安装默认 7（EN）被迁移为 12（ES） | 新用户看到西班牙语 UI |
| `31cbe174` | 语言索引从 9 扩展到 15，旧索引含义完全改变 | 升级用户看到错误语言 |

### 检测方法

```bash
# 检查设置默认值 + 迁移逻辑的组合
grep -n "s.value\|schemaVersion\|migrate\|QSettings" src/ -r --include="*.cpp"
```

### 修复模式

```cpp
// ❌ 错误 — 新安装的默认值被错误迁移
int schemaVer = s.value("schemaVersion", 1).toInt();  // 默认 1
int lang = s.value("language", 7).toInt();            // 默认 7 (EN)
if (schemaVer < 2) {
    lang = migrateLanguageIndex(lang);  // 7 → 12 (ES)！
}

// ✅ 正确 — 通过 contains() 检测是否为真实保存的数据
if (schemaVer < 2 && s.contains("language")) {
    lang = migrateLanguageIndex(lang);
    s.setValue("language", lang);
    s.setValue("schemaVersion", 2);
}
```

### 提交前检查

- [ ] QSettings 迁移是否使用 `s.contains()` 检测真实保存的数据？
- [ ] 默认值是否与迁移逻辑兼容？（新安装的默认值不应被迁移转换）
- [ ] 枚举/索引重排序后，是否有对应的迁移映射表？
- [ ] 迁移是否在第一次运行后持久化了新版本号？

---

## E. QML 文件结构性损坏

### 问题描述

任何自动化脚本或批量编辑可能将 `.qml` 文件替换为非 QML 内容（原始数据、
拼接的函数调用、日志输出），导致 QML 引擎在启动时解析失败。

iOS 静态 Qt 构建尤其脆弱 — **任何单个 QML 错误都会导致整个应用崩溃**：
```
单个 QML 错误 → engine.load() 失败 → rootObjects 空 → return -1 → 闪退
```

### 5WHY 根因链

```
自动化脚本输出非 QML 内容 → 替换了整个 .qml 文件
→ pre-commit 无结构验证 → CI 无 qmllint → iOS 解析失败 → 闪退
```

### 出现场景

1. **批量参数扩展**：向函数调用添加第 N 个参数时，输出可能变成原始数据而非有效 QML
2. **代码生成脚本**：生成器输出格式错误或截断
3. **合并冲突解决**：手动解决冲突时删除了文件头部（pragma/import）
4. **AI/自动化工具编辑**：LLM 或脚本替换内容时缺少 QML 语法意识

### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `49464d89` | 自动化脚本将 415 行 QML 替换为 11 行原始 `t()` 调用数据 | iOS 启动闪退 `Expected token '{'` |

### 检测方法

```bash
# pre-commit check 14 + 21 自动执行
/usr/lib/qt6/bin/qmllint file.qml
head -1 file.qml  # 必须以 // 或 pragma 或 import 或根类型开头
```

### 修复模式

- **预防**：pre-commit check 14（结构验证）+ check 21（qmllint）
- **修复**：恢复完整的 QML 文档结构（pragma/import/根元素）
- **永远不要**：让脚本直接覆盖 `.qml` 文件而不验证输出

### 提交前检查

- [ ] 本次提交是否包含 `.qml` 文件的自动化生成/修改？
- [ ] 每个 `.qml` 文件第一行是否以 `//`, `/*`, `pragma`, `import` 或根类型 `Item {` 等开头？
- [ ] `qmllint` 是否对暂存的 `.qml` 文件零错误通过？
- [ ] 文件是否为有效的 QML 文档结构（不只是数据片段）？

---

### E.1 内联 JS 块绑定 `};` 同行后续属性（iOS qmlcachegen 启动闪退）

#### 问题描述

用内联 JS 块绑定强制方法调用重求值时，若块闭合 `}` 后**同一行**紧跟
`;` + 下一个属性，iOS 静态 Qt 构建的 qmlcachegen 会拒绝该写法并报
`Unexpected token ';'`：

```qml
// ❌ 崩溃 — 块闭合 `}` 与 `; lbl:` 同一行
SummaryStat { val: { var _ = page._groupsVersion; return calcTotalTime() }; lbl: T.tr("totalTimeLabel") }

// ✅ 安全 — 下一属性另起一行（ConfigScreen.qml 的既有模式）
Label {
    text: { let _ = configPollVersion; return T.groupName(currentGroup) }
    font.family: ...
}
```

#### 5WHY 根因链

```
强制方法调用重求值 → 写内联块绑定 `prop: { ... return ... }`
→ 为省行在同一行继续写下一属性（`}; 下一属性`）
→ iOS qmlcachegen 对块后同行 `;` 报 "Unexpected token ';'"
→ 该 QML 解析失败 → main.qml rootObjects=0 → return -1 → 启动闪退
```

#### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `4c2e02b2` | DashboardScreen.qml 3 处 `val/text: { var _ = ...; return ... }; 下一属性` 同行块绑定 | iOS 启动闪退 `Unexpected token ';'`（build 313, crash/NetDiagnostics_startup(2).log） |

#### 检测方法

pre-commit check 23 自动执行（剥离注释/字符串后匹配）：
```bash
grep -nE ':\s*\{[^{}]*\breturn\b[^{}]*\}\s*;'   # 剥离注释后的暂存 QML
```

#### 修复模式

- **首选**：把值计算进可追踪属性（`property string _x: ""` + 在 `progressChanged`
  的 `reload()`/`refreshSummary()` 里赋值），绑定 `text: _x` —— 无内联块，最稳妥
- **次选**：块绑定后换行，下一属性另起一行（ConfigScreen 模式）
- **不要**：在 `prop: { ... };` 同一行继续写属性

#### 提交前检查

- [ ] 暂存 QML 中是否有 `prop: { ... return ... };` 同行后续属性？（pre-commit check 23）
- [ ] 强制方法调用重求值时，是否用可追踪属性/版本号而非内联块绑定？

---

## F. 翻译/国际化缺陷

### 问题描述

翻译系统中的错误可能来自多个维度：
- 语法/拼写错误（各国语言特有）
- 语言切换后 UI 不更新（绑定问题，见 A 类）
- 缺失翻译条目（JSON/QML 不同步）
- 语言索引边界不一致
- 特定语言被边界守卫排除

### 出现场景

1. **添加新翻译键**后，忘记在 `translations.json` 中添加所有 15 种语言
2. **边界守卫错误**：`lang <= 0` 排除了简体中文（索引 0）
3. **翻译数据源漂移**：C++ 中的英文名称与 QML/JSON 不同步
4. **参数化消息**：新增的 C++ 错误消息未加入 `trMsg` 的精确匹配字典

### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `bbe1deaa` | `Tr.trMsg()` 中 `lang <= 0` 排除中文 | 中文用户看到英文错误消息 |
| `5de0de24` | 10 个测试名翻译存在语法/术语错误 | 多语言 UI 不专业 |
| `bbe1deaa` | `Tr.diagName()` 未在所有调用点使用 — 部分直接用 C++ 英文名 | 语言切换后诊断名保持英文 |

### 检测方法

```bash
# 验证 JSON 中所有 15 种语言的条目
python3 -c "
import json
with open('resources/translations.json') as f:
    d = json.load(f)
for k, v in d['properties'].items():
    if len(v) != 15:
        print(f'MISSING: {k} has {len(v)} entries')
"

# 检查 T.tr() 调用是否与 JSON 键匹配
grep -roh 'T\.tr("[^"]*")' src/ --include="*.qml" | sort -u > /tmp/qml_keys.txt
python3 -c "
import json
with open('resources/translations.json') as f:
    d = json.load(f)
for k in set(d['properties'].keys()):
    print(k)
" | sort > /tmp/json_keys.txt
diff /tmp/qml_keys.txt /tmp/json_keys.txt
```

### 提交前检查

- [ ] 新增翻译键是否在 `translations.json` 的所有 6 个分区中都存在？
- [ ] 边界守卫是否错误排除了有效语言索引（如 `<= 0`）？
- [ ] 参数化错误消息模板是否正确处理所有可能的 C++ 输入？
- [ ] UI 标签中的英文是否专业且一致（Title Case / Sentence case）？
- [ ] 新增语言索引时，所有 `if (index > N)` 守卫是否已更新？

---

## G. 构建管道文件缺失

### 问题描述

`.gitignore` 规则可能意外排除构建所需的关键文件（安装脚本、部署脚本、
签名配置等），导致 CI 在运行时找不到文件。

### 5WHY 根因链

```
.gitignore 通配规则过宽 → 排除了关键文件
→ 文件未提交到仓库 → CI checkout 后缺失 → 构建/部署失败
```

### 出现场景

1. **清理 .gitignore**：删除"过时"条目时，未检查该目录下是否仍有被追踪的文件
2. **添加新脚本目录**：目录在 `.gitignore` 规则范围内，新文件自动被忽略
3. **迁移文件位置**：将文件移入被忽略的目录

### 具体案例

| Commit | 问题 | 表现 |
|--------|------|------|
| `70534811` | `.gitignore` 中 `scripts/` 规则排除了 `scripts/installer/NetDiagnostic.nsi` | Windows 动态构建失败 |
| `d44f6977` | 同上原因排除了 `scripts/ios/asc-app.sh` | iOS TestFlight 部署失败 |

### 检测方法

```bash
# 检查 .gitignore 是否排除了被追踪的文件
git ls-files --cached | while read f; do
  git check-ignore "$f" && echo "TRACKED BUT IGNORED: $f"
done

# 检查暂存区是否有新文件被忽略
git ls-files --others --exclude-standard
```

### 修复模式

```gitignore
# ❌ 错误 — 整个目录被忽略
scripts/

# ✅ 正确 — 排除通配，但允许关键文件
scripts/
!scripts/pre-commit
!scripts/install-hooks
!scripts/installer/
```

### 提交前检查

- [ ] `.gitignore` 修改是否会影响被追踪的文件？
- [ ] 新增的构建/部署脚本文件是否确实被 `git add` 暂存？
- [ ] CI 所需的所有文件是否都在仓库中？（检查 `git ls-files`）

---

## H. 代码质量退化

### 问题描述

快速的代码变更可能引入以下退化：
- 缺失 `#include` 头文件，依赖脆弱的传递包含
- 死代码（赋值但从未读取的变量）
- 错误注释（描述已变更的行为）
- 静默截断数据（无警告）

### 出现场景

1. **使用 Qt 容器类型**但未显式包含对应头文件（如 `QSet`、`QJsonArray`）
2. **重构后遗留**：旧的局部变量、旧的注释、旧的别名
3. **数据边界处理**：数组/列表在解析时未检查长度，静默截断多余条目

### 具体案例

| Commit | 问题 |
|--------|------|
| `3ad875d9` | 缺失 `#include <QSet>` — 依赖 `QJsonObject` 的传递包含 |
| `3ad875d9` | 死代码 — `QJsonObject tmpl = ...` 赋值后从未使用 |
| `3ad875d9` | `parseLangArray()` 静默忽略 >15 个条目的数组，无警告 |

### 检测方法

```bash
# 检查未使用的变量（编译器警告）
# 检查缺失的 include（iwyu 或编译器隐式依赖警告）

# 检查数组/列表处理是否有边界检查
grep -rn "toArray\|toList\|\.size()" src/ --include="*.cpp" | grep -v "if\|assert\|Q_ASSERT"
```

### 修复模式

```cpp
// 每个显式使用的 Qt 类型都应有对应的 #include
#include <QSet>       // 使用 QSet<T> 时需要
#include <QJsonArray>  // 使用 QJsonArray 时需要

// 数组解析应添加截断警告
if (arr.size() > kMaxLanguages) {
    qWarning() << "Truncating" << arr.size() << "to" << kMaxLanguages;
}

// 删除死代码，更新过时注释
```

### 提交前检查

- [ ] 编译器是否产生新的警告？（检查 CI 构建日志）
- [ ] 新增的 Qt 类型使用是否有对应的 `#include`？
- [ ] 是否有赋值后从未读取的变量？
- [ ] 数组/列表解析是否有长度验证和截断处理？
- [ ] 注释是否与实际代码行为一致？

---

## 提交前快速自检清单

> 以下检查项覆盖 A-H 所有类别，每次 `git commit` 前执行。

### 🔴 阻止级（不通过则提交中止）

- [ ] **A/B** Q_INVOKABLE 方法中是否通过 `property("xxx")` 而非直接成员访问读取 Q_PROPERTY？
- [ ] **C** 前置声明是否在命名空间作用域（类体外部）？
- [ ] **D** QSettings 迁移是否使用 `s.contains()` 检测真实保存的数据？
- [ ] **E** 暂存的 `.qml` 文件是否通过 `qmllint` 零错误？
- [ ] **E** 暂存的 `.qml` 文件第一行是否为有效 QML 结构？
- [ ] **G** `.gitignore` 修改是否排除了 CI 所需的关键文件？

### 🟡 高危（可能影响功能正确性）

- [ ] **D** 默认值是否与迁移逻辑兼容？
- [ ] **F** 新增翻译键是否在 `translations.json` 中存在？
- [ ] **F** 边界守卫是否错误排除了有效语言索引？
- [ ] **A** QML 中是否有多层 JS 函数调用链依赖可变属性？

### 🟢 常规（代码质量保障）

- [ ] **H** 新增的 Qt 容器类型使用是否有对应的 `#include`？
- [ ] **H** 是否有死代码（赋值未读取、未使用的变量）？
- [ ] **H** 数组/列表解析是否有边界检查和截断处理？
- [ ] **H** 注释是否与代码行为一致？
