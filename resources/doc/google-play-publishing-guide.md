# Google Play 发布指南（NetDiagnostics）

> 面向 **NetDiagnostics**（包名 `com.netdiagnostic.app`，Qt 6.5.3 / minSdk 26 / targetSdk 33）的 Google Play 上架 + IAP 注册完整流程。
> 配套 CI：`.github/workflows/GooglePlay.yml`（镜像 `iOS.yml`：`version → android(AAB) → deploy-google-play`）。

---

## 0. 项目现状速览（重要）

| 事项 | 状态 | 说明 |
|------|------|------|
| Android 构建 | ✅ 已有 | `build.yml` 产出 **APK**（arm64 / x86_64） |
| Play 上传产物 | ⚠️ 需 AAB | Google Play 2021-08 起**新应用只接受 App Bundle（AAB）**；`GooglePlay.yml` + `.github/actions/android-aab` 已补齐 |
| 签名 | ✅ 已有 | `ANDROID_KEYSTORE_*` 持久密钥（`scripts/android-setup-release-keystore.sh` 生成），同一把 key 用于 APK 与 AAB |
| 版本号 | ✅ 已有 | versionCode = `(M*1e6 + m*1e3 + p)*10 + (1 预发布 / 2 正式)`，由 `netdiag-target.cmake` 生成清单 |
| **Google Play Billing（IAP）** | ❌ **未实现** | `PremiumStore::supportsIap()` 在 Android 返回 `false`；清单无 `com.android.vending.BILLING` 权限。**上架可不用 IAP，但要卖 Premium 必须先做客户端实现**（见第 6 节） |
| 隐私政策 | ✅ 已有 | `docs/privacy-policy.html` |

---

## 1. 注册开发者账号（一次性）

1. 访问 [Google Play Console](https://play.google.com/console) → 用 Google 账号登录。
2. **注册开发者账号**：付费 $25（一次性，不能退款）。填写开发者名称、联系方式。
3. **身份验证**（2023 后强制）：提交 D-U-N-S 编号（可用免费 D-U-N-S 申请）、身份证/营业执照等，验证通过前无法发布到 production。
4. 个人账号可先走测试轨道（internal / closed），正式上架仍需完成验证。

> 一个账号可管理多个应用；换账号发布会丢失关联，务必用**最终运营方**的账号注册。

---

## 2. 准备应用（发布前必须完成）

### 2.1 签名与 Play App Signing

Google Play 采用 **Play App Signing** 双层密钥：

- **上传密钥（upload key）**：你用来签 AAB 的 key —— 即仓库里的 `ANDROID_KEYSTORE_*`（`netdiag-release.keystore`，alias 默认 `netdiag`）。**必须永久保存**，丢失后需走复杂的密钥重置流程。
- **应用签名密钥（app signing key）**：由 Google 生成并保管，用于给用户设备分发 APK。

Play Console 首次上传时会提示“Let Google manage and protect your app signing key”（默认推荐，选这个）。

### 2.2 生成/确认上传密钥

```bash
bash scripts/android-setup-release-keystore.sh
# 输出：netdiag-release.keystore + 四个 base64/密码，配置到 GitHub Secrets：
#   ANDROID_KEYSTORE_B64 / ANDROID_KEYSTORE_PASS / ANDROID_KEY_ALIAS / ANDROID_KEY_PASS
```

> 与 `build.yml` 的 `android-apk` 用**同一把 key**，保证测试期 APK 升级到正式 AAB 时签名一致。

### 2.3 版本号

无需手动处理：打 `v0.0.4`（或 `v0.0.4-rc1`）tag 后，`version.yml` 检测版本，`netdiag-target.cmake` 生成：

- versionName = `0.0.4`（预发布为 `0.0.4-rc1`）
- versionCode = `(M*1e6 + m*1e3 + p)*10 + (1 预发布 / 2 正式)`，如 `0.0.4-rc1 → 41`，`0.0.4 → 42`

Play 要求同一条 track 上 versionCode **唯一且递增**，预发布 < 正式版的方案保证正式版能覆盖自己的 RC。

### 2.4 隐私政策

- 已提供 `docs/privacy-policy.html`，**必须部署到公网可访问的 URL**（GitHub Pages / 自有站点）。
- Play Console 的 “App content → Privacy policy” 填写该 URL（如果应用收集任何用户数据；不收集也建议填，避免审核歧义）。

### 2.5 商店素材（Store listing）

| 素材 | 要求 | 来源 |
|------|------|------|
| 应用图标 | 512×512 PNG（32-bit） | `resources/icons/netanalysis.png` |
| Feature graphic | 1024×500 | 可用 `scripts/generate-figma-mockups.py` 派生 |
| 截图 | 至少 2 张；手机 5–8 张推荐 | `scripts/generate-phone-screenshots.py` 等 |
| 短描述 | ≤ 80 字符 | 中英各一条 |
| 完整描述 | ≤ 4000 字符 | 功能/诊断项列表（45 项） |
| 类别 | 应用 → 工具 | 建议勾选“无广告” |

> `resources/netanalysis.desktop` 为桌面入口；Android 图标由 `.github/actions/android-apk` / `android-aab` 自动复制为 `ic_launcher.png`。

### 2.6 App content（App 内容）问卷

在 Play Console 应用内逐项填写：

1. **Data safety（数据安全）**：网络诊断工具会发起网络请求；如实声明“不收集个人数据”（或按实际填写）。
2. **Ads（广告）**：无广告 → 声明不包含广告（若未来接入，需更新）。
3. **Content rating（内容分级）**：填问卷（IARC），工具类通常自动得出“Everyone / 3+”。
4. **Target audience（目标受众）**：16 岁以上（无儿童定向）。
5. **News / Government apps**：否。

---

## 3. 在 Play Console 创建应用

1. **All apps → Create app**：
   - 应用名称：`NetDiagnostics`
   - 默认语言：`English (United States)`（可后续加中文）
   - 应用类型：`App`；类别：`Tools`
   - 选择 **Free（免费）** —— IAP 不影响“免费”，只是应用内购买
2. 创建后按左侧导航逐项完成 2.5 / 2.6 的内容。
3. 先在 **Testers → Internal testing（内部测试）** 建轨道，把测试员邮箱加进名单。

---

## 4. 用 `GooglePlay.yml` 上传发布

### 4.1 首次一次性配置

**（1）服务账号（Service Account）—— 让 CI 有权限上传**

1. Play Console → **Setup → API access** → 关联/创建 Google Cloud 项目。
2. 在 [Google Cloud Console](https://console.cloud.google.com/) 该项目下创建 **Service Account**，下载 **JSON 密钥**。
3. 回到 Play Console → API access → 把该服务账号授权给应用，权限勾选：
   - `View app information`（查看应用信息）
   - `Release to production, testing tracks, and pre-launch reports`（发布到各轨道）
4. 把 JSON 密钥内容存入 GitHub：
   - 仓库 **Settings → Environments → 新建 `GooglePlay`** 环境 → 添加 secret `GOOGLE_PLAY_SERVICE_ACCOUNT_JSON`（值 = 整个 JSON 文本）。
   - （若不想用环境隔离，也可直接放 Actions secrets，但推荐环境，与 `iOS.yml` 的 `TestFlight` 环境对称。）

**（2）GitHub Secrets（签名密钥）**

若尚未配置，添加：
- `ANDROID_KEYSTORE_B64` / `ANDROID_KEYSTORE_PASS` / `ANDROID_KEY_ALIAS` / `ANDROID_KEY_PASS`

### 4.2 触发方式

- **方式 A（推荐，先内测）**：仓库 → **Actions → Google Play Build & Deploy → Run workflow**，`track` 选 `internal`（或 `alpha`/`beta`/`production`），`release_status` 保持 `completed`。
- **方式 B（正式发版）**：推送 `v*` tag（如 `v0.0.4`）→ 自动构建并上传到 **internal** track（保守默认；production 请用方式 A 手动选择）。

### 4.3 工作流做了什么

1. `version`：复用 `version.yml` 从最近 `v*` tag 解析 `M.m.p` 与预发布标记。
2. `android`：arm64-v8a 交叉编译 → `cmake --build build --target aab`（Gradle bundleRelease）→ `apksigner` 用上传密钥签名（**不做 zipalign**，AAB 不允许对齐）→ 上传 AAB 构件。
3. `deploy-google-play`：`r0adkll/upload-google-play@v1` 用服务账号把 AAB 传到指定 track；`versionCode` 由 AAB 自带（工作流里做了交叉校验打印）；同 commit 去重，避免重复上传。

### 4.4 上传后

- 每个轨道上传后为 **draft** 状态 → 在 Play Console **Review release** 页面确认 → “Release to … track” → 等待审核（internal 通常几分钟到几小时）。
- internal 轨道测试员通过 **Google Play 测试链接** 安装。
- 测试通过后：production 轨道上传同版本 → 可**分阶段发布**（staged rollout，如 10% → 100%）。

---

## 5. 发布到生产（Production）

1. 完成第 2、3 节所有内容项（**未完成 App content 问卷无法发布生产**）。
2. 方式 A 手动触发 `GooglePlay.yml`，`track` 选 `production`，或直接在 Console 上传 AAB。
3. **审核**：新应用首次上架通常 1–7 天；工具类通过率较高。被拒时 Console 会给出原因，按提示修改后重新提交。
4. 发布后可在 **Release overview** 监控：安装量、崩溃（Android vitals）、评分。

---

## 6. 注册 IAP（Google Play Billing）

> ⚠️ **关键前提**：Play Console 里“注册”产品只完成商品配置；**客户端必须实现 Google Play Billing Library**，否则用户点了购买也无法完成（Google 审核会拒绝“无法完成的购买 UI”）。
> 当前 `PremiumStore` 的 Android 后端标记为 `future work`（`supportsIap()` 在 Android 返回 `false`，见 `src/Settings/Model/PremiumStore.cpp`）。**本项目 Premium 是一次性买断（非消耗型）**，与 iOS StoreKit 的 `non-consumable` 对应。

### 6.1 Play Console 侧：创建商品

1. **Monetize → Products → In-app products → Create product**。
2. 填写：
   - **Product ID**：`premium_unlock`（一经创建**不可修改**；后续代码用）
   - 名称：`NetDiagnostics Premium`；描述：一次性买断、永久解锁全部功能
   - **Status：Active**
   - 定价：选择国家/地区并设置价格（如 $4.99 / 对应本地价）
   - 类型：**一次性购买（one-time purchase / managed product）** —— 非消耗型（非消耗型商品由 Play 自动“恢复”到已购账号）
3. 保存后状态为 **Draft** → 需在测试轨道发布过应用后，产品才能进入 Active。

### 6.2 测试（License testing + 测试轨道）

1. **Monetize → In-app products → 该产品 → Testers**：添加测试账号（邮箱）。
2. 该测试账号必须加入 **internal / closed 轨道**测试员名单，并用自己的 Google 账号接受测试链接。
3. 测试时可先不真实扣款（Licensed test 响应），验证购买/恢复流程；正式扣款在测试轨道同样支持（会自动退款周期按 Google 规则）。
4. **恢复购买**：卸载重装后，用 Play Billing `queryPurchases()` 恢复非消耗型商品 —— 对应 `PremiumStore::restorePurchases()` 的 Android 实现。

### 6.3 客户端实现（待做清单，按 `PlatformStore.h` 抽象对接）

1. **清单**：`resources/android/AndroidManifest.xml.in` 增加
   ```xml
   <uses-permission android:name="com.android.vending.BILLING"/>
   ```
   （注意：该文件是模板，由 CMake 生成真实清单，改模板即可。）
2. **Billing 依赖**：Google Play Billing Library（`com.android.billingclient:billing`，建议 6.x）加入 gradle 依赖；参照 `androidx-core.jar` 的本地 jar 方案（`resources/android/libs/`）。
3. **实现后端**：新建 `src/Common/Platform/Android/PlatformStoreAndroid.cpp`，实现 `platformInitStore` / `platformStartPurchase` / `platformRestorePurchases`（JNI 调 `BillingClient`），并把 `PremiumStore::supportsIap()` 在 `PLATFORM_ANDROID` 下返回 `true`。
4. **非消耗型商品**：`premium_unlock` 用 `queryPurchases()`（非消耗），购买成功回调里 `setPremium(true)`；启动时 `queryPurchases` 自动恢复已购（对齐 iOS 的 unattended-grant 路径）。
5. **生产审核注意**：商品必须与已上架（至少 internal）的版本对应；未实现客户端前不要把“购买”按钮暴露给 Android 用户。

### 6.4 订阅（本项目不需要）

如需订阅，用 **Monetize → Subscriptions**，设置基础方案/优惠/试用期。NetDiagnostics 是“一次性买断 → 永久解锁”，**不要用订阅**。

---

## 7. 常见坑（对齐项目 5WHY 惯例）

| 坑 | 说明 |
|----|------|
| 上传 APK 到生产被拒 | 新应用必须 AAB；用 `GooglePlay.yml` 的 `android-aab` 产物 |
| 升级失败 `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | 上传密钥换了；所有轨道/所有版本必须同一把 `ANDROID_KEYSTORE_*` key |
| versionCode 重复 | 同 track 必须唯一递增；预发布用 `-rcN` tag 让 versionCode 低于正式版 |
| Billing 权限缺失 | Android 端购买流程不工作/审核拒绝 —— 必须加 `BILLING` 权限 + Billing Library |
| `supportsIap()` 为 false | Android 上 Buy/Restore 按钮被隐藏（`PremiumDialog.qml` 按此门控），属预期；实现 Billing 后改为 true |
| 隐私政策 URL 失效 | 被拒常见原因；确保 `docs/privacy-policy.html` 有可访问的公网地址 |
| 服务账号无权限 | CI 上传 403 —— 在 Play Console API access 里给服务账号授权对应轨道权限 |
| AAB 被 zipalign | 会导致上传报错；`android-aab` action 特意不 zipalign |

---

## 8. 发布检查清单（Release checklist）

- [ ] Play Console 开发者账号 + 身份验证完成（$25）
- [ ] 隐私政策 URL 可访问
- [ ] `ANDROID_KEYSTORE_*` 四件套已入 GitHub Secrets（并已安全备份 keystore）
- [ ] `GooglePlay` 环境 + `GOOGLE_PLAY_SERVICE_ACCOUNT_JSON` 已配置
- [ ] Store listing 素材齐全（图标/Feature graphic/截图/描述，中英）
- [ ] App content 问卷完成（数据安全/广告/分级/受众）
- [ ] `GooglePlay.yml` 手动触发一次 → internal track 成功
- [ ] internal 测试员安装验证（含 IAP 购买/恢复，若已实现）
- [ ] 切 production，首次审核通过，分阶段发布
