// SPDX-License-Identifier: AGPL-3.0-or-later
#include "content.h"
#include "game.h"
#include "uci.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <archive.h>
#include <archive_entry.h>
#include <iostream>

void check(bool ok, const char *text) {
    if (!ok)
        throw std::runtime_error(text);
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Unsafe or invalid package accepted");
}
void zip(const QString &path, const QString &name, const QByteArray &bytes, bool symlink = false) {
    auto *writer = archive_write_new();
    archive_write_set_format_zip(writer);
    check(archive_write_open_filename(writer, QFile::encodeName(path).constData()) == ARCHIVE_OK,
          "Create zip fixture");
    auto *entry = archive_entry_new();
    archive_entry_set_pathname(entry, name.toUtf8().constData());
    archive_entry_set_filetype(entry, symlink ? AE_IFLNK : AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, symlink ? 0 : bytes.size());
    if (symlink)
        archive_entry_set_symlink(entry, "../../outside");
    check(archive_write_header(writer, entry) == ARCHIVE_OK, "Zip entry");
    if (!symlink)
        check(archive_write_data(writer, bytes.constData(), bytes.size()) == bytes.size(), "Zip content");
    archive_entry_free(entry);
    archive_write_close(writer);
    archive_write_free(writer);
}
bool waitFor(const std::function<bool()> &condition, int timeout = 15000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout)
        QTest::qWait(20);
    return condition();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    try {
        check(t64::Content::allowedUrl(QUrl("https://github.com/a/b/releases/download/v/file"), true),
              "Official engine host");
        check(!t64::Content::allowedUrl(QUrl("http://github.com/file"), true), "Reject HTTP");
        check(!t64::Content::allowedUrl(QUrl("https://github.com.evil.invalid/file"), true),
              "Reject host suffix");
        check(!t64::Content::allowedUrl(QUrl("https://github.com:8443/file"), true), "Reject alternate port");
        check(!t64::Content::allowedUrl(QUrl("https://user@github.com/file"), true),
              "Reject embedded credentials");
        check(!t64::Content::allowedUrl(QUrl("https://www.pgnmentor.com/engine"), true),
              "Data source cannot supply executable");
        check(!t64::Content::compatible({{"kind", "engine"}, {"minGlibc", "999.0"}}),
              "Reject unavailable runtime");
        const auto canceled = std::make_shared<std::atomic_bool>(false);
        QVariantMap entry{{"id", "fixture"},
                          {"kind", "database"},
                          {"version", "1"},
                          {"format", "zip"},
                          {"title", "Fixture"}};
        const auto archive = dir.path() + "/fixture.zip";
        const QByteArray pgn("[Event \"Test\"]\n1.e4 e5 *\n");
        zip(archive, "Games.pgn", pgn);
        auto installed = t64::Content::installPackage(entry, archive, dir.path() + "/data", canceled);
        QFile file(installed["path"].toString());
        check(file.open(QIODevice::ReadOnly), "Installed data accessible");
        check(file.readAll() == pgn && !QFileInfo(file).isExecutable(),
              "PGN stays data, no executable permission");
        file.close();
        entry["sha256"] = QString(64, '0');
        rejects([&] { t64::Content::installPackage(entry, archive, dir.path() + "/bad", canceled); });
        check(!QFileInfo::exists(dir.path() + "/bad/fixture.json"), "Failed digest leaves no receipt");
        entry.remove("sha256");
        zip(archive, "../../escape.pgn", pgn);
        rejects([&] { t64::Content::installPackage(entry, archive, dir.path() + "/bad", canceled); });
        check(!QFileInfo::exists(dir.path() + "/escape.pgn"), "Traversal never writes outside staging");
        zip(archive, "Games.pgn", {}, true);
        rejects([&] { t64::Content::installPackage(entry, archive, dir.path() + "/bad", canceled); });
        zip(archive, "Games.pgn", "<html>Service unavailable</html>");
        rejects([&] { t64::Content::installPackage(entry, archive, dir.path() + "/bad", canceled); });
        zip(archive, "Games.pgn", pgn);
        canceled->store(true);
        rejects([&] { t64::Content::installPackage(entry, archive, dir.path() + "/cancel", canceled); });
        canceled->store(false);
        t64::Content manager(dir.path() + "/catalog");
        auto catalog = manager.catalog();
        check(catalog.size() >= 30, "Compiled catalogue available offline");
        QSet<QString> ids;
        int engines = 0, champions = 0;
        for (const auto &value : catalog) {
            const auto row = value.toMap();
            check(!ids.contains(row["id"].toString()), "Unique catalogue identifiers");
            ids.insert(row["id"].toString());
            check(t64::Content::allowedUrl(QUrl(row["url"].toString()), row["kind"] == "engine"),
                  "Catalogue URL policy");
            if (row["kind"] == "engine") {
                ++engines;
                check(row["sha256"].toString().size() == 64 && row["bytes"].toLongLong() > 0,
                      "Pinned executable digest and size");
            }
            if (row["category"] == "Champions du monde")
                ++champions;
        }
        check(engines == 3 && champions == 22, "Three engines, 18 classical and four other FIDE champions");
        manager.install("../unknown");
        check(!manager.busy(), "Unknown content never starts network");
        // Explicit opt-in: verify official packages already downloaded into the supplied directory.
        const auto real = qEnvironmentVariable("T64_CONTENT_FIXTURES");
        if (!real.isEmpty())
            for (const auto &value : catalog) {
                const auto row = value.toMap();
                const auto source = real + '/' + QUrl(row["url"].toString()).fileName();
                if (!QFileInfo::exists(source) || !row["compatible"].toBool())
                    continue;
                auto installed = t64::Content::installPackage(row, source, dir.path() + "/real", canceled);
                if (row["kind"] == "engine") {
                    t64::Uci engine;
                    QSignalSpy failed(&engine, &t64::Uci::failed), best(&engine, &t64::Uci::bestMove);
                    engine.start(installed["path"].toString(), 1, 16, 1, 20);
                    check(waitFor([&] { return engine.ready() || !failed.empty(); }) && engine.ready(),
                          "Official engine handshake");
                    const auto fen = QString::fromStdString(chess::constants::STARTPOS);
                    engine.analyse(fen, "position startpos", true, 200);
                    check(waitFor([&] { return !best.empty() || !failed.empty(); }) && !best.empty(),
                          "Official engine returns move");
                    t64::Game game;
                    game.play(0, best[0][0].toString().toStdString());
                    engine.shutdown();
                } else {
                    QFile pgnFile(installed["path"].toString());
                    check(pgnFile.open(QIODevice::ReadOnly), "Real PGN");
                    check(!t64::parsePgn(pgnFile.readAll().toStdString()).empty(),
                          "Real source games parse legally");
                }
                std::cout << "Verified official fixture: " << row["title"].toString().toStdString() << '\n';
            }
        if (qEnvironmentVariableIsSet("T64_CONTENT_NETWORK")) {
            QSignalSpy ready(&manager, &t64::Content::ready);
            manager.install("pgnmentor-steinitz");
            check(waitFor([&] { return !manager.busy(); }, 120000), "Real network download timeout");
            if (ready.empty())
                std::cerr << manager.status().toStdString() << '\n';
            check(ready.size() == 1, "Real HTTPS download completed");
            auto receipt = ready[0][1].toMap();
            check(receipt["sha256"].toString().size() == 64 && QFileInfo::exists(ready[0][0].toString()),
                  "Network receipt and extracted PGN");
            manager.remove("pgnmentor-steinitz");
            check(!QFileInfo::exists(ready[0][0].toString()), "Remove managed download");
            std::cout << "Verified real Qt HTTPS download and removal\n";
        }
        std::cout << "Content integrity, archive security and catalogue checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
