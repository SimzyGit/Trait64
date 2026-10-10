// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>
namespace t64 {
struct Version {
    quint64 major = 0, minor = 0, patch = 0;
    QStringList pre;
    static std::optional<Version> parse(QString value);
    int compare(const Version &other) const;
};
struct Release {
    QString version, notes, name, sha256;
    qint64 assetId = 0, size = 0;
    bool valid() const { return assetId > 0; }
};
Release selectRelease(const QJsonArray &releases, const QString &current, const QString &channel,
                      const QString &suffix, const QStringList &ignored = {});
bool trustedUpdateUrl(const QUrl &url);
bool verifyFile(const QString &path, const QString &sha256, qint64 size);
// Same-directory atomic replacement, preserving the previous executable for rollback.
void replaceAppImage(const QString &source, const QString &target, const QString &sha256, qint64 size);
QString debianVersion(QString semver);
} // namespace t64
