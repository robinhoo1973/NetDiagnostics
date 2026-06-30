// =============================================================================
// TaskFactory.h — Maps DiagId → DiagnosticTask (replaces ControllerFactory)
// =============================================================================
#pragma once

#include "engine/task/GenericTask.h"
#include <memory>

class TaskFactory {
public:
    // Create a task for the given diagnostic ID.
    // Port scan params only used for G4PortScan (ignored otherwise).
    static std::unique_ptr<DiagnosticTask> createTask(
        DiagId id, const QString& target = {},
        int fromPort = 0, int toPort = 0, bool useCommonPorts = true);
};
