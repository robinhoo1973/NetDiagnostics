# Code Review Checklist

Every PR must pass this checklist. The reviewer signs off on each item.

---

## 1. Correctness

- [ ] **Signal emission**: All Q_PROPERTY setters emit their NOTIFY signal unconditionally (even if downstream methods return early).  
  _5WHY reference: Schema field visibility bug — `assembleTargetUrl()` returned early._  
  Check: `grep -A5 "setTarget\|setTargetScheme\|setTargetPort" src/app/AppState.cpp`

- [ ] **Preprocessor balance**: Every `#if`/`#ifdef`/`#ifndef` has a matching `#endif`. No stray guards from file splits.  
  _5WHY reference: G5 file bifurcation produced 4 `#endif` without matching `#if`._  
  Check: `grep -c '#if\|#ifdef\|#ifndef' file.cpp` == `grep -c '#endif' file.cpp`

- [ ] **Namespace closure**: Headers that open a namespace MUST close it, OR document that `.cpp` files are responsible.  
  _5WHY reference: G5Common.h opened `namespace G5WebsiteUrl` but never closed it, polluting AppState.cpp._  
  Check: Every `namespace X {` has a matching `} // namespace X`

- [ ] **Inline functions in headers**: Functions defined in `.h` must be `inline` or `static` to avoid multiple-definition linker errors.  
  _5WHY reference: G5 `defaultPortForScheme`/`knownSchemes`/`validate`/`portForUrl` produced linker errors._  
  Check: `grep -n "^[a-zA-Z].*(.*) {" headers/*.h`

- [ ] **QML property reactivity**: QML bindings that reference C++ properties re-evaluate correctly. Test with `console.warn()` in `Component.onCompleted`.  
  _5WHY reference: DiagnosticToolbar debug logging confirmed component loading._  
  Check: `Component.onCompleted: console.warn("[Component] loaded")`

## 2. Code Isolation

- [ ] **ND_TESTING guards**: All test/debug code is inside `#ifdef ND_TESTING` — `#endif` blocks. No test code leaks into release builds.  
  Check: Default build (no `-DND_TESTING`) compiles without test sources.

- [ ] **Convenience macros**: Use `TH_LOG_INFO()`, `TH_LOG_ERROR()`, `TH_LOG_STEP()` instead of direct `TestHarness::instance()` calls in production code.  
  Check: Production files use `TH_*` macros, not `#include "testing/TestHarness.h"`.

- [ ] **No QProcess in tests**: Headless tests call AppState API directly, never shell out.  
  Check: `grep QProcess src/testing/`

## 3. Test Coverage

- [ ] **New feature has a test scenario** in `TestScenarios.cpp`.  
  Check: `grep "FeatureName" src/testing/TestScenarios.cpp`

- [ ] **Bug fix has a 5WHY analysis** in the commit message or `docs/5WHY-TEMPLATE.md`.  
  Check: `git log -1 --format=%B | grep "Why #1"`

- [ ] **CI scenarios included**: New test is in `ciScenarios()` or `coreTargets()` for GitHub Actions.  
  Check: `grep "NewFeature" src/testing/TestScenarios.cpp`

- [ ] **Test passes headless**: `./net_diagnostics --test` returns exit code 0.  
  Check: CI pipeline step `- run: ./dist/netdiag-win-x86_64.exe --test`

## 4. GitHub Actions Compatibility

- [ ] **Headless**: Test does not require GUI or user interaction.  
- [ ] **Timeout-safe**: Each test scenario completes within 120 seconds.  
- [ ] **Network-tolerant**: Test targets are publicly accessible (httpbin.org, github.com).  
- [ ] **Exit code**: `--test` returns 0 on pass, 1 on fail (CI can detect).

## 5. Documentation

- [ ] **5WHY analysis committed** for every bug fix.  
- [ ] **CHANGELOG entry** for user-visible changes.  
- [ ] **API changes documented** in header comments.

---

## Sign-off

| Reviewer | Date | Approved |
|----------|------|----------|
| | | [ ] |
