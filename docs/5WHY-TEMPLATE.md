# 5WHY Root Cause Analysis Template

Every bug fix and feature change MUST include a 5WHY analysis. Use this template.

---

## 5WHY Analysis: [Brief Title]

### Problem Statement
_What is the observed symptom? Be specific: include error messages, screenshots, or log excerpts._

> Example: Schema-aware fields (port/user/pass) do not appear when changing URL scheme in the ComboBox.

---

### Root Cause Chain

| Level | Question | Answer |
|-------|----------|--------|
| **Why #1** | Why does the symptom occur? | |
| **Why #2** | Why does #1 happen? | |
| **Why #3** | Why does #2 happen? | |
| **Why #4** | Why does #3 happen? | |
| **Why #5** | Why does #4 happen? (root cause) | |

### Root Cause (Why #5)
_A single sentence naming the fundamental defect._

---

### Fix
_What was changed? Include file paths and line numbers._

| File | Line(s) | Change |
|------|---------|--------|
| | | |

### Verification
_How was the fix verified? What tests confirm it works?_

- [ ] Build passes with `-DND_TESTING=ON`
- [ ] `--test` mode passes the relevant scenario
- [ ] Manual verification steps: _______________

### Prevention
_What process or tooling change prevents this class of bug from recurring?_

---

## Example: Schema Field Visibility

### Problem
Schema-aware fields (port/user/pass) do not react when scheme changes in ComboBox.

### Root Cause Chain

| Level | Question | Answer |
|-------|----------|--------|
| **Why #1** | Why don't fields appear when scheme changes? | QML bindings `_showUser`/`_showPass` never re-evaluate |
| **Why #2** | Why don't bindings re-evaluate? | `appState.targetScheme` changes but no NOTIFY signal fires |
| **Why #3** | Why is no NOTIFY signal emitted? | `setTargetScheme()` only calls `assembleTargetUrl()` — no explicit `emit targetChanged()` |
| **Why #4** | Why does `assembleTargetUrl()` not emit the signal? | It returns early when `m_targetHost.isEmpty()` |
| **Why #5** | Why does it return early? | **Root cause**: Setters delegated signal emission to `assembleTargetUrl()` which silently bails when host is empty |

### Fix
| File | Line(s) | Change |
|------|---------|--------|
| `src/app/AppState.cpp` | 170-219 | All 6 setters now emit `targetChanged()` + `bumpVersion()` unconditionally |

### Verification
- [x] Build passes with `-DND_TESTING=ON`
- [x] Manual: select "ftp" scheme → user/pass fields appear immediately
- [x] Manual: select "https" scheme → user/pass fields hide

### Prevention
- Code review checklist: all setters must emit NOTIFY signal unconditionally
- Automated test: `TestScenarios::uiSimulation()` test case #3 verifies field visibility
