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
    return kStart.elapsed();
}
