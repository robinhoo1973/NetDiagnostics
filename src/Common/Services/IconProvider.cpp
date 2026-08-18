// =============================================================================
// IconProvider.cpp — 见 IconProvider.h
// =============================================================================

#include "IconProvider.h"

#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRectF>
#include <QSvgRenderer>
#include <QtGlobal>

namespace {
constexpr int kDefaultSize = 24;
constexpr int kMaxRenderSide = 1024;

QString stripHash(QString hex)
{
    hex = hex.trimmed();
    if (hex.startsWith(QLatin1Char('#')))
        hex.remove(0, 1);
    return hex.toUpper();
}

// 归一化 JSON 颜色值：保证 #RRGGBB 大写（SVG 颜色必须带 # 才合法）
QString normalizeColor(QString hex)
{
    hex = hex.trimmed();
    if (!hex.startsWith(QLatin1Char('#')))
        hex.prepend(QLatin1Char('#'));
    return hex.toUpper();
}

bool parseColor(const QString& hex6, QColor* out)
{
    QString h = stripHash(hex6);
    if (h.size() != 6)
        return false;
    bool ok = false;
    const int v = h.toInt(&ok, 16);
    if (!ok)
        return false;
    *out = QColor::fromRgb((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
    return true;
}

// 键值查询串解析（theme=dark|light&dpr=2）
QHash<QString, QString> parseQuery(const QString& query)
{
    QHash<QString, QString> kv;
    const QStringList pairs = query.split(QLatin1Char('&'), Qt::SkipEmptyParts);
    for (const QString& pair : pairs) {
        const int eq = pair.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        kv.insert(pair.left(eq), pair.mid(eq + 1));
    }
    return kv;
}
} // namespace

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
    loadMeta();
}

void IconProvider::loadMeta()
{
    QFile f(QStringLiteral(":/icon-runtime.json"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    // 5WHY (2026-08-18, 用户反馈"图标颜色大面积错误"): icons 是"按名称键控的
    // 对象"（生成器 write_icon_runtime 输出 {"icons": {name: {...}}}），
    // 此前用 toArray() 解析对象恒为空 → 全部 accent/second/soft/fixed 回退
    // 字面哨兵（#000000 黑、#B0000n 暗红）→ 所有多色图标配色全错。
    const QJsonObject icons = root.value(QLatin1String("icons")).toObject();
    for (auto it = icons.constBegin(); it != icons.constEnd(); ++it) {
        const QJsonObject o = it.value().toObject();
        const QString name = it.key();
        Meta m;
        m.accent = o.value(QLatin1String("accent")).toString();
        m.second = o.value(QLatin1String("second")).toString();
        m.softDark = o.value(QLatin1String("softDark")).toString();
        m.softLight = o.value(QLatin1String("softLight")).toString();
        const QJsonArray fd = o.value(QLatin1String("fixedDark")).toArray();
        const QJsonArray fl = o.value(QLatin1String("fixedLight")).toArray();
        for (const QJsonValue& c : fd)
            m.fixedDark.append(c.toString());
        for (const QJsonValue& c : fl)
            m.fixedLight.append(c.toString());
        m_meta.insert(name, m);
    }
}

QByteArray IconProvider::masterXml(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_xmlCache.constFind(name);
    if (it != m_xmlCache.constEnd())
        return it.value();

    QString fileName = name;
    if (!fileName.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
        fileName += QLatin1String(".svg");
    QFile f(QStringLiteral(":/icons/master/") + fileName);
    QByteArray xml;
    if (f.open(QIODevice::ReadOnly))
        xml = f.readAll();
    m_xmlCache.insert(name, xml);
    return xml;
}

const IconProvider::Meta* IconProvider::metaFor(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_meta.constFind(name);
    return it == m_meta.constEnd() ? nullptr : &it.value();
}

QColor IconProvider::darken30(const QColor& c)
{
    // generate-colored-icons.py darken_hex 的移植：HSL 亮度 × 0.7，下限 0.05
    float h = 0, s = 0, l = 0, a = 1.0f;
    c.getHslF(&h, &s, &l, &a);
    l = qMax<float>(0.05f, l * 0.7f);
    return QColor::fromHslF(h, s, l, a);
}

QByteArray IconProvider::tintedXml(const QString& name, const Meta& meta,
                                   const QColor& primary, bool dark)
{
    QByteArray xml = masterXml(name);
    if (xml.isEmpty())
        return xml;

    const QByteArray primaryHex = primary.name().toUpper().toLatin1();

    // 1) 主色：渐变起点 #FFFFFF（母版统一为小写，兼容大写以防万一）
    xml.replace("#ffffff", primaryHex);
    xml.replace("#FFFFFF", primaryHex);

    // 2) 渐变深端：HSL 加深 30%
    const QByteArray darkHex = darken30(primary).name().toUpper().toLatin1();
    xml.replace("#aaaaaa", darkHex);
    xml.replace("#AAAAAA", darkHex);

    // 3) 语义强调 #000000（json accent 非空才替换；缺元数据时保持字面黑=确定回退）
    // 5WHY (2026-08-18, loadMeta 修复后暴露): stripHash 去掉 # 前缀会生成
    // 非法 SVG 颜色（fill="38BDF8"）→ 该部分整体不渲染。替换值必须保留 #。
    if (!meta.accent.isEmpty()) {
        const QByteArray accentHex = normalizeColor(meta.accent).toLatin1();
        xml.replace("#000000", accentHex);
    }

    // 4) 第二强调 #101010
    if (!meta.second.isEmpty()) {
        const QByteArray secondHex = normalizeColor(meta.second).toLatin1();
        xml.replace("#101010", secondHex);
    }

    // 5) 柔填充 #777777（按主题）
    const QString soft = dark ? meta.softDark : meta.softLight;
    if (!soft.isEmpty()) {
        const QByteArray softHex = normalizeColor(soft).toLatin1();
        xml.replace("#777777", softHex);
    }

    // 6) 固定多色 #B0000n（按主题；列表长度即槽位数量）
    const QStringList fixed = dark ? meta.fixedDark : meta.fixedLight;
    for (int i = 0; i < fixed.size(); ++i) {
        const QString slot = QStringLiteral("#B0000%1").arg(i + 1);
        const QByteArray slotHex = normalizeColor(fixed.at(i)).toLatin1();
        xml.replace(slot.toLatin1(), slotHex);
    }

    return xml;
}

QImage IconProvider::requestImage(const QString& id, QSize* size,
                                  const QSize& requestedSize)
{
    // id = "<colorHex>/<name>[?theme=..&dpr=..]"
    QString path = id;
    QString query;
    const int qpos = id.indexOf(QLatin1Char('?'));
    if (qpos >= 0) {
        path = id.left(qpos);
        query = id.mid(qpos + 1);
    }
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        if (size)
            *size = QSize(kDefaultSize, kDefaultSize);
        return QImage();
    }
    const QString colorHex = parts.at(0);
    const QString name = parts.at(1);

    QColor primary;
    if (!parseColor(colorHex, &primary))
        primary = Qt::black; // 非法色 → 确定回退（黑）

    const QHash<QString, QString> kv = parseQuery(query);
    const bool dark = kv.value(QLatin1String("theme"), QLatin1String("dark"))
                          != QLatin1String("light");

    // Qt 官方契约（QQuickImageProvider::requestImage, doc.qt.io/qt-6）：
    // "The requestedSize corresponds to the Image::sourceSize... If
    //  requestedSize is a valid size, the image returned should be of that
    //  size." —— Qt 传入的 requestedSize 已按窗口 devicePixelRatio 放大
    // （实测 14 逻辑 px @1.875 → req=26）。再乘 dpr 会 2 倍超采样，且
    // 返回图像设置 devicePixelRatio 会干扰 Qt 对尺寸的归一化。
    // 正确实现：严格按 requestedSize 渲染，不设置 devicePixelRatio。
    int w = requestedSize.width() > 0 ? requestedSize.width() : kDefaultSize;
    int h = requestedSize.height() > 0 ? requestedSize.height() : kDefaultSize;
    w = qBound(1, w, kMaxRenderSide);
    h = qBound(1, h, kMaxRenderSide);
    const int rw = w;
    const int rh = h;

    // 全键渲染缓存：URL（color/theme）即失效键 + 尺寸 → 跨主题/跨尺寸不串图
    // （Qt 引擎自身按 URL+size 缓存，本 LRU 是对重复渲染的二次防护）
    const QString key = QStringLiteral("%1|%2|%3|%4x%5")
                            .arg(name, colorHex, dark ? QStringLiteral("d")
                                                      : QStringLiteral("l"))
                            .arg(rw)
                            .arg(rh);
    {
        QMutexLocker locker(&m_mutex);
        const auto hit = m_renderCache.constFind(key);
        if (hit != m_renderCache.constEnd()) {
            const QImage img = hit.value();
            if (size)
                *size = img.size();
            return img;
        }
    }

    const Meta* meta = metaFor(name);
    const Meta fallback; // 缺元数据 → 空表：主色/渐变/字面黑，确定回退
    const QByteArray xml = tintedXml(name, meta ? *meta : fallback, primary, dark);

    QImage img(rw, rh, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    if (!xml.isEmpty()) {
        QSvgRenderer renderer(xml);
        QPainter p(&img);
        if (renderer.isValid())
            // 显式目标矩形：确保 viewBox 缩放填充整个请求尺寸
            renderer.render(&p, QRectF(0, 0, rw, rh));
        p.end();
    }
    if (size)
        *size = img.size();

    // LRU 插入（锁内替换）
    {
        QMutexLocker locker(&m_mutex);
        if (m_renderCache.contains(key)) {
            m_lruOrder.removeAll(key);
        } else if (m_renderCache.size() >= kMaxCache) {
            const QString oldest = m_lruOrder.takeFirst();
            m_renderCache.remove(oldest);
        }
        m_renderCache.insert(key, img);
        m_lruOrder.append(key);
    }
    return img;
}
