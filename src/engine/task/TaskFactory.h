// =============================================================================
// TaskFactory.h — Maps DiagId → DiagnosticTask (replaces ControllerFactory)
// =============================================================================
#pragma once

#include "engine/task/GenericTask.h"
#include <memory>

class TaskFactory {
public:
    // Create a task for the given diagnostic ID.
    static std::unique_ptr<DiagnosticTask> createTask(
        DiagId id, const QString& target = {});
};
