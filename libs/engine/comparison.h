// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "uci.h"
#include <memory>
#include <vector>
namespace t64 {
class Comparison : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
  public:
    explicit Comparison(QObject *parent = nullptr);
    ~Comparison() override;
    QVariantList rows() const;
    // Ce que disent les moteurs ensemble : le coup sur lequel le plus s'accordent, combien
    // le choisissent, et l'écart de leurs évaluations (en pions, cp seulement).
    static QVariantMap synthesis(const QVariantList &rows);
    QVariantMap summary() const { return synthesis(rows()); }
    bool running() const { return running_; }
    void start(const QVariantList &profiles, const QString &fen, const QString &position, bool chess960);
    void setPosition(const QString &fen, const QString &position, bool chess960);
    Q_INVOKABLE void stop();
  signals:
    void changed();

  private:
    struct Slot {
        std::unique_ptr<Uci> engine;
        QVariantMap data;
        int depth = 24;
    };
    std::vector<std::unique_ptr<Slot>> slots_;
    QTimer debounce_, notify_;
    QString fen_, position_;
    bool running_ = false, chess960_ = false;
    void scheduleChanged();
};
} // namespace t64
