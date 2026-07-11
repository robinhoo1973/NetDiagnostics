// =============================================================================
// GenericTask.h — Wrap any existing diagnostic function as a DiagnosticTask
// =============================================================================
#pragma once

#include "Common/Services/DiagnosticTask.h"
#include <functional>

class GenericTask : public DiagnosticTask {
public:
    using Impl = std::function<DiagnosticResult(DiagId, const QString&)>;

    GenericTask(DiagId id, const QString& target, Impl impl,
                int timeoutMs = 60000, QObject* parent = nullptr)
        : DiagnosticTask(id, target, timeoutMs, parent), m_impl(std::move(impl)) {}

protected:
    DiagnosticResult run() override { return m_impl(diagId(), target()); }

private:
    Impl m_impl;
};
