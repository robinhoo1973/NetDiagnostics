// =============================================================================
// IconProvider.h — 单母版运行时精确着色（图标管线 v4，方案 B）
//
// URL 协议: image://icon/<colorHex>/<iconName>?theme=dark|light
//   colorHex  = 请求色（无 #，6 位十六进制）
//   iconName  = ffffff 母版文件名（可带 .svg）
//   theme     = dark|light（决定 accent/soft/fixed 按主题取值）
//
// 尺寸契约（Qt 官方文档 QQuickImageProvider::requestImage）：
//   requestedSize 对应 Image::sourceSize 且已按窗口 devicePixelRatio 放大，
//   返回图像必须严格为该尺寸，不得再乘 dpr、不得设置 devicePixelRatio；
//   *size 设为图像原始尺寸。
//
// 哨兵色槽替换（与 generate-colored-icons.py 同源逻辑）后 QSvgRenderer 渲染：
//   #FFFFFF → 请求色（主色/渐变起点）
//   #AAAAAA → 请求色 HSL 加深 30%（渐变深端）
//   #000000 → 语义强调色（icon-runtime.json accent）
//   #101010 → 第二强调色（json second）
//   #B0000n → 固定多色（json fixedDark/fixedLight 按主题）
//   #777777 → 柔填充（json softDark/softLight 按主题）
//
// 防"切主题图标不刷新/跨主题串图"（历史 5WHY）：渲染缓存按全键
// (name,color,theme,size) LRU；URL 即失效键——AppIcon 切主题后 URL 变化，
// Qt 图缓存与 provider 缓存同时 miss，强制重渲染。
// =============================================================================
#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>
#include <QStringList>

class IconProvider : public QQuickImageProvider {
public:
    explicit IconProvider();

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    struct Meta {
        QString accent;      // #000000 语义强调（空=保留字面黑；回退槽）
        // 5WHY (复核 2026-08-21 双主题徽章): accent 曾单一值两主题共用——
        // dark 无法高亮黄色徽章（wifi-info 圆圈 i）。accentDark/accentLight
        // 按主题分派（优先）；空时回退 accent。
        QString accentDark;  // #000000 语义强调（dark 主题）
        QString accentLight; // #000000 语义强调（light 主题）
        QString second;      // #101010 第二强调
        QString softDark;    // #777777 柔填充（暗）
        QString softLight;   // #777777 柔填充（亮）
        QStringList fixedDark;   // #B0000n（暗）
        QStringList fixedLight;  // #B0000n（亮）
    };

    QByteArray masterXml(const QString& name);
    QByteArray tintedXml(const QString& name, const Meta& meta,
                         const QColor& primary, bool dark);
    const Meta* metaFor(const QString& name) const;
    void loadMeta();
    static QColor darken30(const QColor& c);

    // 缓存（互斥锁保护；requestImage 可能运行在辅助线程）
    mutable QMutex m_mutex;
    QHash<QString, QByteArray> m_xmlCache;      // 母版 XML（按名）
    QHash<QString, Meta> m_meta;                // 运行时元数据（启动加载）
    QHash<QString, QImage> m_renderCache;       // 渲染 LRU（全键）
    QStringList m_lruOrder;
    static const int kMaxCache = 256;
};
