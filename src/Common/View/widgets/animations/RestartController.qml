// RestartController.qml — 无限循环动画的命令式 restart/stop 契约
// （GeoLocate/WifiWave 共用）
//
// 5WHY (复核 2026-08-20 复用): 两个动画各自复制"创建即真 + 重启从 0
// 相位 + 停止复位"惯用法——曾因照抄时把 onStopped 挂错宿主（Item 无
// stopped 信号）导致 WifiWave 组件编译失败、动画整体不出现。收敛为
// 共享控制器：running 变化时对 target 动画 restart()/stop()，停止时
// 发 stopped 信号供调用方复位本动画视觉（目标式动画写入断绑，必须
// 显式复位）。
//
// 5WHY (复核 2026-08-19 创建即真): 属性变更处理器不响应创建期初值——
// 以 running:true 直接实例化（各动画 Usage 注释的用法）时 restart
// 永不触发。onCompleted 兜底补一次启动判定。
//
// 5WHY (复核 2026-08-19 相位错乱): 不用声明式 running 绑定——Qt 的
// stop() 保留 currentTime，重新 start() 会从中途续播。
//
// Usage: RestartController { running: root.running; target: seq; onStopped: ... }
import QtQuick

Item {
    id: root
    property var target: null       // SequentialAnimation（loops: Animation.Infinite）
    property bool running: false
    signal stopped()

    Component.onCompleted: if (root.running && root.target) root.target.restart()
    onRunningChanged: {
        if (!root.target) return
        if (root.running) root.target.restart()
        else {
            root.target.stop()
            root.stopped()
        }
    }
}
