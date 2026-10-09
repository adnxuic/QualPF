#pragma once
#include "gui/mainwindow.h"
#include <QEventLoop>
#include <QTimer>
#include <stdexcept>

// Test driver only: normal event-loop dispatch, including any accepted internal
// continuation. Product code never spins or blocks waiting for a worker.
inline void awaitSearch(MainWindow &window) {
    auto *tasks = window.searchTasks();
    if (!tasks->busy()) return;
    QEventLoop loop;
    bool timeout = false;
    auto connection = QObject::connect(tasks, &qualpf::gui::SearchTaskController::finished, &loop,
        [&] { if (!tasks->busy()) loop.quit(); });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&] { timeout = true; tasks->cancel(); });
    watchdog.start(120000);
    loop.exec();
    QObject::disconnect(connection);
    if (timeout) throw std::runtime_error("Background search watchdog expired");
}
