// SPDX-License-Identifier: AGPL-3.0-or-later
#include "releases.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <stdexcept>
namespace t64 {
std::optional<Version> Version::parse(QString value) {
    if (value.startsWith('v'))
        value.remove(0, 1);
    static const QRegularExpression re("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z.-"
                                       "]+))?(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?$");
    auto m = re.match(value);
    if (!m.hasMatch())
        return {};
    Version v;
    bool a, b, c;
    v.major = m.captured(1).toULongLong(&a);
    v.minor = m.captured(2).toULongLong(&b);
    v.patch = m.captured(3).toULongLong(&c);
    if (!a || !b || !c)
        return {};
    if (!m.captured(4).isEmpty())
        v.pre = m.captured(4).split('.');
    for (const auto &p : v.pre) {
        if (p.isEmpty())
            return {};
        if (QRegularExpression("^[0-9]+$").match(p).hasMatch() && p.size() > 1 && p.startsWith('0'))
            return {};
    }
    return v;
}
int Version::compare(const Version &o) const {
    for (auto pair : {std::pair{major, o.major}, std::pair{minor, o.minor}, std::pair{patch, o.patch}})
        if (pair.first != pair.second)
            return pair.first > pair.second ? 1 : -1;
    if (pre.isEmpty() != o.pre.isEmpty())
        return pre.isEmpty() ? 1 : -1;
    for (int i = 0; i < std::min(pre.size(), o.pre.size()); ++i) {
        const auto &a = pre[i], &b = o.pre[i];
        if (a == b)
            continue;
        const bool an = QRegularExpression("^[0-9]+$").match(a).hasMatch(),
                   bn = QRegularExpression("^[0-9]+$").match(b).hasMatch();
        if (an != bn)
            return an ? -1 : 1;
        if (an && a.size() != b.size())
            return a.size() > b.size() ? 1 : -1;
        return a > b ? 1 : -1;
    }
    return pre.size() == o.pre.size() ? 0 : (pre.size() > o.pre.size() ? 1 : -1);
}
Packaging packagingFor(const QString &os, const QString &arch, bool appImage, bool systemPath,
                       const QString &debianRelease) {
    // Windows et macOS reçoivent bien un paquet : Trait64 ne sait pas l'installer, mais
    // il peut dire qu'une version est parue et la poser sur le disque. Les en écarter
    // laissait croire qu'aucune mise à jour n'existait pour eux.
    if (os == "windows")
        return {"manual", arch == "x86_64" ? "-windows-x86_64.zip" : QString()};
    if (os == "macos")
        return {"manual", arch == "arm64" ? "-macos-arm64.dmg" : QString()};
    if (arch != "x86_64")
        return {appImage || !systemPath ? "AppImage" : "deb", {}};
    if (appImage || !systemPath)
        return {"AppImage", "-linux-x86_64.AppImage"};
    // Les .deb publiés visent Debian 12 et 13 nommément : ailleurs, mieux vaut ne rien
    // proposer que d'installer un paquet bâti pour une autre bibliothèque C.
    const auto major = debianRelease.trimmed().section('.', 0, 0);
    return {"deb", major == "12" || major == "13" ? "_debian" + major + "_amd64.deb" : QString()};
}
Release selectRelease(const QJsonArray &rows, const QString &current, const QString &channel,
                      const QString &suffix, const QStringList &ignored) {
    auto best = Version::parse(current);
    if (!best || suffix.isEmpty() || !QStringList{"stable", "beta", "development"}.contains(channel))
        return {};
    Release chosen;
    for (const auto &row : rows) {
        auto r = row.toObject();
        QString tag = r["tag_name"].toString();
        auto v = Version::parse(tag);
        if (!v || r["draft"].toBool() || r["published_at"].toString().isEmpty() || ignored.contains(tag) ||
            v->compare(*best) <= 0)
            continue;
        const bool pre = !v->pre.isEmpty();
        if (pre != r["prerelease"].toBool())
            continue;
        if (pre && channel == "stable")
            continue;
        if (pre && channel == "beta" && !QStringList{"beta", "rc"}.contains(v->pre[0]))
            continue;
        if (pre && channel == "development" && !QStringList{"beta", "rc", "dev"}.contains(v->pre[0]))
            continue;
        for (auto a : r["assets"].toArray()) {
            auto asset = a.toObject();
            const QString name = asset["name"].toString(), hash = asset["digest"].toString();
            const qint64 size = asset["size"].toInteger(), id = asset["id"].toInteger();
            if (!name.endsWith(suffix) || name.contains('/') || name.contains('\\') ||
                (!name.startsWith("Trait64-") && !name.startsWith("trait64_")) ||
                asset["state"] != "uploaded" ||
                !QRegularExpression("^sha256:[0-9a-f]{64}$").match(hash).hasMatch() || size < 8 ||
                size > 1024LL * 1024 * 1024 || id <= 0)
                continue;
            chosen = {tag, r["body"].toString().left(100000), name, hash.mid(7), id, size};
            best = v;
            break;
        }
    }
    return chosen;
}
bool trustedUpdateUrl(const QUrl &u) {
    if (u.scheme() != "https" || !u.userInfo().isEmpty() || u.port(443) != 443)
        return false;
    auto h = u.host().toLower();
    return h == "api.github.com" || h == "github.com" || h.endsWith(".githubusercontent.com") ||
           h.endsWith(".githubassets.com");
}
bool verifyFile(const QString &path, const QString &hash, qint64 size) {
    QFile file(path);
    if (file.size() != size || !file.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash digest(QCryptographicHash::Sha256);
    if (!digest.addData(&file))
        return false;
    return digest.result().toHex() == hash.toLatin1();
}
static void copyAtomic(const QString &source, const QString &target) {
    QFile in(source);
    QSaveFile out(target);
    out.setDirectWriteFallback(false);
    if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly))
        throw std::runtime_error("Impossible d’écrire le nouvel exécutable.");
    while (!in.atEnd()) {
        auto bytes = in.read(1024 * 1024);
        if ((bytes.isEmpty() && in.error() != QFileDevice::NoError) || out.write(bytes) != bytes.size())
            throw std::runtime_error("Copie interrompue.");
    }
    if (!out.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup |
                            QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther) ||
        !out.commit())
        throw std::runtime_error("Remplacement atomique impossible.");
}
void replaceAppImage(const QString &source, const QString &target, const QString &hash, qint64 size) {
    if (!verifyFile(source, hash, size))
        throw std::runtime_error("Empreinte de mise à jour incorrecte.");
    QFile in(source);
    in.open(QIODevice::ReadOnly);
    const auto magic = in.read(11);
    if (magic.left(4) != QByteArray("\x7f"
                                    "ELF",
                                    4) ||
        magic.mid(8, 3) != QByteArray("AI\x02", 3))
        throw std::runtime_error("Ce fichier n’est pas une AppImage de type 2.");
    if (QFileInfo(target).isSymLink())
        throw std::runtime_error("Le chemin d’installation ne doit pas être un lien symbolique.");
    if (QFileInfo::exists(target))
        copyAtomic(target, target + ".previous");
    copyAtomic(source, target);
}
QString debianVersion(QString value) {
    if (value.startsWith('v'))
        value.remove(0, 1);
    return value.replace('-', '~');
}
} // namespace t64
