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
