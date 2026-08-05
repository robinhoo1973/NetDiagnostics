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
    readonly property string config: t("Configurations", "Configurations", "Konfigurationen", "Конфигурации", "Configurazioni", "配置", "配置", "Configuraciones", "Configurações")
    readonly property string report: t("Report", "Rapport", "Bericht", "Отчёт", "Rapporto", "报告", "報告", "Informe", "Relatório")
    readonly property string settings: t("Settings", "Paramètres", "Einstellungen", "Настройки", "Impostazioni", "设置", "設定", "Ajustes", "Configurações")

    // ── DiagnosticScreen ──
    readonly property string runningDots: t("Running Diagnostics...", "Diagnostics en cours...", "Diagnose läuft...", "Диагностика...", "Diagnostica in corso...", "正在诊断...", "正在診斷...", "Ejecutando diagnósticos...", "Executando diagnósticos...")
    readonly property string complete: t("Diagnostics Complete", "Diagnostic terminé", "Diagnose abgeschlossen", "Диагностика завершена", "Diagnostica completata", "诊断完成", "診斷完成", "Diagnóstico completado", "Diagnóstico concluído")
    readonly property string cancelled: t("Cancelled", "Annulé", "Abgebrochen", "Отменено", "Annullato", "已取消", "已取消", "Cancelado", "Cancelado")
    readonly property string errorCheck: t("Error — Check Target", "Erreur — Vérifier la cible", "Fehler — Ziel prüfen", "Ошибка — Проверьте цель", "Errore — Controlla target", "错误 — 检查目标", "錯誤 — 檢查目標", "Error — Verifique el objetivo", "Erro — Verifique o alvo")
    readonly property string results: t("Results", "Résultats", "Ergebnisse", "Результаты", "Risultati", "结果", "結果", "Resultados", "Resultados")
    // 5WHY: Removed ▶ emoji prefix — DiagnosticToolbar already renders a play
    // icon in the run/stop button.  Duplicating the icon in the label text
    // creates a double-icon visual artifact and wastes horizontal label space.
    readonly property string runDiag: t("Run Diagnostics", "Lancer diagnostic", "Diagnose starten", "Запустить", "Avvia diagnostica", "运行诊断", "運行診斷", "Ejecutar diagnósticos", "Executar diagnósticos")
    readonly property string running: t("Running", "En cours", "Läuft", "Выполняется", "In corso", "运行中", "運行中", "Ejecutando", "Executando")
    // 5WHY: Removed ■ emoji prefix — DiagnosticToolbar renders stop SVG icon.
    readonly property string stop: t("Stop", "Arrêter", "Stopp", "Стоп", "Ferma", "停止", "停止", "Detener", "Parar")
    readonly property string target: t("Target", "Cible", "Ziel", "Цель", "Obiettivo", "目标", "目標", "Objetivo", "Alvo")
    // ── ConfigScreen ──
    readonly property string selectAll: t("Select All", "Tout sélectionner", "Alle auswählen", "Выбрать все", "Seleziona tutto", "全选", "全選", "Seleccionar todo", "Selecionar tudo")
    readonly property string deselectAll: t("Deselect All", "Tout désélectionner", "Alle abwählen", "Отменить все", "Deseleziona tutto", "取消全选", "取消全選", "Deseleccionar todo", "Desmarcar tudo")

    // ── DashboardScreen ──
    readonly property string noData: t("No Diagnostic Data Yet", "Aucune donnée de diagnostic", "Noch keine Diagnosedaten", "Нет данных диагностики", "Nessun dato diagnostico", "暂无诊断数据", "暫無診斷數據", "Aún no hay datos de diagnóstico", "Ainda não há dados de diagnóstico")
    readonly property string runFromDiag: t("Run a diagnostic from the Diagnostics screen\nto see results here.", "Exécutez un diagnostic depuis l'écran Diagnostics\npour voir les résultats ici.", "Führen Sie eine Diagnose vom Diagnosebildschirm aus,\num die Ergebnisse hier zu sehen.", "Запустите диагностику на экране диагностики,\nчтобы увидеть результаты здесь.", "Esegui una diagnostica dalla schermata Diagnostica\nper vedere i risultati qui.", "从诊断屏幕运行诊断\n以在此处查看结果。", "從診斷畫面運行診斷\n以在此處查看結果。", "Ejecute un diagnóstico desde la pantalla de Diagnósticos\npara ver los resultados aquí.", "Execute um diagnóstico na tela de Diagnósticos\npara ver os resultados aqui.")
    readonly property string perGroup: t("Results by Group", "Résultats par groupe", "Ergebnisse pro Gruppe", "Результаты по группам", "Risultati per gruppo", "分组结果", "分組結果", "Resultados por grupo", "Resultados por grupo")
    readonly property string summary: t("Summary", "Résumé", "Zusammenfassung", "Сводка", "Riepilogo", "摘要", "摘要", "Resumen", "Resumo")
    readonly property string totalDiags: t("Total Diagnostics", "Total diagnostics", "Diagnosen insgesamt", "Всего диагностик", "Diagnostiche totali", "总诊断数", "總診斷數", "Diagnósticos totales", "Diagnósticos totais")
    readonly property string totalTime: t("Total Time", "Temps total", "Gesamtzeit", "Общее время", "Tempo totale", "总时间", "總時間", "Tiempo total", "Tempo total")
    readonly property string completed: t("Completed", "Terminé", "Abgeschlossen", "Завершено", "Completato", "已完成", "已完成", "Completado", "Concluído")
    readonly property string layerTimings: t("Layer Timings", "Chronométrage par couche", "Schichtzeiten", "Время по слоям", "Tempi per livello", "层级时间", "層級時間", "Tiempos por capa", "Tempos por camada")

    // ── SettingsScreen ──
    readonly property string aboutDesc: t("A comprehensive cross-platform network diagnostic tool supporting Windows, macOS, Linux, iOS, and Android.", "Un outil de diagnostic réseau multiplateforme prenant en charge Windows, macOS, Linux, iOS et Android.", "Ein umfassendes plattformübergreifendes Netzwerkdiagnosetool für Windows, macOS, Linux, iOS und Android.", "Комплексный кроссплатформенный инструмент сетевой диагностики с поддержкой Windows, macOS, Linux, iOS и Android.", "Uno strumento completo di diagnostica di rete multipiattaforma che supporta Windows, macOS, Linux, iOS e Android.", "一个全面的跨平台网络诊断工具，支持Windows、macOS、Linux、iOS和Android。", "一個全面的跨平台網路診斷工具，支援Windows、macOS、Linux、iOS和Android。", "Una completa herramienta de diagnóstico de red multiplataforma compatible con Windows, macOS, Linux, iOS y Android.", "Uma completa ferramenta de diagnóstico de rede multiplataforma compatível com Windows, macOS, Linux, iOS e Android.")
    readonly property string crossPlat: t("Cross-platform (Windows, macOS, Linux, iOS, Android)", "Multiplateforme (Windows, macOS, Linux, iOS, Android)", "Plattformübergreifend (Windows, macOS, Linux, iOS, Android)", "Кроссплатформенный (Windows, macOS, Linux, iOS, Android)", "Multipiattaforma (Windows, macOS, Linux, iOS, Android)", "跨平台 (Windows, macOS, Linux, iOS, Android)", "跨平台 (Windows, macOS, Linux, iOS, Android)", "Multiplataforma (Windows, macOS, Linux, iOS, Android)", "Multiplataforma (Windows, macOS, Linux, iOS, Android)")
    readonly property string realtimeDiag: t("Real-Time Diagnostic Engine", "Moteur de diagnostic en temps réel", "Echtzeit-Diagnose-Engine", "Движок диагностики в реальном времени", "Motore di diagnostica in tempo reale", "实时诊断引擎", "實時診斷引擎", "Motor de diagnóstico en tiempo real", "Mecanismo de diagnóstico em tempo real")
    readonly property string detailedReport: t("Detailed Reporting and Export", "Rapports détaillés et exportation", "Detaillierte Berichte und Export", "Подробная отчетность и экспорт", "Report dettagliati ed esportazione", "详细报告和导出", "詳細報告和匯出", "Informes detallados y exportación", "Relatórios detalhados e exportação")
    readonly property string darkTheme: t("Dark Theme UI", "Interface thème sombre", "Dunkles Design", "Темная тема", "Interfaccia tema scuro", "深色主题界面", "深色主題介面", "Interfaz con tema oscuro", "Interface com tema escuro")
    readonly property string themeLight:  t("Light",  "Clair",    "Hell",    "Светлая", "Chiaro",  "浅色", "淺色", "Claro",  "Claro")
    readonly property string themeDark:   t("Dark",   "Sombre",   "Dunkel",  "Темная",  "Scuro",   "深色", "深色", "Oscuro", "Escuro")

    // ── ReportScreen ──
    readonly property string reportPreview: t("Report Preview", "Aperçu du rapport", "Berichtsvorschau", "Предпросмотр отчёта", "Anteprima rapporto", "报告预览", "報告預覽", "Vista previa del informe", "Pré-visualização do relatório")
    readonly property string reportSavedTo: t("Saved To:", "Enregistré :", "Gespeichert:", "Сохранено:", "Salvato:", "已保存至：", "已儲存至：", "Guardado en:", "Salvo em:")
    readonly property string reportExportFailed: t("Export Failed", "Échec de l'export.", "Export fehlgeschlagen.", "Ошибка экспорта.", "Esportazione non riuscita.", "导出失败。", "匯出失敗。", "Error al exportar.", "Falha na exportação.")
    readonly property string reportExportHint: t("Export your diagnostic results as a one-page PDF summary or a full HTML report.", "Exportez vos résultats en résumé PDF d'une page ou en rapport HTML complet.", "Exportieren Sie Ihre Ergebnisse als einseitige PDF-Übersicht oder vollständigen HTML-Bericht.", "Экспортируйте результаты как PDF-сводку на одну страницу или полный HTML-отчёт.", "Esporta i risultati come riepilogo PDF di una pagina o rapporto HTML completo.", "将诊断结果导出为一页PDF汇总或完整HTML报告。", "將診斷結果匯出為一頁PDF彙總或完整HTML報告。", "Exporte sus resultados de diagnóstico como un resumen PDF de una página o un informe HTML completo.", "Exporte seus resultados de diagnóstico como um resumo PDF de uma página ou um relatório HTML completo.")
    readonly property string reportRunFirst: t("Run a diagnostic first to generate a report.", "Lancez d'abord un diagnostic pour générer un rapport.", "Führen Sie zuerst eine Diagnose aus, um einen Bericht zu erstellen.", "Сначала запустите диагностику, чтобы создать отчёт.", "Esegui prima una diagnostica per generare un rapporto.", "请先运行诊断以生成报告。", "請先執行診斷以產生報告。", "Ejecute primero un diagnóstico para generar un informe.", "Execute primeiro um diagnóstico para gerar um relatório.")
    readonly property string reportReviewBtn: t("Review Report", "Consulter le rapport", "Bericht anzeigen", "Просмотр отчёта", "Visualizza rapporto", "查看报告", "查看報告", "Revisar informe", "Revisar relatório")
    readonly property string sharePdfBtn: t("Share PDF", "Partager PDF", "PDF teilen", "Поделиться PDF", "Condividi PDF", "分享PDF", "分享PDF", "Compartir PDF", "Compartilhar PDF")
    readonly property string shareHtmlBtn: t("Share HTML", "Partager HTML", "HTML teilen", "Поделиться HTML", "Condividi HTML", "分享HTML", "分享HTML", "Compartir HTML", "Compartilhar HTML")
    readonly property string emailPdfBtn: t("Email PDF", "E-mail PDF", "PDF per E-Mail", "PDF по почте", "Email PDF", "邮件PDF", "郵件PDF", "Correo PDF", "E-mail PDF")
    readonly property string emailHtmlBtn: t("Email HTML", "E-mail HTML", "HTML per E-Mail", "HTML по почте", "Email HTML", "邮件HTML", "郵件HTML", "Correo HTML", "E-mail HTML")
    readonly property string pdfLoading: t("Loading PDF...", "Chargement PDF...", "PDF wird geladen...", "Загрузка PDF...", "Caricamento PDF...", "正在加载PDF...", "正在載入PDF...", "Cargando PDF...", "Carregando PDF...")
    readonly property string pdfLoadFailed: t("Failed to Load PDF", "Échec du chargement PDF", "PDF konnte nicht geladen werden", "Не удалось загрузить PDF", "Caricamento PDF non riuscito", "PDF加载失败", "PDF載入失敗", "Error al cargar PDF", "Falha ao carregar PDF")
    readonly property string shareBtn: t("Share", "Partager", "Teilen", "Поделиться", "Condividi", "分享", "分享", "Compartir", "Compartilhar")
    readonly property string emailBtn: t("Email", "E-mail", "E-Mail", "По почте", "Email", "邮件", "郵件", "Correo", "E-mail")
    readonly property string premiumBadge: t("PRO", "PRO", "PRO", "PRO", "PRO", "PRO", "PRO", "PRO", "PRO")
    readonly property string premiumRequiredMsg: t("Sharing is a premium feature — unlock to share reports.", "Le partage est une fonction premium — déverrouillez pour partager.", "Teilen ist eine Premium-Funktion — zum Teilen freischalten.", "Обмен — премиум-функция. Разблокируйте для обмена.", "La condivisione è una funzione premium — sblocca per condividere.", "分享为付费功能 — 解锁后可分享报告。", "分享為付費功能 — 解鎖後可分享報告。", "Compartir es una función premium — desbloquéela para compartir informes.", "Compartilhar é um recurso premium — desbloqueie para compartilhar relatórios.")
    readonly property string reportShareOk: t("Report Shared", "Rapport partagé.", "Bericht geteilt.", "Отчёт отправлен.", "Rapporto condiviso.", "报告已分享。", "報告已分享。", "Informe compartido.", "Relatório compartilhado.")
    readonly property string reportShareFail: t("Sharing Failed", "Échec du partage.", "Teilen fehlgeschlagen.", "Ошибка обмена.", "Condivisione non riuscita.", "分享失败。", "分享失敗。", "Error al compartir.", "Falha ao compartilhar.")
    readonly property string premiumUnlocked: t("Premium Unlocked", "Premium déverrouillé", "Premium freigeschaltet", "Премиум разблокирован", "Premium sbloccato", "已解锁高级版", "已解鎖高級版", "Premium desbloqueado", "Premium desbloqueado")
    // ── Subscription / share confirmation flow ──
    readonly property string subscribeTitle: t("Premium Feature", "Fonction Premium", "Premium-Funktion", "Премиум-функция", "Funzione Premium", "高级版功能", "高級版功能", "Función Premium", "Recurso Premium")
    readonly property string subscribeBody: t("Sharing and emailing reports is a Premium feature. Unlock it with a one-time purchase that works on all your devices.", "Le partage et l'envoi de rapports par e-mail sont des fonctions Premium. Débloquez-les avec un achat unique valable sur tous vos appareils.", "Das Teilen und Versenden von Berichten ist eine Premium-Funktion. Schalten Sie sie mit einem einmaligen Kauf frei, der auf all Ihren Geräten gilt.", "Отправка отчётов — премиум-функция. Разблокируйте её разовой покупкой, действующей на всех ваших устройствах.", "La condivisione e l'invio dei rapporti è una funzione Premium. Sbloccala con un acquisto una tantum valido su tutti i tuoi dispositivi.", "分享和邮件发送报告是高级版功能。一次性购买即可解锁，所有设备通用。", "分享和郵件發送報告是高級版功能。一次性購買即可解鎖，所有裝置通用。", "Compartir y enviar informes por correo es una función Premium. Desbloquéela con una compra única válida en todos sus dispositivos.", "Compartilhar e enviar relatórios por e-mail é um recurso Premium. Desbloqueie com uma compra única válida em todos os seus dispositivos.")
    readonly property string subscribeBtn: t("Unlock Premium", "Débloquer Premium", "Premium freischalten", "Разблокировать", "Sblocca Premium", "购买高级版", "購買高級版", "Desbloquear Premium", "Desbloquear Premium")
    readonly property string subscribeNotNow: t("Not Now", "Plus tard", "Später", "Не сейчас", "Non ora", "以后再说", "以後再說", "Ahora no", "Agora não")
    readonly property string restoreBtn: t("Restore Purchases", "Restaurer les achats", "Käufe wiederherstellen", "Восстановить покупки", "Ripristina acquisti", "恢复购买", "恢復購買", "Restaurar compras", "Restaurar compras")
    readonly property string restoreOk: t("Purchases Restored", "Achats restaurés.", "Käufe wiederhergestellt.", "Покупки восстановлены.", "Acquisti ripristinati.", "购买已恢复。", "購買已恢復。", "Compras restauradas.", "Compras restauradas.")
    readonly property string restoreFail: t("No Previous Purchases Found", "Aucun achat précédent trouvé.", "Keine früheren Käufe gefunden.", "Предыдущие покупки не найдены.", "Nessun acquisto precedente trovato.", "未找到之前的购买记录。", "未找到之前的購買記錄。", "No se encontraron compras anteriores.", "Nenhuma compra anterior encontrada.")
    readonly property string restoreError: t("Restore failed. Please try again.", "Échec de la restauration. Veuillez réessayer.", "Wiederherstellung fehlgeschlagen. Bitte versuchen Sie es erneut.", "Ошибка восстановления. Попробуйте снова.", "Ripristino non riuscito. Riprova.", "恢复失败，请重试。", "恢復失敗，請重試。", "Error al restaurar. Inténtelo de nuevo.", "Falha ao restaurar. Tente novamente.")
    readonly property string confirmShareTitle: t("Share Report", "Partager le rapport", "Bericht teilen", "Поделиться отчётом", "Condividi rapporto", "分享报告", "分享報告", "Compartir informe", "Compartilhar relatório")
    readonly property string confirmShareBody: t("Share this diagnostic report now?", "Partager ce rapport de diagnostic maintenant ?", "Diesen Diagnosebericht jetzt teilen?", "Поделиться этим отчётом диагностики?", "Condividere ora questo rapporto diagnostico?", "确认现在分享此诊断报告？", "確認現在分享此診斷報告？", "¿Compartir este informe de diagnóstico ahora?", "Compartilhar este relatório de diagnóstico agora?")
    readonly property string dialogCancel: t("Cancel", "Annuler", "Abbrechen", "Отмена", "Annulla", "取消", "取消", "Cancelar", "Cancelar")
    readonly property string reportResultsAvailable: t(" results available", " résultats disponibles", " Ergebnisse verfügbar", " результатов", " risultati disponibili", " 个结果可用", " 個結果可用", " resultados disponibles", " resultados disponíveis")
    readonly property string reportNoResults: t("No Diagnostic Results", "Aucun résultat de diagnostic", "Keine Diagnoseergebnisse", "Нет результатов", "Nessun risultato", "无诊断结果", "無診斷結果", "Sin resultados de diagnóstico", "Sem resultados de diagnóstico")
    readonly property string cellularWarnTitle: t("Mobile Data Warning", "Avertissement données mobiles", "Mobile Datenwarnung", "Предупреждение о мобильных данных", "Avviso dati mobili", "移动数据警告", "流動數據警告", "Advertencia de datos móviles", "Aviso de dados móveis")
    readonly property string cellularWarnBody: t("You are on cellular data. G3 Internet tests may consume data.\nContinue?", "Vous utilisez les données mobiles. Les tests Internet G3 peuvent consommer des données.\nContinuer ?", "Sie nutzen mobile Daten. G3-Internettests können Daten verbrauchen.\nFortfahren?", "Вы используете мобильные данные. Интернет-тесты G3 могут расходовать трафик.\nПродолжить?", "Stai usando dati mobili. I test Internet G3 potrebbero consumare dati.\nContinuare?", "您正在使用移动数据。G3 互联网测试可能会消耗流量。\n继续？", "您正在使用流動數據。G3 互聯網測試可能會消耗流量。\n繼續？", "Estás usando datos móviles. Las pruebas G3 pueden consumir datos.\n¿Continuar?", "Você está usando dados móveis. Os testes G3 podem consumir dados.\nContinuar?")
    readonly property string cellularCancel: t("Cancel", "Annuler", "Abbrechen", "Отмена", "Annulla", "取消", "取消", "Cancelar", "Cancelar")
    readonly property string cellularContinue: t("Continue", "Continuer", "Fortfahren", "Продолжить", "Continua", "继续", "繼續", "Continuar", "Continuar")

    // ── Target Analysis ──
    readonly property string targetAnalysis: t("Target Analysis", "Analyse de la cible", "Zielanalyse", "Анализ цели", "Analisi obiettivo", "目标分析", "目標分析", "Análisis del objetivo", "Análise do alvo")
    readonly property string knownPortRef: t("Known Port Reference", "Référence des ports connus", "Bekannte Ports Referenz", "Справочник портов", "Riferimento porte note", "已知端口参考", "已知埠參考", "Referencia de puertos conocidos", "Referência de portas conhecidas")
    readonly property string targetTypeLabel: t("Type    :", "Type    :", "Typ     :", "Тип     :", "Tipo    :", "类型    :", "類型    :", "Tipo    :", "Tipo    :")
    readonly property string targetTypeUrl: t("URL", "URL", "URL", "URL", "URL", "URL", "URL", "URL", "URL")
    readonly property string targetTypeIp: t("Remote Host (IP)", "Hôte distant (IP)", "Remote Host (IP)", "Удаленный хост (IP)", "Host remoto (IP)", "远程主机(IP)", "遠端主機(IP)", "Host remoto (IP)", "Host remoto (IP)")
    readonly property string targetTypeHostname: t("Remote Host (Hostname)", "Hôte distant (nom)", "Remote Host (Hostname)", "Удаленный хост (имя)", "Host remoto (nome)", "远程主机(主机名)", "遠端主機(主機名)", "Host remoto (nombre)", "Host remoto (nome)")

    // ── URL component labels (TargetAnalysisPanel breakdown) ──
    readonly property string urlSchemeLabel:   t("Scheme", "Protocole", "Schema", "Схема", "Schema", "协议", "協定", "Esquema", "Esquema")
    readonly property string urlUserLabel:     t("User", "Utilisateur", "Benutzer", "Пользователь", "Utente", "用户", "使用者", "Usuario", "Usuário")
    readonly property string urlHostLabel:     t("Host", "Hôte", "Host", "Хост", "Host", "主机", "主機", "Host", "Host")
    readonly property string urlPortLabel:     t("Port", "Port", "Port", "Порт", "Porta", "端口", "連接埠", "Puerto", "Porta")
    readonly property string urlPathLabel:     t("Path", "Chemin", "Pfad", "Путь", "Percorso", "路径", "路徑", "Ruta", "Caminho")
    readonly property string urlQueryLabel:    t("Query", "Requête", "Abfrage", "Запрос", "Query", "查询参数", "查詢參數", "Consulta", "Consulta")
    readonly property string urlFragmentLabel: t("Fragment", "Fragment", "Fragment", "Фрагмент", "Frammento", "片段", "片段", "Fragmento", "Fragmento")

    // ── Live Progress ──
    readonly property string errorPrefix: t("Error: ", "Erreur : ", "Fehler: ", "Ошибка: ", "Errore: ", "错误: ", "錯誤: ", "Error: ", "Erro: ")
    readonly property string runningStatus: t("Running", "En cours", "Läuft", "Выполняется", "In corso", "运行中", "運行中", "En ejecución", "Em execução")
    readonly property string completeStatus: t("Completed", "Terminé", "Abgeschlossen", "Завершено", "Completato", "完成", "完成", "Completado", "Concluído")
    readonly property string cancelledStatus: cancelled  // alias — DRY, single canonical definition
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

    // ── Translate C++ UI messages (validation + run errors) ────────────
    // Diagnostic result *detail* text is exempt (generated by C++ diagnostics);
    // these are UI-level messages surfaced via appState.errorMessage and
    // appState.targetValidationErrorText.  Exact match first, then prefix
    // match for the two parameterized messages.
    function trMsg(en) {
        if (lang <= 0 || en === "") return en
        var exact = {
            "Empty URL scheme": t("Empty URL scheme", "Schéma d'URL vide", "Leeres URL-Schema", "Пустая схема URL", "Schema URL vuoto", "URL 协议为空", "URL 協定為空", "Esquema de URL vacío", "Esquema de URL vazio"),
            "URL has no hostname": t("URL has no hostname", "L'URL n'a pas de nom d'hôte", "URL hat keinen Hostnamen", "В URL отсутствует имя хоста", "L'URL non ha un hostname", "URL 缺少主机名", "URL 缺少主機名稱", "La URL no tiene nombre de host", "A URL não tem nome de host"),
            "URL has no hostname after userinfo": t("URL has no hostname after userinfo", "Pas de nom d'hôte après les informations utilisateur", "Kein Hostname nach Userinfo", "Нет имени хоста после userinfo", "Nessun hostname dopo userinfo", "用户信息之后缺少主机名", "使用者資訊之後缺少主機名稱", "Sin nombre de host después de userinfo", "Sem nome de host após userinfo"),
            "Invalid IPv6 bracket notation": t("Invalid IPv6 bracket notation", "Notation IPv6 entre crochets invalide", "Ungültige IPv6-Klammernotation", "Недопустимая скобочная запись IPv6", "Notazione tra parentesi IPv6 non valida", "IPv6 方括号表示法无效", "IPv6 方括號表示法無效", "Notación de corchetes IPv6 no válida", "Notação de colchetes IPv6 inválida"),
            "Expected colon after IPv6 bracket": t("Expected colon after IPv6 bracket", "Deux-points attendu après le crochet IPv6", "Doppelpunkt nach IPv6-Klammer erwartet", "Ожидается двоеточие после скобки IPv6", "Atteso due punti dopo la parentesi IPv6", "IPv6 方括号后应为冒号", "IPv6 方括號後應為冒號", "Se esperaban dos puntos después del corchete IPv6", "Dois pontos esperados após o colchete IPv6"),
            "Invalid hostname: consecutive dots": t("Invalid hostname: consecutive dots", "Nom d'hôte invalide : points consécutifs", "Ungültiger Hostname: aufeinanderfolgende Punkte", "Недопустимое имя хоста: последовательные точки", "Hostname non valido: punti consecutivi", "无效的主机名：连续的点", "無效的主機名稱：連續的點", "Nombre de host no válido: puntos consecutivos", "Nome de host inválido: pontos consecutivos"),
            "Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen": t("Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen", "Le label du nom d'hôte doit contenir 1 à 63 caractères alphanumériques (a-z, 0-9, -) et ne peut ni commencer ni finir par un trait d'union", "Hostname-Label muss 1-63 alphanumerische Zeichen (a-z, 0-9, -) enthalten und darf nicht mit Bindestrich beginnen oder enden", "Метка имени хоста должна содержать 1-63 буквенно-цифровых символа (a-z, 0-9, -) и не может начинаться или заканчиваться дефисом", "L'etichetta dell'hostname deve contenere 1-63 caratteri alfanumerici (a-z, 0-9, -) e non può iniziare o terminare con un trattino", "主机名标签须为 1-63 个字母数字字符（a-z、0-9、-），且不能以连字符开头或结尾", "主機名稱標籤須為 1-63 個字母數字字元（a-z、0-9、-），且不能以連字號開頭或結尾", "La etiqueta del nombre de host debe tener entre 1 y 63 caracteres alfanuméricos (a-z, 0-9, -) y no puede comenzar ni terminar con un guion", "O rótulo do nome de host deve ter 1-63 caracteres alfanuméricos (a-z, 0-9, -) e não pode começar nem terminar com hífen"),
            "Port must be a number": t("Port must be a number", "Le port doit être un nombre", "Port muss eine Zahl sein", "Порт должен быть числом", "La porta deve essere un numero", "端口必须是数字", "連接埠必須是數字", "El puerto debe ser un número", "A porta deve ser um número"),
            "No diagnostic tests are enabled. Check Config.": t("No diagnostic tests are enabled. Check Config.", "Aucun test de diagnostic activé. Vérifiez la configuration.", "Keine Diagnosetests aktiviert. Konfiguration prüfen.", "Диагностические тесты не включены. Проверьте конфигурацию.", "Nessun test di diagnostica abilitato. Controlla la configurazione.", "未启用任何诊断测试。请检查配置。", "未啟用任何診斷測試。請檢查配置。", "No hay pruebas de diagnóstico habilitadas. Revise la configuración.", "Nenhum teste de diagnóstico habilitado. Verifique a configuração."),
            "No target specified and no local tests enabled. Enter a target or enable tests in Config.": t("No target specified and no local tests enabled. Enter a target or enable tests in Config.", "Aucune cible spécifiée et aucun test local activé. Saisissez une cible ou activez des tests dans la configuration.", "Kein Ziel angegeben und keine lokalen Tests aktiviert. Geben Sie ein Ziel ein oder aktivieren Sie Tests in der Konfiguration.", "Цель не указана и локальные тесты не включены. Укажите цель или включите тесты в конфигурации.", "Nessun target specificato e nessun test locale abilitato. Inserisci un target o abilita i test nella configurazione.", "未指定目标且未启用本地测试。请输入目标或在配置中启用测试。", "未指定目標且未啟用本地測試。請輸入目標或在配置中啟用測試。", "No se especificó ningún objetivo ni se habilitaron pruebas locales. Introduzca un objetivo o habilite pruebas en la configuración.", "Nenhum alvo especificado e nenhum teste local habilitado. Insira um alvo ou habilite testes na configuração.")
        }
        if (exact[en] !== undefined) return exact[en]

        // Parameterized: "Unsupported protocol: <scheme>:// — supported schemes: <list>"
        var unsupPrefix = "Unsupported protocol: "
        var unsupSep = ":// — supported schemes: "
        if (en.indexOf(unsupSep) > 0 && en.indexOf(unsupPrefix) === 0) {
            var scheme = en.substring(unsupPrefix.length, en.indexOf(unsupSep))
            var schemeList = en.substring(en.indexOf(unsupSep) + unsupSep.length)
            return t("Unsupported protocol: %1:// — supported schemes: %2",
                     "Protocole non pris en charge : %1:// — schémas pris en charge : %2",
                     "Nicht unterstütztes Protokoll: %1:// — unterstützte Schemata: %2",
                     "Неподдерживаемый протокол: %1:// — поддерживаемые схемы: %2",
                     "Protocollo non supportato: %1:// — schemi supportati: %2",
                     "不支持的协议：%1:// — 支持的协议：%2",
                     "不支援的協定：%1:// — 支援的協定：%2",
                     "Protocolo no compatible: %1:// — esquemas compatibles: %2",
                     "Protocolo não suportado: %1:// — esquemas suportados: %2")
                .replace("%1", scheme).replace("%2", schemeList)
        }

        // Parameterized: "Port must be between 1 and 65535 (got <n>)"
        var portPrefix = "Port must be between 1 and 65535 (got "
        if (en.indexOf(portPrefix) === 0 && en.charAt(en.length - 1) === ")") {
            var gotPort = en.substring(portPrefix.length, en.length - 1)
            return t("Port must be between 1 and 65535 (got %1)",
                     "Le port doit être compris entre 1 et 65535 (reçu : %1)",
                     "Port muss zwischen 1 und 65535 liegen (erhalten: %1)",
                     "Порт должен быть в диапазоне 1-65535 (получено: %1)",
                     "La porta deve essere compresa tra 1 e 65535 (ricevuto: %1)",
                     "端口必须在 1 到 65535 之间（当前值：%1）",
                     "連接埠必須介於 1 到 65535 之間（目前值：%1）",
                     "El puerto debe estar entre 1 y 65535 (recibido: %1)",
                     "A porta deve estar entre 1 e 65535 (recebido: %1)")
                .replace("%1", gotPort)
        }

        return en
    }

    // ── Dashboard ──
    readonly property string diagRunComplete: t("Diagnostics Complete", "Diagnostic terminé", "Diagnoselauf abgeschlossen", "Диагностика завершена", "Corsa diagnostica completata", "诊断运行完成", "診斷運行完成", "Ejecución de diagnóstico completada", "Execução de diagnóstico concluída")
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
    readonly property string diagRunning: t("Running", "En cours", "Läuft", "Выполняется", "In corso", "运行中", "運行中", "Ejecutando", "Executando")

    // placeholderMsg removed — SMTP feature deprecated

    // ── Test names (46 entries, ids 0-45) ──
    // 5WHY: previously returned \"\" for English (lang<=0), forcing a
    // fallback to C++ diagDisplayName().  That C++ path was not reliably
    // tracked by QML's binding dependency system when called through a
    // JS function chain from ConfigScreen's getDisplayName().  Now uses
    // the same t()-based path as diagDesc() — English is the first arg
    // to every t() call, returned by t() itself when lang<=0.
    function diagName(id) {
        var names = {
            0:  t("Network Adapters", "Adaptateurs réseau", "Netzwerkadapter", "Сетевые адаптеры", "Schede di rete", "网络适配器", "網路適配器", "Adaptadores de red", "Adaptadores de rede"),
            1:  t("NIC Advanced", "Carte réseau avancée", "Erweiterte NIC", "NIC расширенный", "NIC avanzata", "网卡高级信息", "NIC進階資訊", "NIC avanzada", "NIC avançada"),
            2:  t("WiFi Information", "Informations WiFi", "WLAN-Information", "Информация о WiFi", "Informazioni WiFi", "WiFi信息", "WiFi資訊", "Información WiFi", "Informações WiFi"),
            3:  t("Wired Information", "Informations filaires", "Kabelgebundene Informationen", "Информация о проводной сети", "Informazioni cablate", "有线信息", "有線資訊", "Información por cable", "Informações com fio"),
            4:  t("DHCP Status", "Statut DHCP", "DHCP-Status", "Статус DHCP", "Stato DHCP", "DHCP状态", "DHCP狀態", "Estado DHCP", "Estado DHCP"),
            5:  t("IP Configuration", "Configuration IP", "IP-Konfiguration", "IP конфигурация", "Configurazione IP", "IP配置", "IP配置", "Configuración IP", "Configuração IP"),
            6:  t("Active Connections", "Connexions actives", "Aktive Verbindungen", "Активные соединения", "Connessioni attive", "活动连接", "活動連接", "Conexiones activas", "Conexões ativas"),
            7:  t("Cellular Information", "Informations cellulaires", "Mobilfunkinformationen", "Информация о сотовой сети", "Informazioni cellulari", "蜂窝信息", "蜂窩資訊", "Información celular", "Informações celulares"),
            8:  t("Network Profile", "Profil réseau", "Netzwerkprofil", "Сетевой профиль", "Profilo di rete", "网络配置文件", "網路設定檔", "Perfil de red", "Perfil de rede"),
            9:  t("TCP Settings", "Paramètres TCP", "TCP-Einstellungen", "Настройки TCP", "Impostazioni TCP", "TCP设置", "TCP設定", "Configuración TCP", "Configurações TCP"),
            10: t("Default Gateway", "Passerelle par défaut", "Standardgateway", "Шлюз по умолчанию", "Gateway predefinito", "默认网关", "默認閘道", "Puerta de enlace predeterminada", "Gateway padrão"),
            11: t("Routing Table", "Table de routage", "Routingtabelle", "Таблица маршрутизации", "Tabella di routing", "路由表", "路由表", "Tabla de enrutamiento", "Tabela de roteamento"),
            12: t("ARP Table", "Table ARP", "ARP-Tabelle", "ARP таблица", "Tabella ARP", "ARP表", "ARP表", "Tabla ARP", "Tabela ARP"),
            13: t("Proxy Settings", "Paramètres proxy", "Proxy-Einstellungen", "Настройки прокси", "Impostazioni proxy", "代理设置", "代理設定", "Configuración de proxy", "Configurações de proxy"),
            14: t("Netskope Status", "Statut Netskope", "Netskope-Status", "Статус Netskope", "Stato Netskope", "Netskope状态", "Netskope狀態", "Estado de Netskope", "Estado do Netskope"),
            15: t("DNS Servers", "Serveurs DNS", "DNS-Server", "DNS серверы", "Server DNS", "DNS服务器", "DNS伺服器", "Servidores DNS", "Servidores DNS"),
            16: t("DNS Cache", "Cache DNS", "DNS-Cache", "DNS кэш", "Cache DNS", "DNS缓存", "DNS快取", "Caché DNS", "Cache DNS"),
            17:  t("DNS Integrity", "Intégrité DNS", "DNS-Integrität", "Целостность DNS", "Integrità DNS", "DNS完整性", "DNS完整性", "Integridad DNS", "Integridade DNS"),
            18: t("IP Geolocation", "Géolocalisation IP", "IP-Geolokalisierung", "IP Геолокация", "Geolocalizzazione IP", "IP地理定位", "IP地理定位", "Geolocalización IP", "Geolocalização IP"),
            19: t("Internet Connectivity & Speed", "Connectivité et débit", "Internetverbindung & Geschwindigkeit", "Интернет и скорость", "Connettività e velocità", "互联网连接与速度", "網際網路連線與速度", "Conectividad y velocidad de Internet", "Conectividade e velocidade da Internet"),
            20: t("DNS Resolution", "Résolution DNS", "DNS-Auflösung", "DNS разрешение", "Risoluzione DNS", "DNS解析", "DNS解析", "Resolución DNS", "Resolução DNS"),
            21: t("Ping", "Ping", "Ping", "Пинг", "Ping", "Ping", "Ping", "Ping", "Ping"),
            22: t("Traceroute", "Traceroute", "Traceroute", "Трассировка", "Traceroute", "路由追踪", "路由追蹤", "Traceroute", "Traceroute"),
            23: t("PathPing", "PathPing", "PathPing", "PathPing", "PathPing", "路径Ping", "路徑Ping", "PathPing", "PathPing"),
            24: t("MTU Discovery", "Découverte MTU", "MTU-Erkennung", "MTU обнаружение", "Scoperta MTU", "MTU发现", "MTU發現", "Descubrimiento de MTU", "Descoberta de MTU"),
            25: t("IPv6 Connectivity", "Connectivité IPv6", "IPv6-Konnektivität", "IPv6 подключение", "Connettività IPv6", "IPv6连接", "IPv6連線", "Conectividad IPv6", "Conectividade IPv6"),
            26: t("URL Parsing", "Analyse d'URL", "URL-Analyse", "Разбор URL", "Analisi URL", "URL解析", "URL解析", "Análisis de URL", "Análise de URL"),
            27: t("TCP Connect", "Connexion TCP", "TCP-Verbindung", "TCP соединение", "Connessione TCP", "TCP连接", "TCP連接", "Conexión TCP", "Conexão TCP"),
            28: t("Service Banner", "Bannière de service", "Service-Banner", "Баннер сервиса", "Banner del servizio", "服务标识", "服務標識", "Banner de servicio", "Banner de serviço"),
            29: t("HTTP Request", "Requête HTTP", "HTTP-Anfrage", "HTTP запрос", "Richiesta HTTP", "HTTP请求", "HTTP請求", "Solicitud HTTP", "Requisição HTTP"),
            30: t("HTTP Headers", "En-têtes HTTP", "HTTP-Header", "HTTP заголовки", "Intestazioni HTTP", "HTTP头", "HTTP標頭", "Encabezados HTTP", "Cabeçalhos HTTP"),
            31: t("Security Headers", "En-têtes de sécurité", "Sicherheitsheader", "Заголовки безопасности", "Intestazioni sicurezza", "安全头", "安全標頭", "Encabezados de seguridad", "Cabeçalhos de segurança"),
            32: t("SSL Certificate", "Certificat SSL", "SSL-Zertifikat", "SSL сертификат", "Certificato SSL", "SSL证书", "SSL憑證", "Certificado SSL", "Certificado SSL"),
            33: t("HTTP Redirect", "Redirection HTTP", "HTTP-Weiterleitung", "HTTP редирект", "Reindirizzamento HTTP", "HTTP重定向", "HTTP重定向", "Redirección HTTP", "Redirecionamento HTTP"),
            34: t("HTTP Compression", "Compression HTTP", "HTTP-Komprimierung", "HTTP сжатие", "Compressione HTTP", "HTTP压缩", "HTTP壓縮", "Compresión HTTP", "Compressão HTTP"),
            35: t("HTTP Timing", "Chronométrage HTTP", "HTTP-Timing", "HTTP тайминг", "Temporizzazione HTTP", "HTTP计时", "HTTP計時", "Tiempos HTTP", "Tempos HTTP"),
            36: t("FTP Diagnostics", "Diagnostics FTP", "FTP-Diagnose", "FTP диагностика", "Diagnostica FTP", "FTP诊断", "FTP診斷", "Diagnóstico FTP", "Diagnóstico FTP"),
            37: t("SSH Diagnostics", "Diagnostics SSH", "SSH-Diagnose", "SSH диагностика", "Diagnostica SSH", "SSH诊断", "SSH診斷", "Diagnóstico SSH", "Diagnóstico SSH"),
            38: t("Email Diagnostics", "Diagnostics email", "E-Mail-Diagnose", "Диагностика почты", "Diagnostica email", "电子邮件诊断", "電子郵件診斷", "Diagnóstico de correo", "Diagnóstico de e-mail"),
            39: t("Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet", "Telnet"),
            40: t("MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL", "MySQL"),
            41: t("PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL", "PostgreSQL"),
            42: t("Redis", "Redis", "Redis", "Redis", "Redis", "Redis", "Redis", "Redis", "Redis"),
            43: t("MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB", "MongoDB"),
            44: t("LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP", "LDAP"),
            45: t("MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT", "MQTT"),
        }
        return typeof names[id] === 'string' ? names[id] : ""
    }
    // ── Test descriptions (46 entries, ids 0-45) ──
    function diagDesc(id) {
        // 5WHY: No early-return for English — the EN column is the canonical
        // English source (ConfigScreen previously duplicated it in _enDescs).
        // t() already returns the EN argument when lang<=0.
        var descs = {
            0:  t("List All Network Adapters and Their Operational State", "Lister toutes les cartes réseau et leur état", "Alle Netzwerkadapter und deren Betriebszustand auflisten", "Список всех сетевых адаптеров и их состояние", "Elenca tutte le schede di rete e il loro stato", "列出所有网络适配器及其运行状态", "列出所有網路適配器及其運行狀態", "Enumera todos los adaptadores de red y su estado operativo", "Lista todos os adaptadores de rede e seu estado operacional"),
            1:  t("Driver Version, Hardware Info, and Negotiated Link Speed", "Version du pilote, infos matérielles et vitesse de liaison", "Treiberversion, Hardware-Info und ausgehandelte Verbindungsgeschwindigkeit", "Версия драйвера, информация об оборудовании и скорость соединения", "Versione driver, info hardware e velocità di collegamento", "驱动程序版本、硬件信息和协商链路速度", "驅動程式版本、硬體資訊和協商鏈路速度", "Versión del controlador, información de hardware y velocidad de enlace negociada", "Versão do driver, informações de hardware e velocidade de link negociada"),
            2:  t("Signal Strength, SSID, Channel, and Link Quality", "Force du signal, SSID, canal et qualité de liaison", "Signalstärke, SSID, Kanal und Verbindungsqualität", "Уровень сигнала, SSID, канал и качество связи", "Potenza segnale, SSID, canale e qualità collegamento", "信号强度、SSID、信道和链路质量", "訊號強度、SSID、頻道和鏈路品質", "Intensidad de señal, SSID, canal y calidad del enlace", "Intensidade do sinal, SSID, canal e qualidade do link"),
            3:  t("Ethernet Link Status, Speed, and Duplex Mode", "État de la liaison Ethernet, vitesse et mode duplex", "Ethernet-Verbindungsstatus, Geschwindigkeit und Duplexmodus", "Статус Ethernet соединения, скорость и дуплексный режим", "Stato collegamento Ethernet, velocità e modalità duplex", "以太网链路状态、速度和双工模式", "乙太網鏈路狀態、速度和雙工模式", "Estado del enlace Ethernet, velocidad y modo dúplex", "Estado do link Ethernet, velocidade e modo duplex"),
            4:  t("DHCP Lease Info, Server Address, and Expiration", "Infos de bail DHCP, adresse du serveur et expiration", "DHCP-Lease-Info, Serveradresse und Ablauf", "Информация о DHCP аренде, адрес сервера и срок действия", "Info lease DHCP, indirizzo server e scadenza", "DHCP租约信息、服务器地址和过期时间", "DHCP租約資訊、伺服器位址和過期時間", "Información de concesión DHCP, dirección del servidor y expiración", "Informações de concessão DHCP, endereço do servidor e expiração"),
            5:  t("IP Addresses, Subnet Mask, Default Gateway, DNS Servers", "Adresses IP, masque de sous-réseau, passerelle, serveurs DNS", "IP-Adressen, Subnetzmaske, Standardgateway, DNS-Server", "IP адреса, маска подсети, шлюз по умолчанию, DNS серверы", "Indirizzi IP, subnet mask, gateway predefinito, server DNS", "IP地址、子网掩码、默认网关、DNS服务器", "IP位址、子網路遮罩、預設閘道、DNS伺服器", "Direcciones IP, máscara de subred, puerta de enlace predeterminada, servidores DNS", "Endereços IP, máscara de sub-rede, gateway padrão, servidores DNS"),
            6:  t("TCP/UDP Connections: ESTABLISHED, LISTENING, etc.", "Connexions TCP/UDP: ÉTABLIES, EN ÉCOUTE, etc.", "TCP/UDP-Verbindungen: HERGESTELLT, HÖREND, usw.", "TCP/UDP соединения: УСТАНОВЛЕНО, ПРОСЛУШИВАЕТСЯ и т.д.", "Connessioni TCP/UDP: STABILITE, IN ASCOLTO, ecc.", "TCP/UDP连接：已建立、监听等", "TCP/UDP連線：已建立、監聽等", "Conexiones TCP/UDP: ESTABLECIDA, ESCUCHANDO, etc.", "Conexões TCP/UDP: ESTABELECIDA, ESCUTANDO, etc."),
            7:  t("Cellular Network Type, Signal Strength, Carrier, and Radio Access Technology", "Type de réseau cellulaire, force du signal, opérateur et technologie d''accès radio", "Mobilfunknetztyp, Signalstärke, Anbieter und Funkzugangstechnologie", "Тип сотовой сети, уровень сигнала, оператор и технология радиодоступа", "Tipo di rete cellulare, potenza del segnale, operatore e tecnologia di accesso radio", "蜂窝网络类型、信号强度、运营商和无线接入技术", "蜂窩網路類型、訊號強度、電信業者和無線接入技術", "Tipo de red celular, intensidad de señal, operador y tecnología de acceso radio", "Tipo de rede celular, intensidade do sinal, operadora e tecnologia de acesso via rádio"),
            8:  t("Active Network Profile Type (Domain/Private/Public)", "Type de profil réseau actif (Domaine/Privé/Public)", "Aktiver Netzwerkprofiltyp (Domäne/Privat/Öffentlich)", "Тип активного сетевого профиля (Доменный/Частный/Общественный)", "Tipo profilo rete attivo (Dominio/Privato/Pubblico)", "活动网络配置文件类型（域/专用/公用）", "活動網路設定檔類型（網域/私人/公用）", "Tipo de perfil de red activo (Dominio/Privado/Público)", "Tipo de perfil de rede ativo (Domínio/Privado/Público)"),
            9:  t("TCP/IP Stack Parameters and Configurations", "Paramètres et configuration de la pile TCP/IP", "TCP/IP-Stack-Parameter und Konfiguration", "Параметры и конфигурация стека TCP/IP", "Parametri e configurazione stack TCP/IP", "TCP/IP堆栈参数和配置", "TCP/IP堆疊參數和組態", "Parámetros y configuraciones de la pila TCP/IP", "Parâmetros e configurações da pilha TCP/IP"),
            10: t("Default Gateway Reachability and Response Time", "Accessibilité et temps de réponse de la passerelle", "Erreichbarkeit und Antwortzeit des Standardgateways", "Доступность и время отклика шлюза по умолчанию", "Raggiungibilità e tempo risposta gateway predefinito", "默认网关可达性和响应时间", "預設閘道可達性和回應時間", "Accesibilidad y tiempo de respuesta de la puerta de enlace predeterminada", "Acessibilidade e tempo de resposta do gateway padrão"),
            11: t("IPv4 and IPv6 Routing Table Entries", "Entrées de la table de routage IPv4 et IPv6", "IPv4- und IPv6-Routingtabelleneinträge", "Записи таблицы маршрутизации IPv4 и IPv6", "Voci tabella routing IPv4 e IPv6", "IPv4和IPv6路由表条目", "IPv4和IPv6路由表條目", "Entradas de la tabla de enrutamiento IPv4 e IPv6", "Entradas da tabela de roteamento IPv4 e IPv6"),
            12: t("ARP Cache Entries for Local Network Discovery", "Entrées du cache ARP pour découverte réseau local", "ARP-Cache-Einträge für lokale Netzwerkerkennung", "Записи ARP кэша для обнаружения локальной сети", "Voci cache ARP per rilevamento rete locale", "ARP缓存条目用于本地网络发现", "ARP快取條目用於本地網路探索", "Entradas de caché ARP para el descubrimiento de la red local", "Entradas de cache ARP para descoberta da rede local"),
            13: t("System Proxy Configuration and Auto-Detection", "Configuration et détection automatique du proxy système", "System-Proxy-Konfiguration und Auto-Erkennung", "Конфигурация системного прокси и автоопределение", "Configurazione proxy di sistema e rilevamento automatico", "系统代理配置和自动检测", "系統代理組態和自動偵測", "Configuración del proxy del sistema y detección automática", "Configuração de proxy do sistema e detecção automática"),
            14: t("Netskope Client Status and Connection Health", "Statut du client Netskope et santé de la connexion", "Netskope-Client-Status und Verbindungszustand", "Статус клиента Netskope и состояние соединения", "Stato client Netskope e salute connessione", "Netskope客户端状态和连接健康", "Netskope用戶端狀態和連線健康", "Estado del cliente Netskope y salud de la conexión", "Estado do cliente Netskope e integridade da conexão"),
            15: t("Configured DNS Servers and Their Responsiveness", "Serveurs DNS configurés et leur réactivité", "Konfigurierte DNS-Server und deren Reaktionsfähigkeit", "Настроенные DNS серверы и их отзывчивость", "Server DNS configurati e loro reattività", "配置的DNS服务器及其响应性", "設定的DNS伺服器及其回應性", "Servidores DNS configurados y su capacidad de respuesta", "Servidores DNS configurados e sua capacidade de resposta"),
            16: t("DNS Resolver Cache Entries and Statistics", "Entrées et statistiques du cache du résolveur DNS", "DNS-Resolver-Cache-Einträge und Statistiken", "Записи и статистика кэша DNS резолвера", "Voci e statistiche cache resolver DNS", "DNS解析器缓存条目和统计", "DNS解析器快取條目和統計", "Entradas y estadísticas de la caché del resolvedor DNS", "Entradas e estatísticas do cache do resolvedor DNS"),
            17: t("Check for DNS Hijacking or Spoofing Indicators", "Vérification des indicateurs de détournement DNS", "Prüfung auf DNS-Hijacking oder Spoofing", "Проверка на признаки перехвата DNS", "Controllo indicatori dirottamento DNS", "检查DNS劫持或欺骗指标", "檢查DNS劫持或欺騙指標", "Comprobar indicadores de secuestro o suplantación de DNS", "Verificar indicadores de sequestro ou falsificação de DNS"),
            18: t("IP Geolocation: Finds Physical Location via Server Latency + Detects VPN", "Géolocalisation IP : position physique via latence + détection VPN", "IP-Geolokalisierung: physischer Standort via Latenz + VPN-Erkennung", "IP Геолокация: определение местоположения через задержку + обнаружение VPN", "Geolocalizzazione IP: posizione fisica via latenza + rilevamento VPN", "IP地理定位: 通过服务器延迟确定物理位置 + 检测VPN", "IP地理定位: 通過伺服器延遲確定物理位置 + 檢測VPN", "Geolocalización IP: ubicación física por latencia + detección VPN", "Geolocalização IP: localização física via latência + detecção VPN"),
            19: t("Connectivity Check + Speedtest.net Bandwidth Test", "Test de connectivité + bande passante Speedtest.net", "Konnektivitätsprüfung + Speedtest.net-Bandbreitentest", "Проверка соединения + тест скорости Speedtest.net", "Controllo connettività + test larghezza di banda Speedtest.net", "连接检查+Speedtest.net带宽测试", "連線檢查+Speedtest.net頻寬測試", "Comprobación de conectividad + prueba de ancho de banda de Speedtest.net", "Verificação de conectividade + teste de largura de banda do Speedtest.net"),
            20: t("Resolve Target Hostname to IP Address(es)", "Résoudre le nom d'hôte cible en adresse(s) IP", "Zielhostname in IP-Adresse(n) auflösen", "Разрешить имя хоста цели в IP адрес(а)", "Risolvi hostname target in indirizzo/i IP", "将目标主机名解析为IP地址", "將目標主機名稱解析為IP位址", "Resolver el nombre de host objetivo a dirección(es) IP", "Resolver o nome de host alvo para endereço(s) IP"),
            21: t("Ping Round-trip Time and Packet Loss Statistics", "Temps aller-retour de ping et statistiques de perte de paquets", "Ping-Umlaufzeit und Paketverluststatistik", "Время кругового обхода пинга и статистика потери пакетов", "Tempo andata/ritorno ping e statistiche perdita pacchetti", "Ping往返时间和丢包率统计", "Ping往返時間和丟包率統計", "Tiempo de ida y vuelta de ping y estadísticas de pérdida de paquetes", "Tempo de ida e volta do ping e estatísticas de perda de pacotes"),
            22: t("Route Path and Per-hop Latency to Target", "Chemin de route et latence par saut vers la cible", "Routenpfad und Hop-Latenz zum Ziel", "Путь маршрута и задержка на каждом хопе до цели", "Percorso di route e latenza per hop verso target", "到目标的路由路径和每跳延迟", "到目標的路由路徑和每跳延遲", "Ruta y latencia por salto hasta el objetivo", "Caminho da rota e latência por salto até o alvo"),
            23: t("Combined Traceroute and Ping with Per-Hop Loss", "Traceroute et ping combinés avec perte par saut", "Kombinierter Traceroute und Ping mit Hop-Verlust", "Комбинированная трассировка и пинг с потерей на хопе", "Traceroute e ping combinati con perdita per hop", "组合路由追踪和ping及每跳丢包", "組合路由追蹤和ping及每跳丟包", "Traceroute y ping combinados con pérdida por salto", "Traceroute e ping combinados com perda por salto"),
            24: t("Path MTU Discovery to Target Host", "Découverte du MTU du chemin vers l'hôte cible", "Pfad-MTU-Erkennung zum Zielhost", "Обнаружение MTU пути к целевому хосту", "Scoperta MTU percorso verso host target", "到目标主机的路径MTU发现", "到目標主機的路徑MTU發現", "Descubrimiento de MTU de ruta hasta el host objetivo", "Descoberta de MTU de caminho até o host alvo"),
            25: t("IPv6 DNS (AAAA) Resolution and TCP Connectivity", "Résolution DNS IPv6 (AAAA) et connectivité TCP", "IPv6 DNS (AAAA)-Auflösung und TCP-Konnektivität", "IPv6 DNS (AAAA) разрешение и TCP соединение", "Risoluzione DNS IPv6 (AAAA) e connettività TCP", "IPv6 DNS (AAAA)解析和TCP连接", "IPv6 DNS (AAAA)解析和TCP連線", "Resolución DNS IPv6 (AAAA) y conectividad TCP", "Resolução DNS IPv6 (AAAA) e conectividade TCP"),
            26: t("Parse and Validate the Target URL Components", "Analyser et valider les composants de l'URL cible", "Komponenten der Ziel-URL analysieren und validieren", "Разбор и проверка компонентов целевого URL", "Analizza e convalida i componenti URL target", "解析和验证目标URL组件", "解析和驗證目標URL元件", "Analizar y validar los componentes de la URL objetivo", "Analisar e validar os componentes da URL alvo"),
            27: t("TCP Connectivity Check to the URL Host on Default Port", "Vérification de connectivité TCP vers l'hôte URL sur le port par défaut", "TCP-Konnektivitätsprüfung zum URL-Host auf Standardport", "Проверка TCP соединения с хостом URL на порту по умолчанию", "Controllo connettività TCP all'host URL su porta predefinita", "对URL主机的默认端口进行TCP连接检查", "對URL主機的預設埠進行TCP連線檢查", "Comprobación de conectividad TCP al host de la URL en el puerto predeterminado", "Verificação de conectividade TCP ao host da URL na porta padrão"),
            28: t("Service Banner Detection for Text-based Protocols", "Détection de bannière de service pour protocoles texte", "Service-Banner-Erkennung für textbasierte Protokolle", "Обнаружение баннера сервиса для текстовых протоколов", "Rilevamento banner servizio per protocolli testuali", "基于文本协议的服务标识检测", "基於文字協定的服務標識偵測", "Detección de banner de servicio para protocolos basados en texto", "Detecção de banner de serviço para protocolos baseados em texto"),
            29: t("HTTP Request/Response Headers and Timing", "En-têtes et chronométrage des requêtes/réponses HTTP", "HTTP-Anfrage-/Antwort-Header und Timing", "Заголовки и тайминг HTTP запросов/ответов", "Intestazioni e temporizzazione richiesta/risposta HTTP", "HTTP请求/响应头和计时", "HTTP請求/回應標頭和計時", "Encabezados y tiempos de solicitud/respuesta HTTP", "Cabeçalhos e tempos de requisição/resposta HTTP"),
            30: t("HTTP Response Headers from the Target Server", "En-têtes de réponse HTTP du serveur cible", "HTTP-Antwort-Header vom Zielserver", "HTTP заголовки ответа от целевого сервера", "Intestazioni risposta HTTP dal server target", "来自目标服务器的HTTP响应头", "來自目標伺服器的HTTP回應標頭", "Encabezados de respuesta HTTP del servidor objetivo", "Cabeçalhos de resposta HTTP do servidor alvo"),
            31: t("Security-Related HTTP Headers (HSTS, CSP, etc.)", "En-têtes HTTP de sécurité (HSTS, CSP, etc.)", "Sicherheitsrelevante HTTP-Header (HSTS, CSP, usw.)", "Заголовки безопасности HTTP (HSTS, CSP и т.д.)", "Intestazioni HTTP di sicurezza (HSTS, CSP, ecc.)", "安全相关的HTTP头（HSTS、CSP等）", "安全相關的HTTP標頭（HSTS、CSP等）", "Encabezados HTTP de seguridad (HSTS, CSP, etc.)", "Cabeçalhos HTTP de segurança (HSTS, CSP, etc.)"),
            32: t("SSL/TLS Certificate Chain and Validity Check", "Chaîne de certificats SSL/TLS et vérification de validité", "SSL/TLS-Zertifikatskette und Gültigkeitsprüfung", "Цепочка SSL/TLS сертификатов и проверка действительности", "Catena certificati SSL/TLS e controllo validità", "SSL/TLS证书链和有效性检查", "SSL/TLS憑證鏈和有效性檢查", "Cadena de certificados SSL/TLS y comprobación de validez", "Cadeia de certificados SSL/TLS e verificação de validade"),
            33: t("HTTP Redirect Chain and Final Destination", "Chaîne de redirection HTTP et destination finale", "HTTP-Weiterleitungskette und Endziel", "Цепочка HTTP редиректов и конечный пункт", "Catena reindirizzamento HTTP e destinazione finale", "HTTP重定向链和最终目的地", "HTTP重定向鏈和最終目的地", "Cadena de redirección HTTP y destino final", "Cadeia de redirecionamento HTTP e destino final"),
            34: t("Supported Compression Methods and Encoding", "Méthodes de compression et encodage prises en charge", "Unterstützte Komprimierungsmethoden und Kodierung", "Поддерживаемые методы сжатия и кодирования", "Metodi compressione e codifica supportati", "支持的压缩方法和编码", "支援的壓縮方法和編碼", "Métodos de compresión y codificación admitidos", "Métodos de compressão e codificação suportados"),
            35: t("HTTP Request Timing Breakdown (DNS, Connect, SSL, etc.)", "Décomposition du chronométrage HTTP (DNS, connexion, SSL, etc.)", "HTTP-Anfrage-Timing-Aufschlüsselung (DNS, Verbindung, SSL, usw.)", "Разбивка тайминга HTTP запроса (DNS, соединение, SSL и т.д.)", "Scomposizione temporizzazione richiesta HTTP (DNS, connessione, SSL, ecc.)", "HTTP请求时间分解（DNS、连接、SSL等）", "HTTP請求時間分解（DNS、連線、SSL等）", "Desglose de tiempos de solicitud HTTP (DNS, conexión, SSL, etc.)", "Detalhamento dos tempos de requisição HTTP (DNS, conexão, SSL, etc.)"),
            36: t("FTP Service Reachability and Banner Detection", "Accessibilité du service FTP et détection de bannière", "FTP-Diensterreichbarkeit und Banner-Erkennung", "Доступность FTP сервиса и обнаружение баннера", "Raggiungibilità servizio FTP e rilevamento banner", "FTP服务可达性和横幅检测", "FTP服務可達性和橫幅偵測", "Accesibilidad del servicio FTP y detección de banner", "Acessibilidade do serviço FTP e detecção de banner"),
            37: t("SSH Version and Key Exchange Detection", "Version SSH et détection d'échange de clés", "SSH-Version und Schlüsselaustauscherkennung", "Обнаружение версии SSH и обмена ключами", "Versione SSH e rilevamento scambio chiavi", "SSH版本和密钥交换检测", "SSH版本和金鑰交換偵測", "Detección de versión SSH e intercambio de claves", "Detecção de versão SSH e troca de chaves"),
            38: t("SMTP/IMAP/POP3 Service Detection and Banner", "Détection de service SMTP/IMAP/POP3 et bannière", "SMTP/IMAP/POP3-Diensterkennung und Banner", "Обнаружение сервиса SMTP/IMAP/POP3 и баннера", "Rilevamento servizio SMTP/IMAP/POP3 e banner", "SMTP/IMAP/POP3服务检测和横幅", "SMTP/IMAP/POP3服務偵測和橫幅", "Detección de servicio SMTP/IMAP/POP3 y banner", "Detecção de serviço SMTP/IMAP/POP3 e banner"),
            39: t("Telnet Service Reachability and Login Banner", "Accessibilité du service Telnet et bannière de connexion", "Telnet-Diensterreichbarkeit und Login-Banner", "Доступность сервиса Telnet и баннер входа", "Raggiungibilità servizio Telnet e banner login", "Telnet服务可达性和登录横幅", "Telnet服務可達性和登入橫幅", "Accesibilidad del servicio Telnet y banner de inicio de sesión", "Acessibilidade do serviço Telnet e banner de login"),
            40: t("MySQL Server Reachability and Version Detection", "Accessibilité du serveur MySQL et détection de version", "MySQL-Server-Erreichbarkeit und Versionserkennung", "Доступность сервера MySQL и обнаружение версии", "Raggiungibilità server MySQL e rilevamento versione", "MySQL服务器可达性和版本检测", "MySQL伺服器可達性和版本偵測", "Accesibilidad del servidor MySQL y detección de versión", "Acessibilidade do servidor MySQL e detecção de versão"),
            41: t("PostgreSQL Server Reachability and Version Detection", "Accessibilité du serveur PostgreSQL et détection de version", "PostgreSQL-Server-Erreichbarkeit und Versionserkennung", "Доступность сервера PostgreSQL и обнаружение версии", "Raggiungibilità server PostgreSQL e rilevamento versione", "PostgreSQL服务器可达性和版本检测", "PostgreSQL伺服器可達性和版本偵測", "Accesibilidad del servidor PostgreSQL y detección de versión", "Acessibilidade do servidor PostgreSQL e detecção de versão"),
            42: t("Redis Server Reachability and INFO Command", "Accessibilité du serveur Redis et commande INFO", "Redis-Server-Erreichbarkeit und INFO-Befehl", "Доступность сервера Redis и команда INFO", "Raggiungibilità server Redis e comando INFO", "Redis服务器可达性和INFO命令", "Redis伺服器可達性和INFO命令", "Accesibilidad del servidor Redis y comando INFO", "Acessibilidade do servidor Redis e comando INFO"),
            43: t("MongoDB Server Reachability and Build Info", "Accessibilité du serveur MongoDB et informations de build", "MongoDB-Server-Erreichbarkeit und Build-Info", "Доступность сервера MongoDB и информация о сборке", "Raggiungibilità server MongoDB e info build", "MongoDB服务器可达性和构建信息", "MongoDB伺服器可達性和構建資訊", "Accesibilidad del servidor MongoDB e información de compilación", "Acessibilidade do servidor MongoDB e informações de build"),
            44: t("LDAP Server Reachability and Root DSE", "Accessibilité du serveur LDAP et DSE racine", "LDAP-Server-Erreichbarkeit und Root-DSE", "Доступность сервера LDAP и корневой DSE", "Raggiungibilità server LDAP e DSE root", "LDAP服务器可达性和根DSE", "LDAP伺服器可達性和根DSE", "Accesibilidad del servidor LDAP y DSE raíz", "Acessibilidade do servidor LDAP e DSE raiz"),
            45: t("MQTT Broker Reachability and CONNECT Response", "Accessibilité du broker MQTT et réponse CONNECT", "MQTT-Broker-Erreichbarkeit und CONNECT-Antwort", "Доступность брокера MQTT и ответ CONNECT", "Raggiungibilità broker MQTT e risposta CONNECT", "MQTT代理可达性和CONNECT响应", "MQTT代理可達性和CONNECT回應", "Accesibilidad del broker MQTT y respuesta CONNECT", "Acessibilidade do broker MQTT e resposta CONNECT"),
        }
        return typeof descs[id] === 'string' ? descs[id] : ""
    }


    // ── VPN/IP Geolocation result strings ──
    readonly property string vpnNo: t("No VPN", "Aucun VPN", "Kein VPN", "Нет VPN", "Nessun VPN", "无VPN", "無VPN", "Sin VPN", "Sem VPN")
    readonly property string vpnDetected: t("VPN Detected", "VPN détecté", "VPN erkannt", "VPN обнаружен", "VPN rilevata", "检测到VPN", "檢測到VPN", "VPN detectada", "VPN detectada")
    readonly property string vpnPossible: t("VPN Possible", "VPN possible", "VPN möglich", "VPN возможен", "VPN possibile", "可能VPN", "可能VPN", "VPN posible", "VPN possível")
    readonly property string vpnLocationEst: t("Location Est.", "Position est.", "Standort gesch.", "Место опред.", "Posizione stim.", "估计位置", "估計位置", "Ubicación est.", "Localização est.")
    readonly property string vpnNoData: t("No Data", "Aucune donnée", "Keine Daten", "Нет данных", "Nessun dato", "无数据", "無數據", "Sin datos", "Sem dados")
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

    // ── TargetInput / placeholder texts ──
    readonly property string placeholderHost: t("example.com/path", "exemple.com/chemin", "beispiel.de/pfad", "пример.рф/путь", "esempio.com/percorso", "example.com/path", "example.com/path", "ejemplo.com/ruta", "exemplo.com/caminho")
    readonly property string placeholderPort: t("Port", "Port", "Port", "Порт", "Porta", "端口", "連接埠", "Puerto", "Porta")

    // ── Detail overlay labels ──
    readonly property string detailStatusLabel: t("Status: ", "Statut : ", "Status: ", "Статус: ", "Stato: ", "状态: ", "狀態: ", "Estado: ", "Status: ")
    readonly property string detailDurationLabel: t("Duration: ", "Durée : ", "Dauer: ", "Длительность: ", "Durata: ", "持续时间: ", "持續時間: ", "Duración: ", "Duração: ")
    readonly property string detailUnknownStatus: t("Unknown", "Inconnu", "Unbekannt", "Неизвестно", "Sconosciuto", "未知", "未知", "Desconocido", "Desconhecido")

    // ── Running prefix (DiagGroupPanel live status) ──
    readonly property string runningPrefix: t("Running: ", "En cours : ", "Läuft: ", "Выполняется: ", "In corso: ", "运行中: ", "執行中: ", "Ejecutando: ", "Executando: ")

    // ── Accessibility labels ──
    readonly property string accCloseWindow: t("Close Window", "Fermer la fenêtre", "Fenster schließen", "Закрыть окно", "Chiudi finestra", "关闭窗口", "關閉視窗", "Cerrar ventana", "Fechar janela")
    readonly property string accCloseDetails: t("Close Details", "Fermer les détails", "Details schließen", "Закрыть детали", "Chiudi dettagli", "关闭详情", "關閉詳情", "Cerrar detalles", "Fechar detalhes")
    readonly property string accHidePassword: t("Hide Password", "Masquer le mot de passe", "Passwort verbergen", "Скрыть пароль", "Nascondi password", "隐藏密码", "隱藏密碼", "Ocultar contraseña", "Ocultar senha")
    readonly property string accShowPassword: t("Show Password", "Afficher le mot de passe", "Passwort anzeigen", "Показать пароль", "Mostra password", "显示密码", "顯示密碼", "Mostrar contraseña", "Mostrar senha")
    readonly property string accExpanded: t(" — Expanded", " — déroulé", " — ausgeklappt", " — развёрнуто", " — espanso", " — 已展开", " — 已展開", " — expandido", " — expandido")
    readonly property string accCollapsed: t(" — Collapsed", " — replié", " — eingeklappt", " — свёрнуто", " — compresso", " — 已折叠", " — 已折疊", " — colapsado", " — recolhido")
    readonly property string accActiveTheme: t("Active Theme", "Thème actif", "Aktives Design", "Активная тема", "Tema attivo", "当前主题", "目前主題", "Tema activo", "Tema ativo")
    readonly property string accSwitchTheme: t("Switch to This Theme", "Basculer vers ce thème", "Zu diesem Design wechseln", "Переключиться на эту тему", "Passa a questo tema", "切换到此主题", "切換到此主題", "Cambiar a este tema", "Mudar para este tema")
    readonly property string accDiagnosticSuffix: t(" diagnostic", " diagnostic", " Diagnose", " диагностика", " diagnostica", " 诊断", " 診斷", " diagnóstico", " diagnóstico")

    // ── Purchase / progress indicators ──
    readonly property string purchaseInProgress: t("Processing...", "Traitement...", "Verarbeitung...", "Обработка...", "Elaborazione...", "处理中...", "處理中...", "Procesando...", "Processando...")

    // ── Common fallback / technical labels ──
    readonly property string testIdPrefix: t("Test #", "Test n°", "Test Nr.", "Тест №", "Test n.", "测试 #", "測試 #", "Prueba n.°", "Teste n.º")
    readonly property string versionLabel: t("Version", "Version", "Version", "Версия", "Versione", "版本", "版本", "Versión", "Versão")
    readonly property string buildLabel: t("Build", "Build", "Build", "Сборка", "Build", "构建", "構建", "Build", "Build")

    // ── Host label (TargetAnalysisPanel) ──
    readonly property string hostLabel: t("Host    :", "Hôte    :", "Host    :", "Хост    :", "Host    :", "主机    :", "主機    :", "Host    :", "Host    :")

    // ── IP classification labels (TargetAnalysisPanel) ──
    readonly property string ipClassPrivate: t("Private (RFC 1918)", "Privée (RFC 1918)", "Privat (RFC 1918)", "Частная (RFC 1918)", "Privata (RFC 1918)", "私有 (RFC 1918)", "私人 (RFC 1918)", "Privada (RFC 1918)", "Privada (RFC 1918)")
    readonly property string ipClassCgnat: t("CGNAT (RFC 6598)", "CGNAT (RFC 6598)", "CGNAT (RFC 6598)", "CGNAT (RFC 6598)", "CGNAT (RFC 6598)", "运营商NAT (RFC 6598)", "電信商NAT (RFC 6598)", "CGNAT (RFC 6598)", "CGNAT (RFC 6598)")
    readonly property string ipClassApipa: t("Link-Local (APIPA)", "Lien local (APIPA)", "Link-Lokal (APIPA)", "Локальная (APIPA)", "Collegamento locale (APIPA)", "链路本地 (APIPA)", "連結本地 (APIPA)", "Enlace local (APIPA)", "Link local (APIPA)")
    readonly property string ipClassLoopback: t("Loopback", "Boucle locale", "Lokale Schleife", "Петля", "Loopback", "环回", "環回", "Bucle local", "Loopback")
    readonly property string ipClassPublic: t("Public Routable", "Publique routable", "Öffentlich routbar", "Публичная маршрутизируемая", "Pubblica routabile", "公网可路由", "公網可路由", "Pública enrutable", "Pública roteável")

    // ── URL error labels ──
    readonly property string malformedUrlLabel: t("Malformed URL", "URL mal formée", "Fehlerhafte URL", "Некорректный URL", "URL non valido", "URL格式错误", "URL格式錯誤", "URL mal formada", "URL malformada")

    // ── Accessibility: PDF navigation ──
    readonly property string accPrevPage: t("Previous Page", "Page précédente", "Vorherige Seite", "Предыдущая страница", "Pagina precedente", "上一页", "上一頁", "Página anterior", "Página anterior")
    readonly property string accNextPage: t("Next Page", "Page suivante", "Nächste Seite", "Следующая страница", "Pagina successiva", "下一页", "下一頁", "Página siguiente", "Página seguinte")

    // ── Accessibility: DiagnosticToolbar ──
    readonly property string accHideAdvanced: t("Hide Advanced Options", "Masquer les options avancées", "Erweiterte Optionen ausblenden", "Скрыть дополнительные опции", "Nascondi opzioni avanzate", "隐藏高级选项", "隱藏進階選項", "Ocultar opciones avanzadas", "Ocultar opções avançadas")
    readonly property string accShowAdvanced: t("Show Advanced Options", "Afficher les options avancées", "Erweiterte Optionen anzeigen", "Показать дополнительные опции", "Mostra opzioni avanzate", "显示高级选项", "顯示進階選項", "Mostrar opciones avanzadas", "Mostrar opções avançadas")
    readonly property string accStopDiag: t("Stop Diagnostics", "Arrêter le diagnostic", "Diagnose stoppen", "Остановить диагностику", "Ferma diagnostica", "停止诊断", "停止診斷", "Detener diagnóstico", "Parar diagnóstico")
    readonly property string accRunDiag: t("Run Diagnostics", "Lancer le diagnostic", "Diagnose starten", "Запустить диагностику", "Avvia diagnostica", "运行诊断", "運行診斷", "Ejecutar diagnóstico", "Executar diagnóstico")
    readonly property string accClearTarget: t("Clear Target Input", "Effacer la cible", "Zieleingabe löschen", "Очистить ввод цели", "Cancella input target", "清除目标输入", "清除目標輸入", "Borrar entrada de objetivo", "Limpar entrada de alvo")

    // ── App name (Settings/About screen) ──
    readonly property string appName: t("NetDiagnostics", "NetDiagnostics", "NetDiagnostics", "NetDiagnostics", "NetDiagnostics", "NetDiagnostics", "NetDiagnostics", "NetDiagnostics", "NetDiagnostics")

    // ── Group prefix (G1, G2, ...) ──
    function groupPrefix(idx) {
        var d = lang
        var labels = [
            t("G", "G", "G", "G", "G", "G", "G", "G", "G")
        ]
        return labels[0] + (idx + 1)
    }
}
