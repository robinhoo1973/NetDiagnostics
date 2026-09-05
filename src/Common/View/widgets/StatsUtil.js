// =============================================================================
// StatsUtil.js — groupStats 键归一化单一来源
// =============================================================================
// 5WHY (复核 2026-08-18 Reuse C3): {pass, warn, fail, skip, info, error,
// cancelled, completed, total, durationMs} 键集合 + `|| 0` 守卫曾逐字复制在
// PageGroupPanelSection._refreshStats / PageStatusHeaderSection._refreshAgg /
// DashboardSummaryComp._refresh 三处——加 cancelled 键时三处同步编辑。虽然
// C++ groupStats 已零填充所有键，本模块仍是单一权威：新统计键只需一处扩展，
// 未来 C++ 若移除零填充保证，守卫也集中在此。
// =============================================================================
.pragma library
// 5WHY (复核 2026-08-18 汇总栏空白根因 + 惯例对齐): 三个消费方
// （PageStatusHeaderSection / PageGroupPanelSection / DashboardSummaryComp）
// 曾写作 `W.StatsUtil.normalize(...)`——`import "...StatsUtil.js" as W` 中
// W 即文件命名空间，W.StatsUtil 恒为 undefined → 每次调用抛 TypeError →
// 统计对象残留 undefined/零值 → QML 绑定静默降级：状态头只剩孤立图标、
// 组头只剩 "0"、仪表摘要卡只剩空态句。
// 首轮修复曾以 `var StatsUtil = {normalize}` 迎合既有调用点，但本库惯例是
// 顶层函数经导入命名空间消费（IconTints.tintFor / KeyMetric.formatNumber）。
// 收敛为顶层函数 + 调用点 `W.normalize(...)`，命名空间不双重嵌套。

// 从 groupStats QVariantMap 归一化全部统计键；null/缺失安全。
function normalize(map) {
    return {
        total:                   (map && map.total) || 0,
        completed:               (map && map.completed) || 0,
        completedExclCancelled:  (map && map.completedExclCancelled) || 0,
        pass:                    (map && map.pass) || 0,
        warn:                    (map && map.warn) || 0,
        fail:                    (map && map.fail) || 0,
        skip:                    (map && map.skip) || 0,
        info:                    (map && map.info) || 0,
        error:                   (map && map.error) || 0,
        cancelled:               (map && map.cancelled) || 0,
        durationMs:              (map && map.durationMs) || 0
    }
}

// 5WHY (复核 2026-08-19): 数组身份门控（内容未变不替换 model 数组，避免
// Repeater 全量重建）曾在 Dashboard/Diagnostic 两屏逐字复制——收敛为单一
// helper；内容相同时返回旧数组（身份保持）。元素级比较（本库消费方均为
// ≤5 元素 int 数组，JSON 序列化比较属过量成本）。
function assignIfChanged(current, next) {
    // 5WHY (复核 2026-08-19 null 契约): 与 normalize 同族却曾无 null 守卫——
    // 消费方以未初始化 var 起步时 TypeError。null/undefined 累积器视为
    // "无身份可保"，直接采纳新数组。
    if (!current || !next) return next
    if (current.length !== next.length) return next
    for (var i = 0; i < next.length; ++i)
        if (current[i] !== next[i]) return next
    return current
}

// 5WHY (复核 2026-09-05 三轮 契约分支收敛): shareReportFile 返回值 → toast
// 键的三态分支（ok=已复制、非空路径=已导出打开、空=失败）曾在
// DiagnosticScreen / DashboardScreen(ShareButtons) / DashboardScreen(
// onSectionAction) 三处逐字复制——契约漂移时漏改一处即复活"失败提示成功"
// 的文案背离。单一 helper：AppState 返回契约的 QML 侧唯一映射点。
function shareOutcomeToastKey(out) {
    if (out === "ok") return "reportCopied"
    if (out !== "") return "reportOpened"
    return "shareFailed"
}

// 5WHY (复核 2026-09-05 三轮 空转身份门控): 统计对象曾无条件替换身份——
// 计数未变的空转 tick 也触发 _total/_completed/徽标/表头绑定整轮重估。
// 逐键比较（normalize 键集为单一来源，新增键自动纳入比较）：内容相同
// 保持旧身份，跳过重估。
function statsEqual(a, b) {
    if (!a || !b) return a === b
    for (var k in b)
        if (a[k] !== b[k]) return false
    return true
}

// 5WHY (复核 2026-09-05 /simplify 语义身份): 瓦片墙身份门控曾用滚动哈希
// （碰撞/哨兵/未来键三缺陷见 PageGroupPanelSection 注释）。逐元素比较：
// 同 O(n)、无分配、无碰撞类；diagId/status 之外的未来键自动纳入比较
// （哈希版会静默不门控）。
function sameTiles(a, b) {
    if (!a || !b) return a === b
    if (a.length !== b.length) return false
    for (var i = 0; i < a.length; ++i) {
        if (a[i].diagId !== b[i].diagId) return false
        if (a[i].status !== b[i].status) return false
    }
    return true
}

// 5WHY (复核 2026-09-05 /simplify 行数第三实现): 行数统计曾在 TerminalBlock
// 局部 _lineCount 与 PageTerminalSection 的 split('\n').length 各写一份——
// 空串/尾随换行语义必须与打字机动画计数、区块高度估算一致，漂移即卡片
// 错高/active 门控失配。单一来源：零分配单遍 charCodeAt 扫描（split 为
// 长终端输出如 traceroute 分配 N 个字符串数组）。
function lineCount(s) {
    if (!s || s === "") return 0
    var n = 1
    for (var i = 0; i < s.length; ++i)
        if (s.charCodeAt(i) === 10) n++
    return n
}
