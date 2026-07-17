// ── Translation singleton — accessed as Tr.* in any QML file ──────────
pragma Singleton
import QtQuick

Item {
    // 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT
    // Bind directly to C++ property — NOTIFY signal now reliable on all platforms
    property int lang: appState ? appState.languageIndex : 0

    function t(en, fr, de, ru, it, zh_cn, zh_tw, es, pt) {
        var a = [en, fr, de, ru, it, zh_cn, zh_tw, es, pt]
        var i = lang
        return (i >= 0 && i < a.length && a[i]) ? a[i] : en
    }

    // ── Group names (index 0-4 → G1-G5) ──
    function groupName(idx) {
        var d = lang
        var names = [
            t("System & Adapters", "Système et adaptateurs", "System & Adapter", "Система и адаптеры", "Sistema e schede", "系统和适配器", "系統和適配器", "Sistema y adaptadores", "Sistema e adaptadores"),
            t("Connectivity & Security", "Connectivité et sécurité", "Konnektivität & Sicherheit", "Подключение и безопасность", "Connettività e sicurezza", "连接与安全", "連線與安全", "Conectividad y seguridad", "Conectividade e segurança"),
            t("Internet & DNS", "Internet et DNS", "Internet & DNS", "Интернет и DNS", "Internet e DNS", "互联网与DNS", "網際網路與DNS", "Internet y DNS", "Internet e DNS"),
            t("Remote Host", "Hôte distant", "Remote Host", "Удаленный хост", "Host remoto", "远程主机", "遠端主機", "Host remoto", "Host remoto"),
            t("Protocol", "Protocole", "Protokoll", "Протокол", "Protocollo", "协议", "協定", "Protocolo", "Protocolo"),
        ]
        return (idx >= 0 && idx < names.length) ? names[idx] : ""
    }

    // ── Nav ──
    readonly property string dashboard: t("Dashboard", "Tableau de bord", "Dashboard", "Панель", "Dashboard", "仪表板", "儀表板", "Panel", "Painel")
    readonly property string diagnostics: t("Diagnostics", "Diagnostics", "Diagnose", "Диагностика", "Diagnostica", "诊断", "診斷", "Diagnósticos", "Diagnósticos")
    readonly property string config: t("Config", "Configuration", "Konfiguration", "Конфигурация", "Configurazione", "配置", "配置", "Configuración", "Configuração")
    readonly property string report: t("Report", "Rapport", "Bericht", "Отчёт", "Rapporto", "报告", "報告", "Informe", "Relatório")
    readonly property string settings: t("Settings", "Paramètres", "Einstellungen", "Настройки", "Impostazioni", "设置", "設定", "Ajustes", "Configurações")

    // ── DiagnosticScreen ──
    readonly property string runningDots: t("Running Diagnostics...", "Diagnostics en cours...", "Diagnose läuft...", "Диагностика...", "Diagnostica in corso...", "正在诊断...", "正在診斷...", "Ejecutando diagnósticos...", "Executando diagnósticos...")
    readonly property string complete: t("Diagnostic Complete", "Diagnostic terminé", "Diagnose abgeschlossen", "Диагностика завершена", "Diagnostica completata", "诊断完成", "診斷完成", "Diagnóstico completado", "Diagnóstico concluído")
    readonly property string cancelled: t("Cancelled", "Annulé", "Abgebrochen", "Отменено", "Annullato", "已取消", "已取消", "Cancelado", "Cancelado")
    readonly property string errorCheck: t("Error — Check Target", "Erreur — Vérifier la cible", "Fehler — Ziel prüfen", "Ошибка — Проверьте цель", "Errore — Controlla target", "错误 — 检查目标", "錯誤 — 檢查目標", "Error — Verifique el objetivo", "Erro — Verifique o alvo")
    readonly property string results: t("Results", "Résultats", "Ergebnisse", "Результаты", "Risultati", "结果", "結果", "Resultados", "Resultados")
    readonly property string runDiag: t("▶ Run Diagnostics", "▶ Lancer diagnostic", "▶ Diagnose starten", "▶ Запустить", "▶ Avvia diagnostica", "▶ 运行诊断", "▶ 運行診斷", "▶ Ejecutar diagnósticos", "▶ Executar diagnósticos")
    readonly property string running: t("⏳ Running...", "⏳ En cours...", "⏳ Läuft...", "⏳ Выполняется...", "⏳ In corso...", "⏳ 运行中...", "⏳ 運行中...", "⏳ Ejecutando...", "⏳ Executando...")
    readonly property string stop: t("■ Stop", "■ Arrêter", "■ Stopp", "■ Стоп", "■ Ferma", "■ 停止", "■ 停止", "■ Detener", "■ Parar")
    readonly property string target: t("Target", "Cible", "Ziel", "Цель", "Obiettivo", "目标", "目標", "Objetivo", "Alvo")
    // ── ConfigScreen ──
    readonly property string diagConfig: t("Diagnostic Configuration", "Configuration diagnostic", "Diagnosekonfiguration", "Конфигурация диагностики", "Configurazione diagnostica", "诊断配置", "診斷配置", "Configuración de diagnóstico", "Configuração de diagnóstico")
    readonly property string selectAll: t("Select All", "Tout sélectionner", "Alle auswählen", "Выбрать все", "Seleziona tutto", "全选", "全選", "Seleccionar todo", "Selecionar tudo")
    readonly property string deselectAll: t("Deselect All", "Tout désélectionner", "Alle abwählen", "Отменить все", "Deseleziona tutto", "取消全选", "取消全選", "Deseleccionar todo", "Desmarcar tudo")

    // ── DashboardScreen ──
    readonly property string noData: t("No diagnostic data yet", "Aucune donnée de diagnostic", "Noch keine Diagnosedaten", "Нет данных диагностики", "Nessun dato diagnostico", "暂无诊断数据", "暫無診斷數據", "Aún no hay datos de diagnóstico", "Ainda não há dados de diagnóstico")
    readonly property string runFromDiag: t("Run a diagnostic from the Diagnostics screen\nto see results here.", "Exécutez un diagnostic depuis l'écran Diagnostics\npour voir les résultats ici.", "Führen Sie eine Diagnose vom Diagnosebildschirm aus,\num die Ergebnisse hier zu sehen.", "Запустите диагностику на экране диагностики,\nчтобы увидеть результаты здесь.", "Esegui una diagnostica dalla schermata Diagnostica\nper vedere i risultati qui.", "从诊断屏幕运行诊断\n以在此处查看结果。", "從診斷畫面運行診斷\n以在此處查看結果。", "Ejecute un diagnóstico desde la pantalla de Diagnósticos\npara ver los resultados aquí.", "Execute um diagnóstico na tela de Diagnósticos\npara ver os resultados aqui.")
    readonly property string perGroup: t("Per-Group Results", "Résultats par groupe", "Ergebnisse pro Gruppe", "Результаты по группам", "Risultati per gruppo", "分组结果", "分組結果", "Resultados por grupo", "Resultados por grupo")
    readonly property string summary: t("Summary", "Résumé", "Zusammenfassung", "Сводка", "Riepilogo", "摘要", "摘要", "Resumen", "Resumo")
    readonly property string totalDiags: t("Total Diagnostics", "Total diagnostics", "Diagnosen insgesamt", "Всего диагностик", "Diagnostiche totali", "总诊断数", "總診斷數", "Diagnósticos totales", "Diagnósticos totais")
    readonly property string totalTime: t("Total Time", "Temps total", "Gesamtzeit", "Общее время", "Tempo totale", "总时间", "總時間", "Tiempo total", "Tempo total")
    readonly property string completed: t("Completed", "Terminé", "Abgeschlossen", "Завершено", "Completato", "已完成", "已完成", "Completado", "Concluído")
    readonly property string layerTimings: t("Layer Timings", "Chronométrage par couche", "Schichtzeiten", "Время по слоям", "Tempi per livello", "层级时间", "層級時間", "Tiempos por capa", "Tempos por camada")

    // ── SettingsScreen ──
    readonly property string aboutDesc: t("A comprehensive cross-platform network diagnostic tool supporting Windows, macOS, Linux, iOS, and Android.", "Un outil de diagnostic réseau multiplateforme prenant en charge Windows, macOS, Linux, iOS et Android.", "Ein umfassendes plattformübergreifendes Netzwerkdiagnosetool für Windows, macOS, Linux, iOS und Android.", "Комплексный кроссплатформенный инструмент сетевой диагностики с поддержкой Windows, macOS, Linux, iOS и Android.", "Uno strumento completo di diagnostica di rete multipiattaforma che supporta Windows, macOS, Linux, iOS e Android.", "一个全面的跨平台网络诊断工具，支持Windows、macOS、Linux、iOS和Android。", "一個全面的跨平台網路診斷工具，支援Windows、macOS、Linux、iOS和Android。", "Una completa herramienta de diagnóstico de red multiplataforma compatible con Windows, macOS, Linux, iOS y Android.", "Uma completa ferramenta de diagnóstico de rede multiplataforma compatível com Windows, macOS, Linux, iOS e Android.")
    readonly property string crossPlat: t("Cross-platform (Windows, macOS, Linux, iOS, Android)", "Multiplateforme (Windows, macOS, Linux, iOS, Android)", "Plattformübergreifend (Windows, macOS, Linux, iOS, Android)", "Кроссплатформенный (Windows, macOS, Linux, iOS, Android)", "Multipiattaforma (Windows, macOS, Linux, iOS, Android)", "跨平台 (Windows, macOS, Linux, iOS, Android)", "跨平台 (Windows, macOS, Linux, iOS, Android)", "Multiplataforma (Windows, macOS, Linux, iOS, Android)", "Multiplataforma (Windows, macOS, Linux, iOS, Android)")
    readonly property string realtimeDiag: t("Real-time diagnostic engine", "Moteur de diagnostic en temps réel", "Echtzeit-Diagnose-Engine", "Движок диагностики в реальном времени", "Motore di diagnostica in tempo reale", "实时诊断引擎", "實時診斷引擎", "Motor de diagnóstico en tiempo real", "Mecanismo de diagnóstico em tempo real")
    readonly property string detailedReport: t("Detailed reporting and export", "Rapports détaillés et exportation", "Detaillierte Berichte und Export", "Подробная отчетность и экспорт", "Report dettagliati ed esportazione", "详细报告和导出", "詳細報告和匯出", "Informes detallados y exportación", "Relatórios detalhados e exportação")
    readonly property string darkTheme: t("Dark theme UI", "Interface thème sombre", "Dunkles Design", "Темная тема", "Interfaccia tema scuro", "深色主题界面", "深色主題介面", "Interfaz con tema oscuro", "Interface com tema escuro")
    readonly property string themeSystem: t("System", "Système", "System", "Система", "Sistema", "系统", "系統", "Sistema", "Sistema")
    readonly property string themeLight:  t("Light",  "Clair",    "Hell",    "Светлая", "Chiaro",  "浅色", "淺色", "Claro",  "Claro")
    readonly property string themeDark:   t("Dark",   "Sombre",   "Dunkel",  "Темная",  "Scuro",   "深色", "深色", "Oscuro", "Escuro")
    readonly property string simulatorMode: t("Windows simulator mode", "Mode simulateur Windows", "Windows-Simulator-Modus", "Режим симулятора Windows", "Modalità simulatore Windows", "Windows模拟器模式", "Windows模擬器模式", "Modo simulador de Windows", "Modo simulador do Windows")
    // ── ReportScreen ──
    readonly property string reportPreview: t("Report Preview", "Aperçu du rapport", "Berichtsvorschau", "Предпросмотр отчёта", "Anteprima rapporto", "报告预览", "報告預覽", "Vista previa del informe", "Pré-visualização do relatório")
    readonly property string reportSavedTo: t("Saved to:", "Enregistré :", "Gespeichert:", "Сохранено:", "Salvato:", "已保存至：", "已儲存至：", "Guardado en:", "Salvo em:")
    readonly property string reportExportFailed: t("Export failed.", "Échec de l'export.", "Export fehlgeschlagen.", "Ошибка экспорта.", "Esportazione non riuscita.", "导出失败。", "匯出失敗。", "Error al exportar.", "Falha na exportação.")
    readonly property string reportExportHint: t("Export your diagnostic results as a one-page PDF summary or a full HTML report.", "Exportez vos résultats en résumé PDF d'une page ou en rapport HTML complet.", "Exportieren Sie Ihre Ergebnisse als einseitige PDF-Übersicht oder vollständigen HTML-Bericht.", "Экспортируйте результаты как PDF-сводку на одну страницу или полный HTML-отчёт.", "Esporta i risultati come riepilogo PDF di una pagina o rapporto HTML completo.", "将诊断结果导出为一页PDF汇总或完整HTML报告。", "將診斷結果匯出為一頁PDF彙總或完整HTML報告。", "Exporte sus resultados de diagnóstico como un resumen PDF de una página o un informe HTML completo.", "Exporte seus resultados de diagnóstico como um resumo PDF de uma página ou um relatório HTML completo.")
    readonly property string reportRunFirst: t("Run a diagnostic first to generate a report.", "Lancez d'abord un diagnostic pour générer un rapport.", "Führen Sie zuerst eine Diagnose aus, um einen Bericht zu erstellen.", "Сначала запустите диагностику, чтобы создать отчёт.", "Esegui prima una diagnostica per generare un rapporto.", "请先运行诊断以生成报告。", "請先執行診斷以產生報告。", "Ejecute primero un diagnóstico para generar un informe.", "Execute primeiro um diagnóstico para gerar um relatório.")
    readonly property string reportReviewBtn: t("Review Report", "Consulter le rapport", "Bericht anzeigen", "Просмотр отчёта", "Visualizza rapporto", "查看报告", "查看報告", "Revisar informe", "Revisar relatório")
    readonly property string sharePdfBtn: t("Share PDF", "Partager PDF", "PDF teilen", "Поделиться PDF", "Condividi PDF", "分享PDF", "分享PDF", "Compartir PDF", "Compartilhar PDF")
    readonly property string shareHtmlBtn: t("Share HTML", "Partager HTML", "HTML teilen", "Поделиться HTML", "Condividi HTML", "分享HTML", "分享HTML", "Compartir HTML", "Compartilhar HTML")
    readonly property string emailPdfBtn: t("Email PDF", "E-mail PDF", "PDF per E-Mail", "PDF по почте", "Email PDF", "邮件PDF", "郵件PDF", "Correo PDF", "E-mail PDF")
    readonly property string emailHtmlBtn: t("Email HTML", "E-mail HTML", "HTML per E-Mail", "HTML по почте", "Email HTML", "邮件HTML", "郵件HTML", "Correo HTML", "E-mail HTML")
    readonly property string pdfLoading: t("Loading PDF...", "Chargement PDF...", "PDF wird geladen...", "Загрузка PDF...", "Caricamento PDF...", "正在加载PDF...", "正在載入PDF...", "Cargando PDF...", "Carregando PDF...")
    readonly property string pdfLoadFailed: t("Failed to load PDF", "Échec du chargement PDF", "PDF konnte nicht geladen werden", "Не удалось загрузить PDF", "Caricamento PDF non riuscito", "PDF加载失败", "PDF載入失敗", "Error al cargar PDF", "Falha ao carregar PDF")
    readonly property string shareBtn: t("Share", "Partager", "Teilen", "Поделиться", "Condividi", "分享", "分享", "Compartir", "Compartilhar")
    readonly property string emailBtn: t("Email", "E-mail", "E-Mail", "По почте", "Email", "邮件", "郵件", "Correo", "E-mail")
    readonly property string premiumBadge: t("PRO", "PRO", "PRO", "PRO", "PRO", "PRO", "PRO", "PRO", "PRO")
    readonly property string premiumRequiredMsg: t("Sharing is a premium feature — unlock to share reports.", "Le partage est une fonction premium — déverrouillez pour partager.", "Teilen ist eine Premium-Funktion — zum Teilen freischalten.", "Обмен — премиум-функция. Разблокируйте для обмена.", "La condivisione è una funzione premium — sblocca per condividere.", "分享为付费功能 — 解锁后可分享报告。", "分享為付費功能 — 解鎖後可分享報告。", "Compartir es una función premium — desbloquéela para compartir informes.", "Compartilhar é um recurso premium — desbloqueie para compartilhar relatórios.")
    readonly property string reportShareOk: t("Report shared.", "Rapport partagé.", "Bericht geteilt.", "Отчёт отправлен.", "Rapporto condiviso.", "报告已分享。", "報告已分享。", "Informe compartido.", "Relatório compartilhado.")
    readonly property string reportShareFail: t("Sharing failed.", "Échec du partage.", "Teilen fehlgeschlagen.", "Ошибка обмена.", "Condivisione non riuscita.", "分享失败。", "分享失敗。", "Error al compartir.", "Falha ao compartilhar.")
    readonly property string premiumUnlocked: t("Premium unlocked", "Premium déverrouillé", "Premium freigeschaltet", "Премиум разблокирован", "Premium sbloccato", "已解锁高级版", "已解鎖高級版", "Premium desbloqueado", "Premium desbloqueado")
    // ── Subscription / share confirmation flow ──
    readonly property string subscribeTitle: t("Premium Feature", "Fonction Premium", "Premium-Funktion", "Премиум-функция", "Funzione Premium", "高级版功能", "高級版功能", "Función Premium", "Recurso Premium")
    readonly property string subscribeBody: t("Sharing and emailing reports is a Premium feature. Unlock it with a one-time purchase that works on all your devices.", "Le partage et l'envoi de rapports par e-mail sont des fonctions Premium. Débloquez-les avec un achat unique valable sur tous vos appareils.", "Das Teilen und Versenden von Berichten ist eine Premium-Funktion. Schalten Sie sie mit einem einmaligen Kauf frei, der auf all Ihren Geräten gilt.", "Отправка отчётов — премиум-функция. Разблокируйте её разовой покупкой, действующей на всех ваших устройствах.", "La condivisione e l'invio dei rapporti è una funzione Premium. Sbloccala con un acquisto una tantum valido su tutti i tuoi dispositivi.", "分享和邮件发送报告是高级版功能。一次性购买即可解锁，所有设备通用。", "分享和郵件發送報告是高級版功能。一次性購買即可解鎖，所有裝置通用。", "Compartir y enviar informes por correo es una función Premium. Desbloquéela con una compra única válida en todos sus dispositivos.", "Compartilhar e enviar relatórios por e-mail é um recurso Premium. Desbloqueie com uma compra única válida em todos os seus dispositivos.")
    readonly property string subscribeBtn: t("Unlock Premium", "Débloquer Premium", "Premium freischalten", "Разблокировать", "Sblocca Premium", "购买高级版", "購買高級版", "Desbloquear Premium", "Desbloquear Premium")
    readonly property string subscribeNotNow: t("Not now", "Plus tard", "Später", "Не сейчас", "Non ora", "以后再说", "以後再說", "Ahora no", "Agora não")
    readonly property string restoreBtn: t("Restore Purchases", "Restaurer les achats", "Käufe wiederherstellen", "Восстановить покупки", "Ripristina acquisti", "恢复购买", "恢復購買", "Restaurar compras", "Restaurar compras")
    readonly property string restoreOk: t("Purchases restored.", "Achats restaurés.", "Käufe wiederhergestellt.", "Покупки восстановлены.", "Acquisti ripristinati.", "购买已恢复。", "購買已恢復。", "Compras restauradas.", "Compras restauradas.")
    readonly property string restoreFail: t("No previous purchases found.", "Aucun achat précédent trouvé.", "Keine früheren Käufe gefunden.", "Предыдущие покупки не найдены.", "Nessun acquisto precedente trovato.", "未找到之前的购买记录。", "未找到之前的購買記錄。", "No se encontraron compras anteriores.", "Nenhuma compra anterior encontrada.")
    readonly property string restoreError: t("Restore failed. Please try again.", "Échec de la restauration. Veuillez réessayer.", "Wiederherstellung fehlgeschlagen. Bitte versuchen Sie es erneut.", "Ошибка восстановления. Попробуйте снова.", "Ripristino non riuscito. Riprova.", "恢复失败，请重试。", "恢復失敗，請重試。", "Error al restaurar. Inténtelo de nuevo.", "Falha ao restaurar. Tente novamente.")
    readonly property string confirmShareTitle: t("Share Report", "Partager le rapport", "Bericht teilen", "Поделиться отчётом", "Condividi rapporto", "分享报告", "分享報告", "Compartir informe", "Compartilhar relatório")
    readonly property string confirmShareBody: t("Share this diagnostic report now?", "Partager ce rapport de diagnostic maintenant ?", "Diesen Diagnosebericht jetzt teilen?", "Поделиться этим отчётом диагностики?", "Condividere ora questo rapporto diagnostico?", "确认现在分享此诊断报告？", "確認現在分享此診斷報告？", "¿Compartir este informe de diagnóstico ahora?", "Compartilhar este relatório de diagnóstico agora?")
    readonly property string dialogCancel: t("Cancel", "Annuler", "Abbrechen", "Отмена", "Annulla", "取消", "取消", "Cancelar", "Cancelar")
    readonly property string reportResultsAvailable: t(" results available", " résultats disponibles", " Ergebnisse verfügbar", " результатов", " risultati disponibili", " 个结果可用", " 個結果可用", " resultados disponibles", " resultados disponíveis")
    readonly property string reportNoResults: t("No diagnostic results", "Aucun résultat de diagnostic", "Keine Diagnoseergebnisse", "Нет результатов", "Nessun risultato", "无诊断结果", "無診斷結果", "Sin resultados de diagnóstico", "Sem resultados de diagnóstico")

    // ── Target Analysis ──
    readonly property string targetAnalysis: t("Target Analysis", "Analyse de la cible", "Zielanalyse", "Анализ цели", "Analisi obiettivo", "目标分析", "目標分析", "Análisis del objetivo", "Análise do alvo")
    readonly property string knownPortRef: t("Known Port Reference", "Référence des ports connus", "Bekannte Ports Referenz", "Справочник портов", "Riferimento porte note", "已知端口参考", "已知埠參考", "Referencia de puertos conocidos", "Referência de portas conhecidas")
    readonly property string targetTypeLabel: t("Type    :", "Type    :", "Typ     :", "Тип     :", "Tipo    :", "类型    :", "類型    :", "Tipo    :", "Tipo    :")
    readonly property string targetTypeUrl: t("URL", "URL", "URL", "URL", "URL", "URL", "URL", "URL", "URL")
    readonly property string targetTypeIp: t("Remote Host (IP)", "Hôte distant (IP)", "Remote Host (IP)", "Удаленный хост (IP)", "Host remoto (IP)", "远程主机(IP)", "遠端主機(IP)", "Host remoto (IP)", "Host remoto (IP)")
    readonly property string targetTypeHostname: t("Remote Host (Hostname)", "Hôte distant (nom)", "Remote Host (Hostname)", "Удаленный хост (имя)", "Host remoto (nome)", "远程主机(主机名)", "遠端主機(主機名)", "Host remoto (nombre)", "Host remoto (nome)")

    // ── Live Progress ──
    readonly property string errorPrefix: t("Error: ", "Erreur : ", "Fehler: ", "Ошибка: ", "Errore: ", "错误: ", "錯誤: ", "Error: ", "Erro: ")
    readonly property string runningStatus: t("Running", "En cours", "Läuft", "Выполняется", "In corso", "运行中", "運行中", "En ejecución", "Em execução")
    readonly property string completeStatus: t("Complete", "Terminé", "Abgeschlossen", "Завершено", "Completato", "完成", "完成", "Completado", "Concluído")
    readonly property string cancelledStatus: t("Cancelled", "Annulé", "Abgebrochen", "Отменено", "Annullato", "已取消", "已取消", "Cancelado", "Cancelado")
    readonly property string errorStatus: t("Error", "Erreur", "Fehler", "Ошибка", "Errore", "错误", "錯誤", "Error", "Erro")
    readonly property string readyStatus: t("Ready", "Prêt", "Bereit", "Готов", "Pronto", "就绪", "就緒", "Listo", "Pronto")
    // PM: Actionable error recovery hints shown when diagnostics fail
    readonly property string errorRecoveryHint: t("Check: 1) Network connection  2) Target URL format\n3) Firewall/proxy settings  4) Target server reachability",
        "Vérifiez : 1) Connexion réseau  2) Format de l'URL cible\n3) Paramètres pare-feu/proxy  4) Accessibilité du serveur cible",
        "Prüfen: 1) Netzwerkverbindung  2) Ziel-URL-Format\n3) Firewall/Proxy  4) Zielserver-Erreichbarkeit",
        "Проверьте: 1) Сетевое подключение  2) Формат URL\n3) Настройки брандмауэра/прокси  4) Доступность сервера",
        "Verifica: 1) Connessione di rete  2) Formato URL\n3) Impostazioni firewall/proxy  4) Raggiungibilità server",
        "检查：1) 网络连接  2) 目标URL格式\n3) 防火墙/代理设置  4) 目标服务器可访问性",
        "檢查：1) 網路連線  2) 目標URL格式\n3) 防火牆/代理設定  4) 目標伺服器可訪問性",
        "Verifique: 1) Conexión de red  2) Formato de URL\n3) Configuración de firewall/proxy  4) Accesibilidad del servidor",
        "Verifique: 1) Conexão de rede  2) Formato da URL\n3) Configurações de firewall/proxy  4) Acessibilidade do servidor")

    // ── Dashboard ──
    readonly property string diagRunComplete: t("Diagnostic Run Complete", "Diagnostic terminé", "Diagnoselauf abgeschlossen", "Диагностика завершена", "Corsa diagnostica completata", "诊断运行完成", "診斷運行完成", "Ejecución de diagnóstico completada", "Execução de diagnóstico concluída")
    readonly property string targetLabel: t("Target: ", "Cible : ", "Ziel: ", "Цель: ", "Obiettivo: ", "目标: ", "目標: ", "Objetivo: ", "Alvo: ")
    readonly property string naLabel: t("N/A", "N/D", "k.A.", "Н/Д", "N/D", "不适用", "不適用", "N/D", "N/D")

    // ── Scheme group labels (TargetInput schema selector) ──────────────
    readonly property string schemeGroupWeb:    t("Web", "Web", "Web", "Веб", "Web", "网页", "網頁", "Web", "Web")
    readonly property string schemeGroupFile:   t("File Transfer", "Transfert de fichier", "Dateiübertragung", "Передача файлов", "Trasferimento file", "文件传输", "文件傳輸", "Transferencia de archivos", "Transferência de arquivos")
    readonly property string schemeGroupEmail:  t("Email", "Email", "E-Mail", "Эл. почта", "Email", "电子邮件", "電子郵件", "Correo", "Email")
    readonly property string schemeGroupDb:     t("Database", "Base de données", "Datenbank", "База данных", "Database", "数据库", "數據庫", "Base de datos", "Base de dados")
    readonly property string schemeGroupRemote: t("Remote Access", "Accès distant", "Fernzugriff", "Удаленный доступ", "Accesso remoto", "远程访问", "遠程訪問", "Acceso remoto", "Acesso remoto")
    readonly property string schemeGroupDir:    t("Directory", "Annuaire", "Verzeichnis", "Каталог", "Directory", "目录", "目錄", "Directorio", "Diretório")
    readonly property string schemeGroupMsg:    t("Messaging", "Messagerie", "Nachrichten", "Сообщения", "Messaggistica", "消息", "消息", "Mensajería", "Mensagens")

    // ── Summary cards ──
    readonly property string summaryPass: t("Pass", "Réussi", "Bestanden", "Пройден", "Superato", "通过", "通過", "Correcto", "Aprovado")
    readonly property string summaryWarning: t("Warning", "Avertissement", "Warnung", "Предупреждение", "Avviso", "警告", "警告", "Advertencia", "Aviso")
    readonly property string summaryFail: t("Fail", "Échec", "Fehlgeschlagen", "Неудача", "Fallito", "失败", "失敗", "Fallido", "Falhou")
    readonly property string summarySkipped: t("Skipped", "Ignoré", "Übersprungen", "Пропущено", "Saltato", "已跳过", "已跳過", "Omitido", "Ignorado")
    readonly property string summaryInfo: t("Info", "Info", "Info", "Инфо", "Info", "信息", "資訊", "Info", "Info")
    // summaryError removed — unused

    // ── TestResultItem ──
    readonly property string diagRunning: t("Running...", "En cours...", "Läuft...", "Выполняется...", "In corso...", "运行中...", "運行中...", "Ejecutando...", "Executando...")

    // placeholderMsg removed — SMTP feature deprecated

    // ── Test names (45 entries, ids 0-44) ──
    function diagName(id) {
        if (lang <= 0) return ""  // English uses C++ names directly
        var names = {
            0:  t("Network Adapters", "Adaptateurs réseau", "Netzwerkadapter", "Сетевые адаптеры", "Schede di rete", "网络适配器", "網路適配器", "Adaptadores de red", "Adaptadores de rede"),
            1:  t("NIC Advanced", "Carte réseau avancée", "Erweiterte NIC", "NIC расширенный", "NIC avanzata", "NIC高级", "NIC進階", "NIC avanzada", "NIC avançada"),
            2:  t("WiFi Information", "Informations WiFi", "WLAN-Information", "Информация о WiFi", "Informazioni WiFi", "WiFi信息", "WiFi資訊", "Información WiFi", "Informações WiFi"),
            3:  t("Wired Information", "Informations filaire", "Kabelgebundene Information", "Информация о проводной сети", "Informazioni cablate", "有线信息", "有線資訊", "Información por cable", "Informações com fio"),
            4:  t("DHCP Status", "Statut DHCP", "DHCP-Status", "Статус DHCP", "Stato DHCP", "DHCP状态", "DHCP狀態", "Estado DHCP", "Estado DHCP"),
            5:  t("IP Configuration", "Configuration IP", "IP-Konfiguration", "IP конфигурация", "Configurazione IP", "IP配置", "IP配置", "Configuración IP", "Configuração IP"),
            6:  t("Active Connections", "Connexions actives", "Aktive Verbindungen", "Активные соединения", "Connessioni attive", "活动连接", "活動連接", "Conexiones activas", "Conexões ativas"),
            7:  t("Cellular Information", "Information cellulaire", "Mobilfunkinformation", "Информация о сотовой сети", "Informazioni cellulare", "蜂窝信息", "蜂窩資訊", "Información celular", "Informações celulares"),
            8:  t("Network Profile", "Profil réseau", "Netzwerkprofil", "Сетевой профиль", "Profilo di rete", "网络配置文件", "網路設定檔", "Perfil de red", "Perfil de rede"),
            9:  t("TCP Settings", "Paramètres TCP", "TCP-Einstellungen", "Настройки TCP", "Impostazioni TCP", "TCP设置", "TCP設定", "Configuración TCP", "Configurações TCP"),
            10: t("Default Gateway", "Passerelle par défaut", "Standardgateway", "Шлюз по умолчанию", "Gateway predefinito", "默认网关", "默認閘道", "Puerta de enlace predeterminada", "Gateway padrão"),
            11: t("Routing Table", "Table de routage", "Routingtabelle", "Таблица маршрутизации", "Tabella di routing", "路由表", "路由表", "Tabla de enrutamiento", "Tabela de roteamento"),
            12: t("ARP Table", "Table ARP", "ARP-Tabelle", "ARP таблица", "Tabella ARP", "ARP表", "ARP表", "Tabla ARP", "Tabela ARP"),
            13: t("Proxy Settings", "Paramètres proxy", "Proxy-Einstellungen", "Настройки прокси", "Impostazioni proxy", "代理设置", "代理設定", "Configuración de proxy", "Configurações de proxy"),
            14: t("Netskope Status", "Statut Netskope", "Netskope-Status", "Статус Netskope", "Stato Netskope", "Netskope状态", "Netskope狀態", "Estado de Netskope", "Estado do Netskope"),
            15: t("DNS Servers", "Serveurs DNS", "DNS-Server", "DNS серверы", "Server DNS", "DNS服务器", "DNS伺服器", "Servidores DNS", "Servidores DNS"),
            16: t("DNS Cache", "Cache DNS", "DNS-Cache", "DNS кэш", "Cache DNS", "DNS缓存", "DNS快取", "Caché DNS", "Cache DNS"),
            17: t("DNS Pollution", "Pollution DNS", "DNS-Verschmutzung", "Загрязнение DNS", "Inquinamento DNS", "DNS污染", "DNS污染", "Contaminación DNS", "Poluição DNS"),
            18: t("VPN Status", "Statut VPN", "VPN-Status", "Статус VPN", "Stato VPN", "VPN状态", "VPN狀態", "Estado VPN", "Estado VPN"),
            19: t("Internet Connectivity & Speed", "Connectivité et débit", "Internet & Geschwindigkeit", "Интернет и скорость", "Connettività e velocità", "互联网连接与速度", "網際網路連線與速度", "Conectividad y velocidad de Internet", "Conectividade e velocidade da Internet"),
            20: t("DNS Resolution", "Résolution DNS", "DNS-Auflösung", "DNS разрешение", "Risoluzione DNS", "DNS解析", "DNS解析", "Resolución DNS", "Resolução DNS"),
            21: t("Ping", "Ping", "Ping", "Пинг", "Ping", "Ping", "Ping", "Ping", "Ping"),
            22: t("Traceroute", "Traceroute", "Traceroute", "Трассировка", "Traceroute", "路由追踪", "路由追蹤", "Traceroute", "Traceroute"),
            23: t("PathPing", "PathPing", "PathPing", "PathPing", "PathPing", "路径Ping", "路徑Ping", "PathPing", "PathPing"),
            24: t("MTU Discovery", "Découverte MTU", "MTU-Erkennung", "MTU обнаружение", "Scoperta MTU", "MTU发现", "MTU發現", "Descubrimiento de MTU", "Descoberta de MTU"),
            25: t("URL Parsing", "Analyse d'URL", "URL-Analyse", "Парсинг URL", "Analisi URL", "URL解析", "URL解析", "Análisis de URL", "Análise de URL"),
            26: t("TCP Connect", "Connexion TCP", "TCP-Verbindung", "TCP соединение", "Connessione TCP", "TCP连接", "TCP連接", "Conexión TCP", "Conexão TCP"),
            27: t("Service Banner", "Bannière de service", "Service-Banner", "Баннер сервиса", "Banner del servizio", "服务横幅", "服務橫幅", "Banner de servicio", "Banner de serviço"),
            28: t("HTTP Request", "Requête HTTP", "HTTP-Anfrage", "HTTP запрос", "Richiesta HTTP", "HTTP请求", "HTTP請求", "Solicitud HTTP", "Requisição HTTP"),
            29: t("HTTP Headers", "En-têtes HTTP", "HTTP-Header", "HTTP заголовки", "Intestazioni HTTP", "HTTP头", "HTTP標頭", "Encabezados HTTP", "Cabeçalhos HTTP"),
            30: t("Security Headers", "En-têtes de sécurité", "Sicherheitsheader", "Заголовки безопасности", "Intestazioni sicurezza", "安全头", "安全標頭", "Encabezados de seguridad", "Cabeçalhos de segurança"),
            31: t("SSL Certificate", "Certificat SSL", "SSL-Zertifikat", "SSL сертификат", "Certificato SSL", "SSL证书", "SSL憑證", "Certificado SSL", "Certificado SSL"),
            32: t("HTTP Redirect", "Redirection HTTP", "HTTP-Weiterleitung", "HTTP редирект", "Reindirizzamento HTTP", "HTTP重定向", "HTTP重定向", "Redirección HTTP", "Redirecionamento HTTP"),
            33: t("HTTP Compression", "Compression HTTP", "HTTP-Komprimierung", "HTTP сжатие", "Compressione HTTP", "HTTP压缩", "HTTP壓縮", "Compresión HTTP", "Compressão HTTP"),
            34: t("HTTP Timing", "Chronométrage HTTP", "HTTP-Timing", "HTTP тайминг", "Temporizzazione HTTP", "HTTP计时", "HTTP計時", "Tiempos HTTP", "Tempos HTTP"),
            35: t("FTP Diagnostics", "Diagnostics FTP", "FTP-Diagnose", "FTP диагностика", "Diagnostica FTP", "FTP诊断", "FTP診斷", "Diagnóstico FTP", "Diagnóstico FTP"),
            36: t("SSH Diagnostics", "Diagnostics SSH", "SSH-Diagnose", "SSH диагностика", "Diagnostica SSH", "SSH诊断", "SSH診斷", "Diagnóstico SSH", "Diagnóstico SSH"),
            37: t("Email Diagnostics", "Diagnostics email", "E-Mail-Diagnose", "Диагностика почты", "Diagnostica email", "电子邮件诊断", "電子郵件診斷", "Diagnóstico de correo", "Diagnóstico de e-mail"),
            38: t("Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet"),
            39: t("MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL"),
            40: t("PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL"),
            41: t("Redis", "Redis", "Redis", "Redis", "Redis", "Redis", "Redis", "Redis", "Redis"),
            42: t("MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB"),
            43: t("LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP"),
            44: t("MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT"),
        }
        return typeof names[id] === 'string' ? names[id] : ""
    }
    // ── Test descriptions (45 entries, ids 0-44) ──
    function diagDesc(id) {
        if (lang <= 0) return ""  // English: use C++ descriptions
        var descs = {
            0:  t("List all network adapters and their operational state", "Lister toutes les cartes réseau et leur état", "Alle Netzwerkadapter und deren Betriebszustand auflisten", "Список всех сетевых адаптеров и их состояние", "Elenca tutte le schede di rete e il loro stato", "列出所有网络适配器及其运行状态", "列出所有網路適配器及其運行狀態", "Enumera todos los adaptadores de red y su estado operativo", "Lista todos os adaptadores de rede e seu estado operacional"),
            1:  t("Driver version, hardware info, and negotiated link speed", "Version du pilote, infos matérielles et vitesse de liaison", "Treiberversion, Hardware-Info und ausgehandelte Verbindungsgeschwindigkeit", "Версия драйвера, информация об оборудовании и скорость соединения", "Versione driver, info hardware e velocità di collegamento", "驱动程序版本、硬件信息和协商链路速度", "驅動程式版本、硬體資訊和協商鏈路速度", "Versión del controlador, información de hardware y velocidad de enlace negociada", "Versão do driver, informações de hardware e velocidade de link negociada"),
            2:  t("Signal strength, SSID, channel, and link quality", "Force du signal, SSID, canal et qualité de liaison", "Signalstärke, SSID, Kanal und Verbindungsqualität", "Уровень сигнала, SSID, канал и качество связи", "Potenza segnale, SSID, canale e qualità collegamento", "信号强度、SSID、信道和链路质量", "訊號強度、SSID、頻道和鏈路品質", "Intensidad de señal, SSID, canal y calidad del enlace", "Intensidade do sinal, SSID, canal e qualidade do link"),
            3:  t("Ethernet link status, speed, and duplex mode", "État de la liaison Ethernet, vitesse et mode duplex", "Ethernet-Verbindungsstatus, Geschwindigkeit und Duplexmodus", "Статус Ethernet соединения, скорость и дуплексный режим", "Stato collegamento Ethernet, velocità e modalità duplex", "以太网链路状态、速度和双工模式", "乙太網鏈路狀態、速度和雙工模式", "Estado del enlace Ethernet, velocidad y modo dúplex", "Estado do link Ethernet, velocidade e modo duplex"),
            4:  t("DHCP lease info, server address, and expiration", "Infos de bail DHCP, adresse du serveur et expiration", "DHCP-Lease-Info, Serveradresse und Ablauf", "Информация о DHCP аренде, адрес сервера и срок действия", "Info lease DHCP, indirizzo server e scadenza", "DHCP租约信息、服务器地址和过期时间", "DHCP租約資訊、伺服器位址和過期時間", "Información de concesión DHCP, dirección del servidor y expiración", "Informações de concessão DHCP, endereço do servidor e expiração"),
            5:  t("IP addresses, subnet mask, default gateway, DNS servers", "Adresses IP, masque de sous-réseau, passerelle, serveurs DNS", "IP-Adressen, Subnetzmaske, Standardgateway, DNS-Server", "IP адреса, маска подсети, шлюз по умолчанию, DNS серверы", "Indirizzi IP, subnet mask, gateway predefinito, server DNS", "IP地址、子网掩码、默认网关、DNS服务器", "IP位址、子網路遮罩、預設閘道、DNS伺服器", "Direcciones IP, máscara de subred, puerta de enlace predeterminada, servidores DNS", "Endereços IP, máscara de sub-rede, gateway padrão, servidores DNS"),
            6:  t("TCP/UDP connections: ESTABLISHED, LISTENING, etc.", "Connexions TCP/UDP: ÉTABLIES, EN ÉCOUTE, etc.", "TCP/UDP-Verbindungen: HERGESTELLT, HÖREND, usw.", "TCP/UDP соединения: УСТАНОВЛЕНО, ПРОСЛУШИВАЕТСЯ и т.д.", "Connessioni TCP/UDP: STABILITE, IN ASCOLTO, ecc.", "TCP/UDP连接：已建立、监听等", "TCP/UDP連線：已建立、監聽等", "Conexiones TCP/UDP: ESTABLECIDA, ESCUCHANDO, etc.", "Conexões TCP/UDP: ESTABELECIDA, ESCUTANDO, etc."),
            7:  t("Cellular network type, signal strength, carrier, and radio access technology", "Type de réseau cellulaire, force du signal, opérateur et technologie d''accès radio", "Mobilfunknetztyp, Signalstärke, Anbieter und Funkzugangstechnologie", "Тип сотовой сети, уровень сигнала, оператор и технология радиодоступа", "Tipo di rete cellulare, potenza del segnale, operatore e tecnologia di accesso radio", "蜂窝网络类型、信号强度、运营商和无线接入技术", "蜂窩網路類型、訊號強度、電信業者和無線接入技術", "Tipo de red celular, intensidad de señal, operador y tecnología de acceso radio", "Tipo de rede celular, intensidade do sinal, operadora e tecnologia de acesso via rádio"),
            8:  t("Active network profile type (Domain/Private/Public)", "Type de profil réseau actif (Domaine/Privé/Public)", "Aktiver Netzwerkprofiltyp (Domäne/Privat/Öffentlich)", "Тип активного сетевого профиля (Доменный/Частный/Общественный)", "Tipo profilo rete attivo (Dominio/Privato/Pubblico)", "活动网络配置文件类型（域/专用/公用）", "活動網路設定檔類型（網域/私人/公用）", "Tipo de perfil de red activo (Dominio/Privado/Público)", "Tipo de perfil de rede ativo (Domínio/Privado/Público)"),
            9:  t("TCP/IP stack parameters and configurations", "Paramètres et configuration de la pile TCP/IP", "TCP/IP-Stack-Parameter und Konfiguration", "Параметры и конфигурация стека TCP/IP", "Parametri e configurazione stack TCP/IP", "TCP/IP堆栈参数和配置", "TCP/IP堆疊參數和組態", "Parámetros y configuraciones de la pila TCP/IP", "Parâmetros e configurações da pilha TCP/IP"),
            10: t("Default gateway reachability and response time", "Accessibilité et temps de réponse de la passerelle", "Erreichbarkeit und Antwortzeit des Standardgateways", "Доступность и время отклика шлюза по умолчанию", "Raggiungibilità e tempo risposta gateway predefinito", "默认网关可达性和响应时间", "預設閘道可達性和回應時間", "Accesibilidad y tiempo de respuesta de la puerta de enlace predeterminada", "Acessibilidade e tempo de resposta do gateway padrão"),
            11: t("IPv4 and IPv6 routing table entries", "Entrées de la table de routage IPv4 et IPv6", "IPv4- und IPv6-Routingtabelleneinträge", "Записи таблицы маршрутизации IPv4 и IPv6", "Voci tabella routing IPv4 e IPv6", "IPv4和IPv6路由表条目", "IPv4和IPv6路由表條目", "Entradas de la tabla de enrutamiento IPv4 e IPv6", "Entradas da tabela de roteamento IPv4 e IPv6"),
            12: t("ARP cache entries for local network discovery", "Entrées du cache ARP pour découverte réseau local", "ARP-Cache-Einträge für lokale Netzwerkerkennung", "Записи ARP кэша для обнаружения локальной сети", "Voci cache ARP per rilevamento rete locale", "ARP缓存条目用于本地网络发现", "ARP快取條目用於本地網路探索", "Entradas de caché ARP para el descubrimiento de la red local", "Entradas de cache ARP para descoberta da rede local"),
            13: t("System proxy configuration and auto-detection", "Configuration et détection automatique du proxy système", "System-Proxy-Konfiguration und Auto-Erkennung", "Конфигурация системного прокси и автоопределение", "Configurazione proxy di sistema e rilevamento automatico", "系统代理配置和自动检测", "系統代理組態和自動偵測", "Configuración del proxy del sistema y detección automática", "Configuração de proxy do sistema e detecção automática"),
            14: t("Netskope client status and connection health", "Statut du client Netskope et santé de la connexion", "Netskope-Client-Status und Verbindungszustand", "Статус клиента Netskope и состояние соединения", "Stato client Netskope e salute connessione", "Netskope客户端状态和连接健康", "Netskope用戶端狀態和連線健康", "Estado del cliente Netskope y salud de la conexión", "Estado do cliente Netskope e integridade da conexão"),
            15: t("Configured DNS servers and their responsiveness", "Serveurs DNS configurés et leur réactivité", "Konfigurierte DNS-Server und deren Reaktionsfähigkeit", "Настроенные DNS серверы и их отзывчивость", "Server DNS configurati e loro reattività", "配置的DNS服务器及其响应性", "設定的DNS伺服器及其回應性", "Servidores DNS configurados y su capacidad de respuesta", "Servidores DNS configurados e sua capacidade de resposta"),
            16: t("DNS resolver cache entries and statistics", "Entrées et statistiques du cache du résolveur DNS", "DNS-Resolver-Cache-Einträge und Statistiken", "Записи и статистика кэша DNS резолвера", "Voci e statistiche cache resolver DNS", "DNS解析器缓存条目和统计", "DNS解析器快取條目和統計", "Entradas y estadísticas de la caché del resolvedor DNS", "Entradas e estatísticas do cache do resolvedor DNS"),
            17: t("Check for DNS hijacking or spoofing indicators", "Vérification des indicateurs de détournement DNS", "Prüfung auf DNS-Hijacking oder Spoofing", "Проверка на признаки перехвата DNS", "Controllo indicatori dirottamento DNS", "检查DNS劫持或欺骗指标", "檢查DNS劫持或欺騙指標", "Comprobar indicadores de secuestro o suplantación de DNS", "Verificar indicadores de sequestro ou falsificação de DNS"),
            18: t("VPN status detection: classifies user as A(CN no VPN), B(CN behind VPN), C(overseas behind CN VPN), or D(overseas no VPN)", "Détection de statut VPN", "VPN-Status-Erkennung", "Определение статуса VPN", "Rilevamento stato VPN", "VPN状态检测: A国内无VPN, B国内挂VPN, C海外挂CN VPN, D海外无VPN", "VPN狀態檢測", "Detección de estado VPN", "Detecção de estado VPN"),
            21: t("Connectivity check + Speedtest.net bandwidth test", "Test de connectivité + bande passante Speedtest.net", "Konnektivitätsprüfung + Speedtest.net-Bandbreitentest", "Проверка соединения + тест скорости Speedtest.net", "Controllo connettività + test larghezza di banda Speedtest.net", "连接检查+Speedtest.net带宽测试", "連線檢查+Speedtest.net頻寬測試", "Comprobación de conectividad + prueba de ancho de banda de Speedtest.net", "Verificação de conectividade + teste de largura de banda do Speedtest.net")
            22: t("Resolve target hostname to IP address(es)", "Résoudre le nom d'hôte cible en adresse(s) IP", "Zielhostname in IP-Adresse(n) auflösen", "Разрешить имя хоста цели в IP адрес(а)", "Risolvi hostname target in indirizzo/i IP", "将目标主机名解析为IP地址", "將目標主機名稱解析為IP位址", "Resolver el nombre de host objetivo a dirección(es) IP", "Resolver o nome de host alvo para endereço(s) IP")
            23: t("TCP connect round-trip time and packet loss", "Temps aller-retour de connexion TCP et perte de paquets", "TCP-Verbindungsumlaufzeit und Paketverlust", "Время кругового обхода TCP соединения и потеря пакетов", "Tempo andata/ritorno connessione TCP e perdita pacchetti", "TCP连接往返时间和丢包率", "TCP連線往返時間和丟包率", "Tiempo de ida y vuelta de conexión TCP y pérdida de paquetes", "Tempo de ida e volta da conexão TCP e perda de pacotes")
            24: t("Route path and per-hop latency to target", "Chemin de route et latence par saut vers la cible", "Routenpfad und Hop-Latenz zum Ziel", "Путь маршрута и задержка на каждом хопе до цели", "Percorso di route e latenza per hop verso target", "到目标的路由路径和每跳延迟", "到目標的路由路徑和每跳延遲", "Ruta y latencia por salto hasta el objetivo", "Caminho da rota e latência por salto até o alvo")
            25: t("Combined traceroute and ping with per-hop loss", "Traceroute et ping combinés avec perte par saut", "Kombinierter Traceroute und Ping mit Hop-Verlust", "Комбинированная трассировка и пинг с потерей на хопе", "Traceroute e ping combinati con perdita per hop", "组合路由追踪和ping及每跳丢包", "組合路由追蹤和ping及每跳丟包", "Traceroute y ping combinados con pérdida por salto", "Traceroute e ping combinados com perda por salto")
            26: t("Path MTU discovery to target host", "Découverte du MTU du chemin vers l'hôte cible", "Pfad-MTU-Erkennung zum Zielhost", "Обнаружение MTU пути к целевому хосту", "Scoperta MTU percorso verso host target", "到目标主机的路径MTU发现", "到目標主機的路徑MTU發現", "Descubrimiento de MTU de ruta hasta el host objetivo", "Descoberta de MTU de caminho até o host alvo")
            27: t("Parse and validate the target URL components", "Analyser et valider les composants de l'URL cible", "Komponenten der Ziel-URL analysieren und validieren", "Разбор и проверка компонентов целевого URL", "Analizza e convalida i componenti URL target", "解析和验证目标URL组件", "解析和驗證目標URL元件", "Analizar y validar los componentes de la URL objetivo", "Analisar e validar os componentes da URL alvo")
            28: t("TCP connectivity check to the URL host on default port", "Vérification de connectivité TCP vers l'hôte URL sur le port par défaut", "TCP-Konnektivitätsprüfung zum URL-Host auf Standardport", "Проверка TCP соединения с хостом URL на порту по умолчанию", "Controllo connettività TCP all'host URL su porta predefinita", "对URL主机的默认端口进行TCP连接检查", "對URL主機的預設埠進行TCP連線檢查", "Comprobación de conectividad TCP al host de la URL en el puerto predeterminado", "Verificação de conectividade TCP ao host da URL na porta padrão")
            29: t("Service banner detection for text-based protocols", "Détection de bannière de service pour protocoles texte", "Service-Banner-Erkennung für textbasierte Protokolle", "Обнаружение баннера сервиса для текстовых протоколов", "Rilevamento banner servizio per protocolli testuali", "基于文本协议的服务横幅检测", "基於文字協定的服務橫幅偵測", "Detección de banner de servicio para protocolos basados en texto", "Detecção de banner de serviço para protocolos baseados em texto")
            30: t("HTTP request/response headers and timing", "En-têtes et chronométrage des requêtes/réponses HTTP", "HTTP-Anfrage-/Antwort-Header und Timing", "Заголовки и тайминг HTTP запросов/ответов", "Intestazioni e temporizzazione richiesta/risposta HTTP", "HTTP请求/响应头和计时", "HTTP請求/回應標頭和計時", "Encabezados y tiempos de solicitud/respuesta HTTP", "Cabeçalhos e tempos de requisição/resposta HTTP")
            31: t("HTTP response headers from the target server", "En-têtes de réponse HTTP du serveur cible", "HTTP-Antwort-Header vom Zielserver", "HTTP заголовки ответа от целевого сервера", "Intestazioni risposta HTTP dal server target", "来自目标服务器的HTTP响应头", "來自目標伺服器的HTTP回應標頭", "Encabezados de respuesta HTTP del servidor objetivo", "Cabeçalhos de resposta HTTP do servidor alvo")
            32: t("Security-related HTTP headers (HSTS, CSP, etc.)", "En-têtes HTTP de sécurité (HSTS, CSP, etc.)", "Sicherheitsrelevante HTTP-Header (HSTS, CSP, usw.)", "Заголовки безопасности HTTP (HSTS, CSP и т.д.)", "Intestazioni HTTP di sicurezza (HSTS, CSP, ecc.)", "安全相关的HTTP头（HSTS、CSP等）", "安全相關的HTTP標頭（HSTS、CSP等）", "Encabezados HTTP de seguridad (HSTS, CSP, etc.)", "Cabeçalhos HTTP de segurança (HSTS, CSP, etc.)")
            33: t("SSL/TLS certificate chain and validity check", "Chaîne de certificats SSL/TLS et vérification de validité", "SSL/TLS-Zertifikatskette und Gültigkeitsprüfung", "Цепочка SSL/TLS сертификатов и проверка действительности", "Catena certificati SSL/TLS e controllo validità", "SSL/TLS证书链和有效性检查", "SSL/TLS憑證鏈和有效性檢查", "Cadena de certificados SSL/TLS y comprobación de validez", "Cadeia de certificados SSL/TLS e verificação de validade")
            34: t("HTTP redirect chain and final destination", "Chaîne de redirection HTTP et destination finale", "HTTP-Weiterleitungskette und Endziel", "Цепочка HTTP редиректов и конечный пункт", "Catena reindirizzamento HTTP e destinazione finale", "HTTP重定向链和最终目的地", "HTTP重定向鏈和最終目的地", "Cadena de redirección HTTP y destino final", "Cadeia de redirecionamento HTTP e destino final")
            35: t("Supported compression methods and encoding", "Méthodes de compression et encodage prises en charge", "Unterstützte Komprimierungsmethoden und Kodierung", "Поддерживаемые методы сжатия и кодирования", "Metodi compressione e codifica supportati", "支持的压缩方法和编码", "支援的壓縮方法和編碼", "Métodos de compresión y codificación admitidos", "Métodos de compressão e codificação suportados")
            36: t("HTTP request timing breakdown (DNS, connect, SSL, etc.)", "Décomposition du chronométrage HTTP (DNS, connexion, SSL, etc.)", "HTTP-Anfrage-Timing-Aufschlüsselung (DNS, Verbindung, SSL, usw.)", "Разбивка тайминга HTTP запроса (DNS, соединение, SSL и т.д.)", "Scomposizione temporizzazione richiesta HTTP (DNS, connessione, SSL, ecc.)", "HTTP请求时间分解（DNS、连接、SSL等）", "HTTP請求時間分解（DNS、連線、SSL等）", "Desglose de tiempos de solicitud HTTP (DNS, conexión, SSL, etc.)", "Detalhamento dos tempos de requisição HTTP (DNS, conexão, SSL, etc.)")
            37: t("FTP service reachability and banner detection", "Accessibilité du service FTP et détection de bannière", "FTP-Diensterreichbarkeit und Banner-Erkennung", "Доступность FTP сервиса и обнаружение баннера", "Raggiungibilità servizio FTP e rilevamento banner", "FTP服务可达性和横幅检测", "FTP服務可達性和橫幅偵測", "Accesibilidad del servicio FTP y detección de banner", "Acessibilidade do serviço FTP e detecção de banner")
            38: t("SSH version and key exchange detection", "Version SSH et détection d'échange de clés", "SSH-Version und Schlüsselaustauscherkennung", "Обнаружение версии SSH и обмена ключами", "Versione SSH e rilevamento scambio chiavi", "SSH版本和密钥交换检测", "SSH版本和金鑰交換偵測", "Detección de versión SSH e intercambio de claves", "Detecção de versão SSH e troca de chaves")
            39: t("SMTP/IMAP/POP3 service detection and banner", "Détection de service SMTP/IMAP/POP3 et bannière", "SMTP/IMAP/POP3-Diensterkennung und Banner", "Обнаружение сервиса SMTP/IMAP/POP3 и баннера", "Rilevamento servizio SMTP/IMAP/POP3 e banner", "SMTP/IMAP/POP3服务检测和横幅", "SMTP/IMAP/POP3服務偵測和橫幅", "Detección de servicio SMTP/IMAP/POP3 y banner", "Detecção de serviço SMTP/IMAP/POP3 e banner")
            40: t("Telnet service reachability and login banner", "Accessibilité du service Telnet et bannière de connexion", "Telnet-Diensterreichbarkeit und Login-Banner", "Доступность сервиса Telnet и баннер входа", "Raggiungibilità servizio Telnet e banner login", "Telnet服务可达性和登录横幅", "Telnet服務可達性和登入橫幅", "Accesibilidad del servicio Telnet y banner de inicio de sesión", "Acessibilidade do serviço Telnet e banner de login")
            41: t("MySQL server reachability and version detection", "Accessibilité du serveur MySQL et détection de version", "MySQL-Server-Erreichbarkeit und Versionserkennung", "Доступность сервера MySQL и обнаружение версии", "Raggiungibilità server MySQL e rilevamento versione", "MySQL服务器可达性和版本检测", "MySQL伺服器可達性和版本偵測", "Accesibilidad del servidor MySQL y detección de versión", "Acessibilidade do servidor MySQL e detecção de versão")
            42: t("PostgreSQL server reachability and version detection", "Accessibilité du serveur PostgreSQL et détection de version", "PostgreSQL-Server-Erreichbarkeit und Versionserkennung", "Доступность сервера PostgreSQL и обнаружение версии", "Raggiungibilità server PostgreSQL e rilevamento versione", "PostgreSQL服务器可达性和版本检测", "PostgreSQL伺服器可達性和版本偵測", "Accesibilidad del servidor PostgreSQL y detección de versión", "Acessibilidade do servidor PostgreSQL e detecção de versão")
            43: t("Redis server reachability and INFO command", "Accessibilité du serveur Redis et commande INFO", "Redis-Server-Erreichbarkeit und INFO-Befehl", "Доступность сервера Redis и команда INFO", "Raggiungibilità server Redis e comando INFO", "Redis服务器可达性和INFO命令", "Redis伺服器可達性和INFO命令", "Accesibilidad del servidor Redis y comando INFO", "Acessibilidade do servidor Redis e comando INFO")
            44: t("MongoDB server reachability and build info", "Accessibilité du serveur MongoDB et informations de build", "MongoDB-Server-Erreichbarkeit und Build-Info", "Доступность сервера MongoDB и информация о сборке", "Raggiungibilità server MongoDB e info build", "MongoDB服务器可达性和构建信息", "MongoDB伺服器可達性和構建資訊", "Accesibilidad del servidor MongoDB e información de compilación", "Acessibilidade do servidor MongoDB e informações de build")
            45: t("LDAP server reachability and root DSE", "Accessibilité du serveur LDAP et DSE racine", "LDAP-Server-Erreichbarkeit und Root-DSE", "Доступность сервера LDAP и корневой DSE", "Raggiungibilità server LDAP e DSE root", "LDAP服务器可达性和根DSE", "LDAP伺服器可達性和根DSE", "Accesibilidad del servidor LDAP y DSE raíz", "Acessibilidade do servidor LDAP e DSE raiz")
            45: t("MQTT broker reachability and CONNECT response", "Accessibilité du broker MQTT et réponse CONNECT", "MQTT-Broker-Erreichbarkeit und CONNECT-Antwort", "Доступность брокера MQTT и ответ CONNECT", "Raggiungibilità broker MQTT e risposta CONNECT", "MQTT代理可达性和CONNECT响应", "MQTT代理可達性和CONNECT回應", "Accesibilidad del broker MQTT y respuesta CONNECT", "Acessibilidade do broker MQTT e resposta CONNECT"),
        }
        return typeof descs[id] === 'string' ? descs[id] : ""
    }

    // ── Dashboard summary + common labels ──
    // 5WHY: totalDiags/totalTime/completed were duplicated as
    // totalDiagsLabel/totalTimeLabel/completedLabel with
    // identical translation strings — a DRY violation that could
    // cause desync if only one copy was updated.  Now aliased so
    // both names work, single canonical definition per string.
    readonly property string totalDiagsLabel: totalDiags
    readonly property string totalTimeLabel: totalTime
    readonly property string completedLabel: completed
    readonly property string diagsSuffix: t(" tests", " tests", " Tests", " тестов", " test", " 个测试", " 個測試", " pruebas", " testes")

    // ── Settings screen ──
    readonly property string appearanceSection: t("Appearance", "Apparence", "Erscheinungsbild", "Внешний вид", "Aspetto", "外观", "外觀", "Apariencia", "Aparência")
    readonly property string themeLabel: t("Theme", "Thème", "Design", "Тема", "Tema", "主题", "主題", "Tema", "Tema")
    readonly property string languageSection: t("Language", "Langue", "Sprache", "Язык", "Lingua", "语言", "語言", "Idioma", "Idioma")
    readonly property string aboutSection: t("About", "À propos", "Über", "О программе", "Informazioni", "关于", "關於", "Acerca de", "Sobre")
    readonly property string usernameLabel: t("Username", "Nom d'utilisateur", "Benutzername", "Имя пользователя", "Nome utente", "用户名", "使用者名稱", "Usuario", "Usuário")
    readonly property string passwordLabel: t("Password", "Mot de passe", "Passwort", "Пароль", "Password", "密码", "密碼", "Contraseña", "Senha")
    readonly property string fromAddrLabel: t("From Address", "Adresse d'expédition", "Absenderadresse", "Адрес отправителя", "Indirizzo mittente", "发件地址", "發件地址", "Dirección de remitente", "Endereço do remetente")
    readonly property string simulatorTitle: t("NetDiagnostics Simulator", "Simulateur NetDiagnostics", "NetDiagnostics Simulator", "Симулятор NetDiagnostics", "Simulatore NetDiagnostics", "NetDiagnostics 模拟器", "NetDiagnostics 模擬器", "Simulador NetDiagnostics", "Simulador NetDiagnostics")

    // ── Simulator screen labels ──
    readonly property string targetSection: t("TARGET", "CIBLE", "ZIEL", "ЦЕЛЬ", "OBIETTIVO", "目标", "目標", "OBJETIVO", "ALVO")
    readonly property string testStatusSection: t("TEST STATUS", "ÉTAT DU TEST", "TESTSTATUS", "СТАТУС ТЕСТА", "STATO TEST", "测试状态", "測試狀態", "ESTADO DE PRUEBA", "STATUS DO TESTE")
    readonly property string statusLabel: t("Status:", "Statut :", "Status:", "Статус:", "Stato:", "状态:", "狀態:", "Estado:", "Status:")
    readonly property string progressLabel: t("Progress:", "Progrès :", "Fortschritt:", "Прогресс:", "Progresso:", "进度:", "進度:", "Progreso:", "Progresso:")
    readonly property string logEvidenceSection: t("LOG / EVIDENCE", "JOURNAL / PREUVES", "PROTOKOLL / BEWEISE", "ЖУРНАЛ / ДОКАЗАТЕЛЬСТВА", "LOG / PROVE", "日志 / 证据", "日誌 / 證據", "REGISTRO / EVIDENCIA", "LOG / EVIDÊNCIAS")
    readonly property string logEmptyHint: t("Set a target and click ▶ Run to begin.", "Définissez une cible et cliquez sur ▶ pour commencer.", "Ziel setzen und ▶ klicken.", "Установите цель и нажмите ▶.", "Imposta un target e clicca ▶ per iniziare.", "设置目标并点击 ▶ 运行开始。", "設定目標並點擊 ▶ 運行開始。", "Establezca un objetivo y haga clic en ▶ para comenzar.", "Defina um alvo e clique em ▶ para iniciar.")
    readonly property string idleStatus: t("Idle", "Inactif", "Leerlauf", "Ожидание", "Inattivo", "空闲", "閒置", "Inactivo", "Ocioso")
    readonly property string toastScreenshotSaved: t("Screenshot: ", "Capture : ", "Screenshot: ", "Снимок: ", "Screenshot: ", "截图: ", "截圖: ", "Captura: ", "Captura: ")
}
