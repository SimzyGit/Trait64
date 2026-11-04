// SPDX-License-Identifier: AGPL-3.0-or-later
#include "game.h"
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

namespace t64 {
void readPgnParallel(std::istream &input, int workers, const std::function<void(Game &&, uint64_t)> &consume,
                     const std::function<void(uint64_t)> &progress) {
    if (workers < 1 || workers > 8)
        throw std::runtime_error("Nombre de lecteurs PGN invalide (1 à 8).");
    if (workers == 1) {
        readPgnRecords(input, consume, progress);
        return;
    }
    using Parsed = std::optional<Game>;
    struct Pending {
        std::future<Parsed> game;
        uint64_t offset;
        size_t bytes;
    };
    // L'ordre de ces deux déclarations porte une règle, et non un goût : les objets
    // locaux se détruisent à l'envers de leur déclaration, donc le pool — qui joint ses
    // fils — s'efface avant les futures qu'il alimente. L'inverse laissait mourir une
    // future pendant que son producteur écrivait encore dedans, ce qui arrivait à chaque
    // interruption, puisque l'arrêt remonte par une exception à travers toute la pile.
    std::deque<Pending> pending;
    size_t buffered = 0;
    struct Pool {
        std::mutex mutex;
        std::condition_variable ready;
        std::deque<std::packaged_task<Parsed()>> tasks;
        bool closed = false;
        std::vector<std::thread> threads;
        ~Pool() {
            {
                std::lock_guard lock(mutex);
                closed = true;
                // Les tâches encore en file sont abandonnées : leurs futures ne seront
                // plus attendues, puisque l'appelant s'en va.
                tasks.clear();
            }
            ready.notify_all();
            // Aucun fil ne survit à cette ligne, donc plus personne n'écrit dans une
            // future après elle.
            for (auto &thread : threads)
                thread.join();
        }
    } pool;
    for (int i = 0; i < workers; ++i)
        pool.threads.emplace_back([&] {
            for (;;) {
                std::packaged_task<Parsed()> task;
                {
                    std::unique_lock lock(pool.mutex);
                    pool.ready.wait(lock, [&] { return pool.closed || !pool.tasks.empty(); });
                    if (pool.closed)
                        return;
                    task = std::move(pool.tasks.front());
                    pool.tasks.pop_front();
                }
                task(); // packaged_task transports parser exceptions to the ordered consumer.
            }
        });
    auto drain = [&] {
        auto &front = pending.front();
        auto game = front.game.get();
        if (game)
            consume(std::move(*game), front.offset);
        buffered -= front.bytes;
        pending.pop_front();
    };
    scanPgnRecords(
        input,
        [&](std::string &&record, uint64_t offset) {
            // Backpressure bounds both completed games and raw text waiting for parsing.
            while (!pending.empty() &&
                   (pending.size() >= size_t(workers * 2) || buffered + record.size() > 32 * 1024 * 1024))
                drain();
            const auto bytes = record.size();
            std::packaged_task<Parsed()> task([text = std::move(record)]() -> Parsed {
                auto games = parsePgn(text, 1);
                if (games.empty())
                    return std::nullopt;
                return std::move(games[0]);
            });
            pending.push_back({task.get_future(), offset, bytes});
            buffered += bytes;
            {
                std::lock_guard lock(pool.mutex);
                pool.tasks.push_back(std::move(task));
            }
            pool.ready.notify_one();
        },
        progress);
    while (!pending.empty())
        drain();
}
} // namespace t64
