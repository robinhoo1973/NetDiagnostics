// =============================================================================
// PlatformFlags.h — Authoritative platform bitmask (DIAG-12, NEW-1)
//
// Single platform enum shared by DiagnosticMeta, PlatformAdapter and
// DeviceCapability.  Derived from DiagCapability.h (3-way: Desktop/iOS/Android).
//
// R5 决策记录（全面审查 45 探针的桌面 OS 差异后）：维持三档不拆分。
// 差异归纳：(a) 实现方式差异——Windows IP Helper / Linux /proc / macOS sysctl，
// 由探针内编译期 #if 承载；(b) 实现质量缺口——已补齐（Apple datagram ICMP
// ping/traceroute、Linux raw ICMP + TCP-TTL 回退、macOS netstat 活跃连接）；
// (c) 平台事实差异——macOS 无 systemd-resolved 等，以诚实降级呈现。
// 无任何探针存在“仅某桌面 OS 能跑”的能力性差异 → 位掩码维持能力域语义。
// 若将来出现 Windows 专属深探针，再加 PF_Windows=1<<3（向后兼容）。
// =============================================================================
#pragma once

namespace PlatformFlag {
    enum Flag : unsigned {
        PF_Desktop = 1u << 0,   // Windows / macOS / Linux
        PF_IOS     = 1u << 1,
        PF_Android = 1u << 2,
        PF_Mobile  = PF_IOS | PF_Android,
        PF_All     = PF_Desktop | PF_IOS | PF_Android,
    };
}
