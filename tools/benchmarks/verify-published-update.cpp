// SPDX-License-Identifier: AGPL-3.0-or-later
#include "releases.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        if (argc != 3) throw std::runtime_error("metadata.json and expected tag required");
        QFile input(QString::fromUtf8(argv[1]));
        if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read metadata");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(input.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            throw std::runtime_error("Invalid release metadata");
        const auto release = document.object();
        const auto tag = QString::fromUtf8(argv[2]);
        if (release["tag_name"] != tag || release["draft"].toBool() ||
            release["published_at"].toString().isEmpty())
            throw std::runtime_error("Release has not been published as expected");
        const QJsonArray releases{release};
        QJsonArray selected;
        for (const auto &suffix : QStringList{"-linux-x86_64.AppImage", "_debian12_amd64.deb", "_debian13_amd64.deb"}) {
            const auto candidate = t64::selectRelease(releases, "0.3.0-dev.57", "development", suffix);
            if (!candidate.valid() || candidate.version != tag || !candidate.name.contains(tag.mid(1)))
                throw std::runtime_error("Expected update was not selected");
            for (const auto &channel : QStringList{"stable", "beta"})
                if (t64::selectRelease(releases, "0.3.0-dev.57", channel, suffix).valid())
                    throw std::runtime_error("Development release leaked into another channel");
            if (t64::selectRelease(releases, tag, "development", suffix).valid() ||
                t64::selectRelease(releases, "0.3.0-dev.57", "development", suffix, {tag}).valid() ||
                t64::selectRelease(releases, "0.3.0-dev.999", "development", suffix).valid())
                throw std::runtime_error("Current, ignored or older release selected");
            selected.append(QJsonObject{{"name", candidate.name}, {"sha256", candidate.sha256},
                                       {"asset_id", candidate.assetId}, {"bytes", candidate.size}});
        }
        std::cout << QJsonDocument(QJsonObject{{"tag", tag}, {"selected", selected},
            {"development_channel", true}, {"stable_and_beta_exclusion", true},
            {"same_version_and_downgrade_rejection", true}, {"ignored_version_rejection", true}})
            .toJson().constData();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
