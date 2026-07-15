// =============================================================================
// ProtocolRegistry.h — V2: Hardcoded protocol & test case registry.
//
// Single source of truth for all protocol groups and their test cases.
// UI auto-generates protocol group buttons from this registry.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

struct TestCase {
    QString caseId;
    QString caseName;
    QString host = "localhost";
    int     port = 0;
    QString username;
    QString password;
    QString url;
    QString expectedResult;
};

struct ProtocolGroup {
    QString groupId;
    QString groupName;
    QString icon;
    QStringList protocols;
    QList<TestCase> testCases;
};

class ProtocolRegistry : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList groups READ groups CONSTANT)
    Q_PROPERTY(int groupCount READ groupCount CONSTANT)

public:
    static ProtocolRegistry& instance() {
        static ProtocolRegistry reg;
        return reg;
    }

    QVariantList groups() const { return m_groupList; }
    int groupCount() const { return m_groups.size(); }

    Q_INVOKABLE QVariantList testCasesForGroup(int idx) const {
        if (idx < 0 || idx >= m_groups.size()) return {};
        QVariantList result;
        for (const auto& tc : m_groups[idx].testCases) {
            QVariantMap m;
            m["caseId"]   = tc.caseId;
            m["caseName"] = tc.caseName;
            m["host"]     = tc.host;
            m["port"]     = tc.port;
            m["url"]      = tc.url;
            m["expected"] = tc.expectedResult;
            result.append(m);
        }
        return result;
    }

    Q_INVOKABLE QVariantMap groupAt(int idx) const {
        if (idx < 0 || idx >= m_groups.size()) return {};
        const auto& g = m_groups[idx];
        QVariantMap m;
        m["groupId"]   = g.groupId;
        m["groupName"] = g.groupName;
        m["icon"]      = g.icon;
        m["count"]     = g.testCases.size();
        return m;
    }

    Q_INVOKABLE QString buildTargetForCase(int groupIdx, int caseIdx) const {
        if (groupIdx < 0 || groupIdx >= m_groups.size()) return {};
        const auto& g = m_groups[groupIdx];
        if (caseIdx < 0 || caseIdx >= g.testCases.size()) return {};
        const auto& tc = g.testCases[caseIdx];
        QString t = tc.url.isEmpty() ? tc.host : tc.url;
        if (tc.port > 0 && !t.contains(':'))
            t += QStringLiteral(":%1").arg(tc.port);
        if (!tc.username.isEmpty())
            t = tc.username + QStringLiteral("@") + t;
        return t;
    }

private:
    ProtocolRegistry() { initDefaults(); }

    void initDefaults() {
        // ── Web (HTTP, HTTPS) ──────────────────────────────────────────
        {
            ProtocolGroup g;
            g.groupId = "web"; g.groupName = "Web"; g.icon = "globe";
            g.protocols = {"HTTP","HTTPS"};
            g.testCases = {
                {"web-http-get",   "HTTP GET",        "httpbin.org", 80,  "", "", "https://httpbin.org/get", "200 OK"},
                {"web-http-hdr",   "HTTP Headers",    "httpbin.org", 80,  "", "", "https://httpbin.org/headers", "200 OK"},
                {"web-https-cert", "HTTPS Certificate","microsoft.com", 443, "", "", "https://microsoft.com", "Valid Cert"},
                {"web-https-tls",  "HTTPS TLS Check",  "github.com", 443, "", "", "https://github.com", "TLS 1.3"},
            };
            m_groups.append(g);
        }
        // ── Secure (SSH, Email) ────────────────────────────────────────
        {
            ProtocolGroup g;
            g.groupId = "secure"; g.groupName = "Secure"; g.icon = "mail";
            g.protocols = {"SSH","Email"};
            g.testCases = {
                {"sec-ssh-port",   "SSH Port Check",   "github.com",  22,  "", "", "ssh://github.com:22", "Port Open"},
                {"sec-smtp-port",  "SMTP Port Check",  "smtp-mail.outlook.com", 587, "", "", "smtp://smtp-mail.outlook.com:587", "Port Open"},
            };
            m_groups.append(g);
        }
        // ── File Transfer (FTP) ────────────────────────────────────────
        {
            ProtocolGroup g;
            g.groupId = "file"; g.groupName = "File Transfer"; g.icon = "portscan";
            g.protocols = {"FTP","SFTP"};
            g.testCases = {
                {"file-ftp-port",  "FTP Port Check",   "ftp.gnu.org",  21,  "", "", "ftp://ftp.gnu.org:21", "Port Open"},
            };
            m_groups.append(g);
        }
        // ── Remote Access (Telnet) ─────────────────────────────────────
        {
            ProtocolGroup g;
            g.groupId = "remote"; g.groupName = "Remote Access"; g.icon = "target";
            g.protocols = {"Telnet"};
            g.testCases = {
                {"rem-telnet-port","Telnet Port Check", "towel.blinkenlights.nl", 23, "", "", "telnet://towel.blinkenlights.nl:23", "Port Open"},
            };
            m_groups.append(g);
        }
        // ── Database (MySQL, PostgreSQL, Redis, MongoDB) ───────────────
        {
            ProtocolGroup g;
            g.groupId = "db"; g.groupName = "Database"; g.icon = "config";
            g.protocols = {"MySQL","PostgreSQL","Redis","MongoDB"};
            g.testCases = {
                {"db-mysql-port",  "MySQL Port Check",  "localhost",  3306, "", "", "mysql://localhost:3306", "Port Check"},
                {"db-pg-port",     "PostgreSQL Port",   "localhost",  5432, "", "", "postgresql://localhost:5432", "Port Check"},
                {"db-redis-port",  "Redis Port Check",  "localhost",  6379, "", "", "redis://localhost:6379", "Port Check"},
                {"db-mongo-port",  "MongoDB Port Check","localhost", 27017, "", "", "mongodb://localhost:27017", "Port Check"},
            };
            m_groups.append(g);
        }
        // ── Custom (LDAP, MQTT) ────────────────────────────────────────
        {
            ProtocolGroup g;
            g.groupId = "custom"; g.groupName = "Custom"; g.icon = "tune";
            g.protocols = {"LDAP","MQTT"};
            g.testCases = {
                {"cust-ldap-port", "LDAP Port Check",   "ldap.forumsys.com", 389, "", "", "ldap://ldap.forumsys.com:389", "Port Open"},
                {"cust-mqtt-port", "MQTT Port Check",   "test.mosquitto.org",1883, "", "", "mqtt://test.mosquitto.org:1883", "Port Open"},
            };
            m_groups.append(g);
        }

        // Build QML-friendly variant list
        for (const auto& g : m_groups) {
            QVariantMap m;
            m["groupId"]   = g.groupId;
            m["groupName"] = g.groupName;
            m["icon"]      = g.icon;
            m["count"]     = g.testCases.size();
            m_groupList.append(m);
        }
    }

    QList<ProtocolGroup> m_groups;
    QVariantList m_groupList;
};
