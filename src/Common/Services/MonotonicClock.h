// =============================================================================
// MonotonicClock.h — 进程级单调毫秒基准（DiagnosticBase / AppState 共用）
//
// 5WHY (复核 2026-08-20 墙钟计时): UI 运行计时曾以墙钟（Date.now() 与
// QDateTime::currentMSecsSinceEpoch）相减——两者同源一致，但 NTP 校时/
// 手动改时在长探针（180s）中段步进，计时圆点瞬时跳变（颜色阈值也随
// 错误时长重估）。QElapsedTimer 单调（CLOCK_MONOTONIC）不受墙钟步进
// 影响。Meyer's singleton 惰性起表（SIOF 安全：函数局部 static）。
// =============================================================================
#pragma once

#include <QElapsedTimer>

inline qint64 monotonicMsSinceAppStart() {
    static const QElapsedTimer kStart = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    // 5WHY (复核 2026-08-21 首轮零值碰撞 + 哨兵显式化): 首轮首探针初始化
    // kStart 时 elapsed()==0——UI 曾以 startedAtMonoMs > 0 作"有起点"哨兵，
    // 0 被判"未注入"。曾以 +1 偏置在时钟域兜底（时钟契约 "ms since app
    // start" 被整体平移 1ms，任何混用他钟的消费方继承偏斜）。修正落于
    // 消费侧：itemFor 注入显式 hasStartedAt 标记，时钟恢复诚实值
    // （相减双方同源，计时差不受影响）。
    return kStart.elapsed();
}
