// ── Translation singleton — accessed as Tr.* in any QML file ──────────
pragma Singleton
import QtQuick

Item {
    // 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW
    // Bind directly to C++ property — NOTIFY signal now reliable on all platforms
    property int lang: appState ? appState.languageIndex : 0

    function t(en, fr, de, ru, it, zh_cn, zh_tw) {
        var a = [en, fr, de, ru, it, zh_cn, zh_tw]
        var i = lang
        return (i >= 0 && i < a.length && a[i]) ? a[i] : en
    }

    // ── Group names (index 0-4 → G1-G5) ──
    function groupName(idx) {
        var d = lang
        var names = [
            t("System & Adapters","Système et adaptateurs","System & Adapter","Система и адаптеры","Sistema e schede","系统和适配器","系統和適配器"),
            t("Connectivity & Security","Connectivité et sécurité","Konnektivität & Sicherheit","Подключение и безопасность","Connettività e sicurezza","连接与安全","連線與安全"),
            t("Internet & DNS","Internet et DNS","Internet & DNS","Интернет и DNS","Internet e DNS","互联网与DNS","網際網路與DNS"),
            t("Remote Host","Hôte distant","Remote Host","Удаленный хост","Host remoto","远程主机","遠端主機"),
            t("Website / URL","Site Web / URL","Website / URL","Веб-сайт / URL","Sito Web / URL","网站 / URL","網站 / URL"),
        ]
        return (idx >= 0 && idx < names.length) ? names[idx] : ""
    }

    // ── Nav ──
    readonly property string dashboard: t("Dashboard","Tableau de bord","Dashboard","Панель","Dashboard","仪表板","儀表板")
    readonly property string diagnostics: t("Diagnostics","Diagnostics","Diagnose","Диагностика","Diagnostica","诊断","診斷")
    readonly property string config: t("Config","Configuration","Konfiguration","Конфигурация","Configurazione","配置","配置")
    readonly property string report: t("Report","Rapport","Bericht","Отчёт","Rapporto","报告","報告")
    readonly property string settings: t("Settings","Paramètres","Einstellungen","Настройки","Impostazioni","设置","設定")

    // ── DiagnosticScreen ──
    readonly property string runningDots: t("Running Diagnostics...","Diagnostics en cours...","Diagnose läuft...","Диагностика...","Diagnostica in corso...","正在诊断...","正在診斷...")
    readonly property string complete: t("Diagnostic Complete","Diagnostic terminé","Diagnose abgeschlossen","Диагностика завершена","Diagnostica completata","诊断完成","診斷完成")
    readonly property string cancelled: t("Cancelled","Annulé","Abgebrochen","Отменено","Annullato","已取消","已取消")
    readonly property string errorCheck: t("Error — Check Target","Erreur — Vérifier la cible","Fehler — Ziel prüfen","Ошибка — Проверьте цель","Errore — Controlla target","错误 — 检查目标","錯誤 — 檢查目標")
    readonly property string results: t("Results","Résultats","Ergebnisse","Результаты","Risultati","结果","結果")
    readonly property string diagGroup: t("Diagnosis Group","Groupe de diagnostic","Diagnosegruppe","Группа диагностики","Gruppo diagnostica","诊断组","診斷組")
    readonly property string reset: t("Reset","Réinitialiser","Zurücksetzen","Сброс","Ripristina","重置","重置")
    readonly property string runDiag: t("▶ Run Diagnostics","▶ Lancer diagnostic","▶ Diagnose starten","▶ Запустить","▶ Avvia diagnostica","▶ 运行诊断","▶ 運行診斷")
    readonly property string running: t("⏳ Running...","⏳ En cours...","⏳ Läuft...","⏳ Выполняется...","⏳ In corso...","⏳ 运行中...","⏳ 運行中...")
    readonly property string stop: t("■ Stop","■ Arrêter","■ Stopp","■ Стоп","■ Ferma","■ 停止","■ 停止")
    readonly property string target: t("Target","Cible","Ziel","Цель","Obiettivo","目标","目標")
    readonly property string enterTarget: t("Enter URL, IP address, or hostname...","Entrez URL, IP ou nom d'hôte...","URL, IP oder Hostname eingeben...","Введите URL, IP или имя хоста...","Inserisci URL, IP o hostname...","输入URL、IP地址或主机名...","輸入URL、IP位址或主機名...")
    readonly property string portScan: t("Port Scan","Scan de ports","Port-Scan","Сканирование портов","Scansione porte","端口扫描","端口掃描")
    readonly property string scanCommon: t("Scan Common Ports","Scanner ports communs","Gängige Ports scannen","Сканировать порты","Scansiona porte comuni","扫描常用端口","掃描常用端口")
    readonly property string range: t("Range:","Plage:","Bereich:","Диапазон:","Intervallo:","范围:","範圍:")
    readonly property string fromMust: t("From Must Be ≤ To","De doit être ≤ À","Von muss ≤ Bis sein","От должно быть ≤ До","Da deve essere ≤ A","起始必须 ≤ 结束","起始必須 ≤ 結束")

    // ── ConfigScreen ──
    readonly property string diagConfig: t("Diagnostic Configuration","Configuration diagnostic","Diagnosekonfiguration","Конфигурация диагностики","Configurazione diagnostica","诊断配置","診斷配置")
    readonly property string selectAll: t("Select All","Tout sélectionner","Alle auswählen","Выбрать все","Seleziona tutto","全选","全選")
    readonly property string deselectAll: t("Deselect All","Tout désélectionner","Alle abwählen","Отменить все","Deseleziona tutto","取消全选","取消全選")

    // ── DashboardScreen ──
    readonly property string noData: t("No diagnostic data yet","Aucune donnée de diagnostic","Noch keine Diagnosedaten","Нет данных диагностики","Nessun dato diagnostico","暂无诊断数据","暫無診斷數據")
    readonly property string runFromDiag: t("Run a diagnostic from the Diagnostics screen\nto see results here.","Exécutez un diagnostic depuis l'écran Diagnostics\npour voir les résultats ici.","Führen Sie eine Diagnose vom Diagnosebildschirm aus,\num die Ergebnisse hier zu sehen.","Запустите диагностику на экране диагностики,\nчтобы увидеть результаты здесь.","Esegui una diagnostica dalla schermata Diagnostica\nper vedere i risultati qui.","从诊断屏幕运行诊断\n以在此处查看结果。","從診斷畫面運行診斷\n以在此處查看結果。")
    readonly property string perGroup: t("Per-Group Results","Résultats par groupe","Ergebnisse pro Gruppe","Результаты по группам","Risultati per gruppo","分组结果","分組結果")
    readonly property string summary: t("Summary","Résumé","Zusammenfassung","Сводка","Riepilogo","摘要","摘要")
    readonly property string totalDiags: t("Total Diagnostics","Total diagnostics","Diagnosen insgesamt","Всего диагностик","Diagnostiche totali","总诊断数","總診斷數")
    readonly property string totalTime: t("Total Time","Temps total","Gesamtzeit","Общее время","Tempo totale","总时间","總時間")
    readonly property string completed: t("Completed","Terminé","Abgeschlossen","Завершено","Completato","已完成","已完成")
    readonly property string layerTimings: t("Layer Timings","Chronométrage par couche","Schichtzeiten","Время по слоям","Tempi per livello","层级时间","層級時間")

    // ── SettingsScreen ──
    readonly property string interfaceLang: t("Interface Language","Langue de l'interface","Oberflächensprache","Язык интерфейса","Lingua dell'interfaccia","界面语言","界面語言")
    readonly property string chooseLang: t("Choose the display language for the application.","Choisissez la langue d'affichage de l'application.","Wählen Sie die Anzeigesprache der Anwendung.","Выберите язык отображения приложения.","Scegli la lingua di visualizzazione dell'applicazione.","选择应用程序的显示语言。","選擇應用程式的顯示語言。")
    readonly property string emailSmtp: t("Email (SMTP) Configuration","Configuration email (SMTP)","E-Mail (SMTP) Konfiguration","Настройка email (SMTP)","Configurazione email (SMTP)","电子邮件(SMTP)配置","電子郵件(SMTP)配置")
    readonly property string smtpServer: t("SMTP Server","Serveur SMTP","SMTP-Server","SMTP-сервер","Server SMTP","SMTP服务器","SMTP伺服器")
    readonly property string about: t("About","À propos","Über","О программе","Informazioni","关于","關於")
    readonly property string version: t("Version 1.0.0","Version 1.0.0","Version 1.0.0","Версия 1.0.0","Versione 1.0.0","版本 1.0.0","版本 1.0.0")
    readonly property string aboutDesc: t("A comprehensive cross-platform network diagnostic tool supporting Windows, macOS, Linux, iOS, and Android.",
        "Un outil de diagnostic réseau multiplateforme prenant en charge Windows, macOS, Linux, iOS et Android.",
        "Ein umfassendes plattformübergreifendes Netzwerkdiagnosetool für Windows, macOS, Linux, iOS und Android.",
        "Комплексный кроссплатформенный инструмент сетевой диагностики с поддержкой Windows, macOS, Linux, iOS и Android.",
        "Uno strumento completo di diagnostica di rete multipiattaforma che supporta Windows, macOS, Linux, iOS e Android.",
        "一个全面的跨平台网络诊断工具，支持Windows、macOS、Linux、iOS和Android。","一個全面的跨平台網路診斷工具，支援Windows、macOS、Linux、iOS和Android。")
    readonly property string crossPlat: t("Cross-platform (Windows, macOS, Linux, iOS, Android)","Multiplateforme (Windows, macOS, Linux, iOS, Android)","Plattformübergreifend (Windows, macOS, Linux, iOS, Android)","Кроссплатформенный (Windows, macOS, Linux, iOS, Android)","Multipiattaforma (Windows, macOS, Linux, iOS, Android)","跨平台 (Windows, macOS, Linux, iOS, Android)","跨平台 (Windows, macOS, Linux, iOS, Android)")
    readonly property string realtimeDiag: t("Real-time diagnostic engine","Moteur de diagnostic en temps réel","Echtzeit-Diagnose-Engine","Движок диагностики в реальном времени","Motore di diagnostica in tempo reale","实时诊断引擎","實時診斷引擎")
    readonly property string detailedReport: t("Detailed reporting and export","Rapports détaillés et exportation","Detaillierte Berichte und Export","Подробная отчетность и экспорт","Report dettagliati ed esportazione","详细报告和导出","詳細報告和匯出")
    readonly property string darkTheme: t("Dark theme UI","Interface thème sombre","Dunkles Design","Темная тема","Interfaccia tema scuro","深色主题界面","深色主題介面")
    readonly property string simulatorMode: t("Windows simulator mode","Mode simulateur Windows","Windows-Simulator-Modus","Режим симулятора Windows","Modalità simulatore Windows","Windows模拟器模式","Windows模擬器模式")
    // ── ReportScreen ──
    readonly property string reportPreview: t("Report Preview","Aperçu du rapport","Berichtsvorschau","Предпросмотр отчёта","Anteprima rapporto","报告预览","報告預覽")
    readonly property string reportPlaceholder: t("Report generation and preview will be\nimplemented in Phase 9.",
        "La génération et l'aperçu du rapport seront\nimplémentés dans la phase 9.",
        "Die Berichterstellung und Vorschau wird\nin Phase 9 implementiert.",
        "Создание и предпросмотр отчета будет\nреализовано в фазе 9.",
        "La generazione e l'anteprima del rapporto saranno\nimplementate nella Fase 9.",
        "报告生成和预览将在\n第9阶段实现。","報告生成和預覽將在\n第9階段實現。")
    readonly property string reportFeaturePdf: t("Export to PDF format","Export au format PDF","Export als PDF","Экспорт в PDF","Esporta in formato PDF","导出为PDF格式","匯出為PDF格式")
    readonly property string reportFeatureEmail: t("Share reports via email","Partager par email","Berichte per E-Mail teilen","Поделиться по email","Condividi via email","通过电子邮件分享报告","透過電子郵件分享報告")
    readonly property string reportFeatureHtml: t("HTML report with embedded charts","Rapport HTML avec graphiques","HTML-Bericht mit Diagrammen","HTML отчет с графиками","Report HTML con grafici","带嵌入图表的HTML报告","帶嵌入圖表的HTML報告")
    readonly property string reportFeatureHistory: t("Historical report comparison","Comparaison historique","Historischer Vergleich","Историческое сравнение","Confronto storico","历史报告比较","歷史報告比較")
    readonly property string reportResultsAvailable: t(" results available"," résultats disponibles"," Ergebnisse verfügbar"," результатов"," risultati disponibili"," 个结果可用"," 個結果可用")
    readonly property string reportNoResults: t("No diagnostic results","Aucun résultat de diagnostic","Keine Diagnoseergebnisse","Нет результатов","Nessun risultato","无诊断结果","無診斷結果")

    // ── Target Analysis ──
    readonly property string targetAnalysis: t("Target Analysis","Analyse de la cible","Zielanalyse","Анализ цели","Analisi obiettivo","目标分析","目標分析")
    readonly property string knownPortRef: t("Known Port Reference","Référence des ports connus","Bekannte Ports Referenz","Справочник портов","Riferimento porte note","已知端口参考","已知埠參考")
    readonly property string targetTypeLabel: t("Type    :","Type    :","Typ     :","Тип     :","Tipo    :","类型    :","類型    :")
    readonly property string targetTypeUrl: t("URL","URL","URL","URL","URL","URL","URL")
    readonly property string targetTypeIp: t("Remote Host (IP)","Hôte distant (IP)","Remote Host (IP)","Удаленный хост (IP)","Host remoto (IP)","远程主机(IP)","遠端主機(IP)")
    readonly property string targetTypeHostname: t("Remote Host (Hostname)","Hôte distant (nom)","Remote Host (Hostname)","Удаленный хост (имя)","Host remoto (nome)","远程主机(主机名)","遠端主機(主機名)")

    // ── Live Progress ──
    readonly property string errorPrefix: t("Error: ","Erreur : ","Fehler: ","Ошибка: ","Errore: ","错误: ","錯誤: ")
    readonly property string runningStatus: t("Running","En cours","Läuft","Выполняется","In corso","运行中","運行中")
    readonly property string completeStatus: t("Complete","Terminé","Abgeschlossen","Завершено","Completato","完成","完成")
    readonly property string cancelledStatus: t("Cancelled","Annulé","Abgebrochen","Отменено","Annullato","已取消","已取消")
    readonly property string errorStatus: t("Error","Erreur","Fehler","Ошибка","Errore","错误","錯誤")
    readonly property string readyStatus: t("Ready","Prêt","Bereit","Готов","Pronto","就绪","就緒")

    // ── Dashboard ──
    readonly property string diagRunComplete: t("Diagnostic Run Complete","Diagnostic terminé","Diagnoselauf abgeschlossen","Диагностика завершена","Corsa diagnostica completata","诊断运行完成","診斷運行完成")
    readonly property string targetLabel: t("Target: ","Cible : ","Ziel: ","Цель: ","Obiettivo: ","目标: ","目標: ")
    readonly property string naLabel: t("N/A","N/D","k.A.","Н/Д","N/D","不适用","不適用")

    // ── Summary cards ──
    readonly property string summaryPass: t("Pass","Réussi","Bestanden","Пройден","Superato","通过","通過")
    readonly property string summaryWarning: t("Warning","Avertissement","Warnung","Предупреждение","Avviso","警告","警告")
    readonly property string summaryFail: t("Fail","Échec","Fehlgeschlagen","Неудача","Fallito","失败","失敗")
    readonly property string summarySkipped: t("Skipped","Ignoré","Übersprungen","Пропущено","Saltato","已跳过","已跳過")
    readonly property string summaryInfo: t("Info","Info","Info","Инфо","Info","信息","資訊")

    // ── TestResultItem ──
    readonly property string diagRunning: t("Running...","En cours...","Läuft...","Выполняется...","In corso...","运行中...","運行中...")

    // ── Placeholder ──
    readonly property string placeholderMsg: t("SMTP configuration is a placeholder and will be implemented in a future update.",
        "La configuration SMTP est un espace réservé et sera implémentée dans une prochaine mise à jour.",
        "Die SMTP-Konfiguration ist ein Platzhalter und wird in einem zukünftigen Update implementiert.",
        "Конфигурация SMTP является заполнителем и будет реализована в будущем обновлении.",
        "La configurazione SMTP è un segnaposto e sarà implementata in un futuro aggiornamento.",
        "SMTP配置为占位符，将在未来的更新中实现。","SMTP配置為預留位置，將在未來的更新中實現。")

    // ── Test names (all 38, ids 0-37) ──
    function diagName(id) {
        if (lang <= 0) return ""  // English uses C++ names directly
        var names = {
            0:  t("Network Adapters","Adaptateurs réseau","Netzwerkadapter","Сетевые адаптеры","Schede di rete","网络适配器","網路適配器"),
            1:  t("NIC Advanced","Carte réseau avancée","Erweiterte NIC","NIC расширенный","NIC avanzata","NIC高级","NIC進階"),
            2:  t("WiFi Information","Informations WiFi","WLAN-Information","Информация о WiFi","Informazioni WiFi","WiFi信息","WiFi資訊"),
            3:  t("Wired Information","Informations filaire","Kabelgebundene Information","Информация о проводной сети","Informazioni cablate","有线信息","有線資訊"),
            4:  t("DHCP Status","Statut DHCP","DHCP-Status","Статус DHCP","Stato DHCP","DHCP状态","DHCP狀態"),
            5:  t("IP Configuration","Configuration IP","IP-Konfiguration","IP конфигурация","Configurazione IP","IP配置","IP配置"),
            6:  t("Active Connections","Connexions actives","Aktive Verbindungen","Активные соединения","Connessioni attive","活动连接","活動連接"),
            7:  t("Network Profile","Profil réseau","Netzwerkprofil","Сетевой профиль","Profilo di rete","网络配置文件","網路設定檔"),
            8:  t("TCP Settings","Paramètres TCP","TCP-Einstellungen","Настройки TCP","Impostazioni TCP","TCP设置","TCP設定"),
            9:  t("Default Gateway","Passerelle par défaut","Standardgateway","Шлюз по умолчанию","Gateway predefinito","默认网关","默認閘道"),
            10: t("Routing Table","Table de routage","Routingtabelle","Таблица маршрутизации","Tabella di routing","路由表","路由表"),
            11: t("ARP Table","Table ARP","ARP-Tabelle","ARP таблица","Tabella ARP","ARP表","ARP表"),
            12: t("Proxy Settings","Paramètres proxy","Proxy-Einstellungen","Настройки прокси","Impostazioni proxy","代理设置","代理設定"),
            13: t("Netskope Status","Statut Netskope","Netskope-Status","Статус Netskope","Stato Netskope","Netskope状态","Netskope狀態"),
            14: t("DNS Servers","Serveurs DNS","DNS-Server","DNS серверы","Server DNS","DNS服务器","DNS伺服器"),
            15: t("DNS Cache","Cache DNS","DNS-Cache","DNS кэш","Cache DNS","DNS缓存","DNS快取"),
            16: t("DNS Pollution","Pollution DNS","DNS-Verschmutzung","Загрязнение DNS","Inquinamento DNS","DNS污染","DNS污染"),
            17: t("Internet Connectivity & Speed","Connectivité et débit","Internet & Geschwindigkeit","Интернет и скорость","Connettività e velocità","互联网连接与速度","網際網路連線與速度"),
            18: t("Internet Connectivity & Speed","Connectivité et débit","Internet & Geschwindigkeit","Интернет и скорость","Connettività e velocità","互联网连接与速度","網際網路連線與速度"),
            19: t("DNS Resolution","Résolution DNS","DNS-Auflösung","DNS разрешение","Risoluzione DNS","DNS解析","DNS解析"),
            20: t("Ping","Ping","Ping","Пинг","Ping","Ping","Ping"),
            21: t("Traceroute","Traceroute","Traceroute","Трассировка","Traceroute","路由追踪","路由追蹤"),
            22: t("PathPing","PathPing","PathPing","PathPing","PathPing","路径Ping","路徑Ping"),
            23: t("MTU Discovery","Découverte MTU","MTU-Erkennung","MTU обнаружение","Scoperta MTU","MTU发现","MTU發現"),
            24: t("Port Scan","Scan de ports","Port-Scan","Сканирование портов","Scansione porte","端口扫描","端口掃描"),
            25: t("URL Parsing","Analyse d'URL","URL-Analyse","Парсинг URL","Analisi URL","URL解析","URL解析"),
            26: t("TCP Connect","Connexion TCP","TCP-Verbindung","TCP соединение","Connessione TCP","TCP连接","TCP連接"),
            27: t("Service Banner","Bannière de service","Service-Banner","Баннер сервиса","Banner del servizio","服务横幅","服務橫幅"),
            28: t("HTTP Request","Requête HTTP","HTTP-Anfrage","HTTP запрос","Richiesta HTTP","HTTP请求","HTTP請求"),
            29: t("HTTP Headers","En-têtes HTTP","HTTP-Header","HTTP заголовки","Intestazioni HTTP","HTTP头","HTTP標頭"),
            30: t("Security Headers","En-têtes de sécurité","Sicherheitsheader","Заголовки безопасности","Intestazioni sicurezza","安全头","安全標頭"),
            31: t("SSL Certificate","Certificat SSL","SSL-Zertifikat","SSL сертификат","Certificato SSL","SSL证书","SSL憑證"),
            32: t("HTTP Redirect","Redirection HTTP","HTTP-Weiterleitung","HTTP редирект","Reindirizzamento HTTP","HTTP重定向","HTTP重定向"),
            33: t("HTTP Compression","Compression HTTP","HTTP-Komprimierung","HTTP сжатие","Compressione HTTP","HTTP压缩","HTTP壓縮"),
            34: t("HTTP Timing","Chronométrage HTTP","HTTP-Timing","HTTP тайминг","Temporizzazione HTTP","HTTP计时","HTTP計時"),
            35: t("FTP Diagnostics","Diagnostics FTP","FTP-Diagnose","FTP диагностика","Diagnostica FTP","FTP诊断","FTP診斷"),
            36: t("SSH Diagnostics","Diagnostics SSH","SSH-Diagnose","SSH диагностика","Diagnostica SSH","SSH诊断","SSH診斷"),
            37: t("Email Diagnostics","Diagnostics email","E-Mail-Diagnose","Диагностика почты","Diagnostica email","电子邮件诊断","電子郵件診斷"),
        }
        return typeof names[id] === 'string' ? names[id] : ""
    }
    // ── Test descriptions (all 38) ──
    function diagDesc(id) {
        if (lang <= 0) return ""  // English: use C++ descriptions
        var descs = {
            0:  t("List all network adapters and their operational state","Lister toutes les cartes réseau et leur état","Alle Netzwerkadapter und deren Betriebszustand auflisten","Список всех сетевых адаптеров и их состояние","Elenca tutte le schede di rete e il loro stato","列出所有网络适配器及其运行状态","列出所有網路適配器及其運行狀態"),
            1:  t("Driver version, hardware info, and negotiated link speed","Version du pilote, infos matérielles et vitesse de liaison","Treiberversion, Hardware-Info und ausgehandelte Verbindungsgeschwindigkeit","Версия драйвера, информация об оборудовании и скорость соединения","Versione driver, info hardware e velocità di collegamento","驱动程序版本、硬件信息和协商链路速度","驅動程式版本、硬體資訊和協商鏈路速度"),
            2:  t("Signal strength, SSID, channel, and link quality","Force du signal, SSID, canal et qualité de liaison","Signalstärke, SSID, Kanal und Verbindungsqualität","Уровень сигнала, SSID, канал и качество связи","Potenza segnale, SSID, canale e qualità collegamento","信号强度、SSID、信道和链路质量","訊號強度、SSID、頻道和鏈路品質"),
            3:  t("Ethernet link status, speed, and duplex mode","État de la liaison Ethernet, vitesse et mode duplex","Ethernet-Verbindungsstatus, Geschwindigkeit und Duplexmodus","Статус Ethernet соединения, скорость и дуплексный режим","Stato collegamento Ethernet, velocità e modalità duplex","以太网链路状态、速度和双工模式","乙太網鏈路狀態、速度和雙工模式"),
            4:  t("DHCP lease info, server address, and expiration","Infos de bail DHCP, adresse du serveur et expiration","DHCP-Lease-Info, Serveradresse und Ablauf","Информация о DHCP аренде, адрес сервера и срок действия","Info lease DHCP, indirizzo server e scadenza","DHCP租约信息、服务器地址和过期时间","DHCP租約資訊、伺服器位址和過期時間"),
            5:  t("IP addresses, subnet mask, default gateway, DNS servers","Adresses IP, masque de sous-réseau, passerelle, serveurs DNS","IP-Adressen, Subnetzmaske, Standardgateway, DNS-Server","IP адреса, маска подсети, шлюз по умолчанию, DNS серверы","Indirizzi IP, subnet mask, gateway predefinito, server DNS","IP地址、子网掩码、默认网关、DNS服务器","IP位址、子網路遮罩、預設閘道、DNS伺服器"),
            6:  t("TCP/UDP connections: ESTABLISHED, LISTENING, etc.","Connexions TCP/UDP: ÉTABLIES, EN ÉCOUTE, etc.","TCP/UDP-Verbindungen: HERGESTELLT, HÖREND, usw.","TCP/UDP соединения: УСТАНОВЛЕНО, ПРОСЛУШИВАЕТСЯ и т.д.","Connessioni TCP/UDP: STABILITE, IN ASCOLTO, ecc.","TCP/UDP连接：已建立、监听等","TCP/UDP連線：已建立、監聽等"),
            7:  t("Active network profile type (Domain/Private/Public)","Type de profil réseau actif (Domaine/Privé/Public)","Aktiver Netzwerkprofiltyp (Domäne/Privat/Öffentlich)","Тип активного сетевого профиля (Доменный/Частный/Общественный)","Tipo profilo rete attivo (Dominio/Privato/Pubblico)","活动网络配置文件类型（域/专用/公用）","活動網路設定檔類型（網域/私人/公用）"),
            8:  t("TCP/IP stack parameters and configurations","Paramètres et configuration de la pile TCP/IP","TCP/IP-Stack-Parameter und Konfiguration","Параметры и конфигурация стека TCP/IP","Parametri e configurazione stack TCP/IP","TCP/IP堆栈参数和配置","TCP/IP堆疊參數和組態"),
            9:  t("Default gateway reachability and response time","Accessibilité et temps de réponse de la passerelle","Erreichbarkeit und Antwortzeit des Standardgateways","Доступность и время отклика шлюза по умолчанию","Raggiungibilità e tempo risposta gateway predefinito","默认网关可达性和响应时间","預設閘道可達性和回應時間"),
            10: t("IPv4 and IPv6 routing table entries","Entrées de la table de routage IPv4 et IPv6","IPv4- und IPv6-Routingtabelleneinträge","Записи таблицы маршрутизации IPv4 и IPv6","Voci tabella routing IPv4 e IPv6","IPv4和IPv6路由表条目","IPv4和IPv6路由表條目"),
            11: t("ARP cache entries for local network discovery","Entrées du cache ARP pour découverte réseau local","ARP-Cache-Einträge für lokale Netzwerkerkennung","Записи ARP кэша для обнаружения локальной сети","Voci cache ARP per rilevamento rete locale","ARP缓存条目用于本地网络发现","ARP快取條目用於本地網路探索"),
            12: t("System proxy configuration and auto-detection","Configuration et détection automatique du proxy système","System-Proxy-Konfiguration und Auto-Erkennung","Конфигурация системного прокси и автоопределение","Configurazione proxy di sistema e rilevamento automatico","系统代理配置和自动检测","系統代理組態和自動偵測"),
            13: t("Netskope client status and connection health","Statut du client Netskope et santé de la connexion","Netskope-Client-Status und Verbindungszustand","Статус клиента Netskope и состояние соединения","Stato client Netskope e salute connessione","Netskope客户端状态和连接健康","Netskope用戶端狀態和連線健康"),
            14: t("Configured DNS servers and their responsiveness","Serveurs DNS configurés et leur réactivité","Konfigurierte DNS-Server und deren Reaktionsfähigkeit","Настроенные DNS серверы и их отзывчивость","Server DNS configurati e loro reattività","配置的DNS服务器及其响应性","設定的DNS伺服器及其回應性"),
            15: t("DNS resolver cache entries and statistics","Entrées et statistiques du cache du résolveur DNS","DNS-Resolver-Cache-Einträge und Statistiken","Записи и статистика кэша DNS резолвера","Voci e statistiche cache resolver DNS","DNS解析器缓存条目和统计","DNS解析器快取條目和統計"),
            16: t("Check for DNS hijacking or spoofing indicators","Vérification des indicateurs de détournement DNS","Prüfung auf DNS-Hijacking oder Spoofing","Проверка на признаки перехвата DNS","Controllo indicatori dirottamento DNS","检查DNS劫持或欺骗指标","檢查DNS劫持或欺騙指標"),
            17: t("Connectivity check + Speedtest.net bandwidth test","Test de connectivité + bande passante Speedtest.net","Konnektivitätsprüfung + Speedtest.net-Bandbreitentest","Проверка соединения + тест скорости Speedtest.net","Controllo connettività + test larghezza di banda Speedtest.net","连接检查+Speedtest.net带宽测试","連線檢查+Speedtest.net頻寬測試"),
            18: t("Connectivity check + Speedtest.net bandwidth test","Test de connectivité + bande passante Speedtest.net","Konnektivitätsprüfung + Speedtest.net-Bandbreitentest","Проверка соединения + тест скорости Speedtest.net","Controllo connettività + test larghezza di banda Speedtest.net","连接检查+Speedtest.net带宽测试","連線檢查+Speedtest.net頻寬測試"),
            19: t("Resolve target hostname to IP address(es)","Résoudre le nom d'hôte cible en adresse(s) IP","Zielhostname in IP-Adresse(n) auflösen","Разрешить имя хоста цели в IP адрес(а)","Risolvi hostname target in indirizzo/i IP","将目标主机名解析为IP地址","將目標主機名稱解析為IP位址"),
            20: t("TCP connect round-trip time and packet loss","Temps aller-retour de connexion TCP et perte de paquets","TCP-Verbindungsumlaufzeit und Paketverlust","Время кругового обхода TCP соединения и потеря пакетов","Tempo andata/ritorno connessione TCP e perdita pacchetti","TCP连接往返时间和丢包率","TCP連線往返時間和丟包率"),
            21: t("Route path and per-hop latency to target","Chemin de route et latence par saut vers la cible","Routenpfad und Hop-Latenz zum Ziel","Путь маршрута и задержка на каждом хопе до цели","Percorso di route e latenza per hop verso target","到目标的路由路径和每跳延迟","到目標的路由路徑和每跳延遲"),
            22: t("Combined traceroute and ping with per-hop loss","Traceroute et ping combinés avec perte par saut","Kombinierter Traceroute und Ping mit Hop-Verlust","Комбинированная трассировка и пинг с потерей на хопе","Traceroute e ping combinati con perdita per hop","组合路由追踪和ping及每跳丢包","組合路由追蹤和ping及每跳丟包"),
            23: t("Path MTU discovery to target host","Découverte du MTU du chemin vers l'hôte cible","Pfad-MTU-Erkennung zum Zielhost","Обнаружение MTU пути к целевому хосту","Scoperta MTU percorso verso host target","到目标主机的路径MTU发现","到目標主機的路徑MTU發現"),
            24: t("TCP port scan (common / custom range / both)","Scan de ports TCP (communs / plage personnalisée / les deux)","TCP-Port-Scan (gängig / benutzerdefinierter Bereich / beides)","Сканирование TCP портов (стандартные / пользовательский диапазон / оба)","Scansione porte TCP (comuni / intervallo personalizzato / entrambi)","TCP端口扫描（常用/自定义范围/两者）","TCP埠掃描（常用/自訂範圍/兩者）"),
            25: t("Parse and validate the target URL components","Analyser et valider les composants de l'URL cible","Komponenten der Ziel-URL analysieren und validieren","Разбор и проверка компонентов целевого URL","Analizza e convalida i componenti URL target","解析和验证目标URL组件","解析和驗證目標URL元件"),
            26: t("TCP connectivity check to the URL host on default port","Vérification de connectivité TCP vers l'hôte URL sur le port par défaut","TCP-Konnektivitätsprüfung zum URL-Host auf Standardport","Проверка TCP соединения с хостом URL на порту по умолчанию","Controllo connettività TCP all'host URL su porta predefinita","对URL主机的默认端口进行TCP连接检查","對URL主機的預設埠進行TCP連線檢查"),
            27: t("Service banner detection for text-based protocols","Détection de bannière de service pour protocoles texte","Service-Banner-Erkennung für textbasierte Protokolle","Обнаружение баннера сервиса для текстовых протоколов","Rilevamento banner servizio per protocolli testuali","基于文本协议的服务横幅检测","基於文字協定的服務橫幅偵測"),
            28: t("HTTP request/response headers and timing","En-têtes et chronométrage des requêtes/réponses HTTP","HTTP-Anfrage-/Antwort-Header und Timing","Заголовки и тайминг HTTP запросов/ответов","Intestazioni e temporizzazione richiesta/risposta HTTP","HTTP请求/响应头和计时","HTTP請求/回應標頭和計時"),
            29: t("HTTP response headers from the target server","En-têtes de réponse HTTP du serveur cible","HTTP-Antwort-Header vom Zielserver","HTTP заголовки ответа от целевого сервера","Intestazioni risposta HTTP dal server target","来自目标服务器的HTTP响应头","來自目標伺服器的HTTP回應標頭"),
            30: t("Security-related HTTP headers (HSTS, CSP, etc.)","En-têtes HTTP de sécurité (HSTS, CSP, etc.)","Sicherheitsrelevante HTTP-Header (HSTS, CSP, usw.)","Заголовки безопасности HTTP (HSTS, CSP и т.д.)","Intestazioni HTTP di sicurezza (HSTS, CSP, ecc.)","安全相关的HTTP头（HSTS、CSP等）","安全相關的HTTP標頭（HSTS、CSP等）"),
            31: t("SSL/TLS certificate chain and validity check","Chaîne de certificats SSL/TLS et vérification de validité","SSL/TLS-Zertifikatskette und Gültigkeitsprüfung","Цепочка SSL/TLS сертификатов и проверка действительности","Catena certificati SSL/TLS e controllo validità","SSL/TLS证书链和有效性检查","SSL/TLS憑證鏈和有效性檢查"),
            32: t("HTTP redirect chain and final destination","Chaîne de redirection HTTP et destination finale","HTTP-Weiterleitungskette und Endziel","Цепочка HTTP редиректов и конечный пункт","Catena reindirizzamento HTTP e destinazione finale","HTTP重定向链和最终目的地","HTTP重定向鏈和最終目的地"),
            33: t("Supported compression methods and encoding","Méthodes de compression et encodage prises en charge","Unterstützte Komprimierungsmethoden und Kodierung","Поддерживаемые методы сжатия и кодирования","Metodi compressione e codifica supportati","支持的压缩方法和编码","支援的壓縮方法和編碼"),
            34: t("HTTP request timing breakdown (DNS, connect, SSL, etc.)","Décomposition du chronométrage HTTP (DNS, connexion, SSL, etc.)","HTTP-Anfrage-Timing-Aufschlüsselung (DNS, Verbindung, SSL, usw.)","Разбивка тайминга HTTP запроса (DNS, соединение, SSL и т.д.)","Scomposizione temporizzazione richiesta HTTP (DNS, connessione, SSL, ecc.)","HTTP请求时间分解（DNS、连接、SSL等）","HTTP請求時間分解（DNS、連線、SSL等）"),
            35: t("FTP service reachability and banner detection","Accessibilité du service FTP et détection de bannière","FTP-Diensterreichbarkeit und Banner-Erkennung","Доступность FTP сервиса и обнаружение баннера","Raggiungibilità servizio FTP e rilevamento banner","FTP服务可达性和横幅检测","FTP服務可達性和橫幅偵測"),
            36: t("SSH version and key exchange detection","Version SSH et détection d'échange de clés","SSH-Version und Schlüsselaustauscherkennung","Обнаружение версии SSH и обмена ключами","Versione SSH e rilevamento scambio chiavi","SSH版本和密钥交换检测","SSH版本和金鑰交換偵測"),
            37: t("SMTP/IMAP/POP3 service detection and banner","Détection de service SMTP/IMAP/POP3 et bannière","SMTP/IMAP/POP3-Diensterkennung und Banner","Обнаружение сервиса SMTP/IMAP/POP3 и баннера","Rilevamento servizio SMTP/IMAP/POP3 e banner","SMTP/IMAP/POP3服务检测和横幅","SMTP/IMAP/POP3服務偵測和橫幅"),
        }
        return typeof descs[id] === 'string' ? descs[id] : ""
    }

    // ── Dashboard summary + common labels ──
    readonly property string totalDiagsLabel: t("Total Diagnostics","Total diagnostics","Diagnosen insgesamt","Всего диагностик","Diagnostiche totali","总诊断数","總診斷數")
    readonly property string totalTimeLabel: t("Total Time","Temps total","Gesamtzeit","Общее время","Tempo totale","总时间","總時間")
    readonly property string completedLabel: t("Completed","Terminé","Abgeschlossen","Завершено","Completato","已完成","已完成")
    readonly property string resetLabel: t("Reset","Réinitialiser","Zurücksetzen","Сброс","Ripristina","重置","重置")
    readonly property string diagsSuffix: t(" tests"," tests"," Tests"," тестов"," test"," 个测试"," 個測試")

    // ── Settings screen ──
    readonly property string languageSection: t("Language / 语言","Langue / 语言","Sprache / 语言","Язык / 语言","Lingua / 语言","Language / 语言","Language / 語言")
    readonly property string emailConfigSection: t("Email (SMTP) Configuration","Configuration email (SMTP)","E-Mail (SMTP) Konfiguration","Настройка email (SMTP)","Configurazione email (SMTP)","电子邮件(SMTP)配置","電子郵件(SMTP)配置")
    readonly property string aboutSection: t("About","À propos","Über","О программе","Informazioni","关于","關於")
    readonly property string smtpServerLabel: t("SMTP Server","Serveur SMTP","SMTP-Server","SMTP-сервер","Server SMTP","SMTP服务器","SMTP伺服器")
    readonly property string portLabel: t("Port","Port","Port","Порт","Porta","端口","埠")
    readonly property string usernameLabel: t("Username","Nom d'utilisateur","Benutzername","Имя пользователя","Nome utente","用户名","使用者名稱")
    readonly property string passwordLabel: t("Password","Mot de passe","Passwort","Пароль","Password","密码","密碼")
    readonly property string fromAddrLabel: t("From Address","Adresse d'expédition","Absenderadresse","Адрес отправителя","Indirizzo mittente","发件地址","發件地址")
    readonly property string simulatorTitle: t("NetAnalysis Simulator","Simulateur NetAnalysis","NetAnalysis Simulator","Симулятор NetAnalysis","Simulatore NetAnalysis","NetAnalysis 模拟器","NetAnalysis 模擬器")
}
