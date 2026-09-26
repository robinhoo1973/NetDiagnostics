# 多视角代码评审委员会 — 检测功能/详情页全面评审报告

> 日期：2026-09-27 ｜ 委员会：Qt/C++ 开发、架构师、测试工程师、UI/UX 设计师、网络诊断领域专家（第一轮独立取证）+ 开发/架构双对抗质询（第二轮）
> 范围：44 探针实现逻辑 + 详情页 UI/UX 与内容输出
> 第一轮 45 条发现 → 交叉质询裁决：高置信 19 / 条件 14 / 否决或搁置 7

## 结论摘要

详情页的信息架构（八段顺序、折叠策略、7 状态三重编码、空态/错误态、契约门控）经多轮 5WHY 打磨质量高；探针侧"失败不得伪装成功"文化贯彻到位（多处哨兵/诚实脚注）。主要缺口集中在三类：
1. **误导性健康结论**（最高风险）：全 DNS 故障报「DNS CLEAN Pass 100 分」、Windows G4 DNS 恒测阿里 DNS、MTU 探测失败仍报「无分片预期」、inconclusive 渲染 80/100 绿、traceroute 把 ICMP 过滤的健康网络挂黄。
2. **CI 闸门形同虚设**：selftest 断言面只有一项、探针删剩 1 个仍绿、注册缺口 fail-fast 被 ND_RELEASE 编译掉、pre-commit 30 项不进 CI。
3. **语义呈现断链**：Warning 塌缩为红色错误卡、severity 下发零消费、指标三源漂移（meta/KeyMetric.js/ResultChart）、G5 协议族零 narrative、summary 不本地化。

推荐修复 15 项分 4 阶段（测试闸门→探针正确性→语义呈现→QML 结构→文档工具），全部为最小修复/渐进演进，不动信息架构。已否决的激进方案：MTU 改 Warning（噪音）、traceroute 改 Info（破坏完成度语义）、U1 改色板值（违背用户冻结决策）、KeyMetric 上移 C++（破坏热迭代）。

## 冲突裁决记录

| 冲突 | 裁决 | 理由 |
|------|------|------|
| U1 对比度 vs 色值冻结 | 派生 token（不动色板值） | 冻结对象是令牌值，缺陷在使用层；onSuccessContainer 先例 |
| D6 traceroute Warning→Info | 保持 Warning 改文案 | severity 语义=探测达成度，ICMP 过滤=未达成 |
| D8 采集型 Pass→Info | 改 Info 但与 severity 呈现同轮 | 语义过载修正需呈现承接防认知冲击 |
| A1 KeyMetric 上移 C++ vs 保留 JS | 保留 JS + 机器对账 | QML 热迭代与展示关注点归属；代码生成下轮 |

## 实施清单（详见提交）

- Phase 0 测试闸门：selftest 硬化（状态打印/预期总量/verifyOk 入退出码/ND_TESTING fail-fast）、pre-commit 进 CI、删孤儿脚本
- Phase 1 探针正确性：Phase1 超时分类、tcpProbe 相位旗标、HTTP 响应体上限、trNarrative 单遍替换、traceroute/MTU 披露文案、inconclusive 呈现、Windows DNS 枚举复用
- Phase 2 语义呈现：severity 视觉消费（错误卡分级配色+属性行严重度）、采集型 Info、scaffold Pass 默认结论
- Phase 3 QML 结构：DetailSections 单一装配、状态词对比度派生、折叠头 44px+键盘
- Phase 4 文档工具：契约文档刷新、--dump-contract + KeyMetric 对账脚本

## 条件/下轮候选

C3 评分重设计（需真实网络样本校准）、D2 SSL 信任展示层、D3 summary 本地化（机制+试点）、C7 DHCP 门控补证、T4 边界用例（ci-http-server.py 入 CI）、A3 标签机器键解耦、U3 宽度钳制（需视觉回归）。

## 否决记录（防重复提）

A6 空详情页（表述过时，banner 空已 Warning；残留并入 G5 narrative 补全）、A7 durationMs 双用（半过时，不同表面有意设计）、U5 魔法字号 lint（投入>收益）、U7 双层镀铬（美学低影响）、T6 文档全量机器对账（先手动刷新，自动化并入 A1 脚本二期）。
