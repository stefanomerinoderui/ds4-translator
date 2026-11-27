#include "calibrationcontroller.h"

#include <QStringList>

#include "ipc-client.h"

namespace {

constexpr auto kStatusCommand = QLatin1String("status");

// Stick centre: the pad averages a few samples taken a moment apart, as
// ds4-ctl does.
constexpr int kCenterSamples = 5;
constexpr int kCenterSamplePauseMs = 100;

// Stick range, after dualshock-tools: Continue unlocks once both sticks are
// done, or after this long regardless (a worn stick may never reach the
// full-circle threshold); the hint appears if they have barely moved by then.
constexpr qint64 kRotateUnlockMs = 15000;
constexpr qint64 kRotateHintAfterMs = 5000;
constexpr double kRotateHintBelowProgress = 0.1;
constexpr int kRotateTickMs = 250;

QStringList toStringList(const std::vector<std::string> &values) {
    QStringList list;
    for (const std::string &value : values) {
        list.append(QString::fromStdString(value));
    }
    return list;
}

} // namespace

CalibrationController::CalibrationController(QObject *parent)
    : QObject(parent), m_ipc(new IpcClient(this)), m_pauseTimer(new QTimer(this)),
      m_rotateTicker(new QTimer(this)) {
    connect(m_ipc, &IpcClient::replyReady, this, &CalibrationController::onReplyReady);

    m_pauseTimer->setSingleShot(true);
    connect(m_pauseTimer, &QTimer::timeout, this, &CalibrationController::runNext);

    // The unlock and the hint depend on time passing, not only on samples
    // arriving, so they are re-evaluated on a tick.
    m_rotateTicker->setInterval(kRotateTickMs);
    connect(m_rotateTicker, &QTimer::timeout, this, [this]() {
        const qint64 elapsed = m_rotateClock.elapsed();
        const bool hint = elapsed >= kRotateHintAfterMs && m_coverage.progress() < kRotateHintBelowProgress;
        const bool timeUp = elapsed >= kRotateUnlockMs;
        if (hint != m_keepRotatingHint) {
            m_keepRotatingHint = hint;
            emit coverageChanged();
        }
        if (timeUp != m_rotateTimeUp) {
            m_rotateTimeUp = timeUp;
            emit changed();
        }
    });
}

QStringList CalibrationController::targets() const {
    return toStringList(ds4ipc::calibration_target_values());
}

bool CalibrationController::canContinue() const {
    switch (m_phase) {
    case Confirm:
    case Instruct:
        return true;
    case Rotating:
        return m_coverage.complete() || m_rotateTimeUp;
    default:
        return false;
    }
}

QList<CalibrationController::Action> CalibrationController::buildActions(const QString &target,
                                                                          QString *error) const {
    const std::string t = target.toStdString();
    QList<Action> actions;
    std::string buildError;
    auto command = [&](const std::string &step, int position = 0, int side = 0) {
        const std::string wire = ds4ipc::build_calibrate(t, step, position, side, false, &buildError);
        actions.append({Action::Command, QString::fromStdString(wire)});
        return !wire.empty();
    };

    if (!command("start")) {
        *error = QString::fromStdString(buildError);
        return {};
    }
    if (target == QLatin1String("stick-center")) {
        actions.append({Action::Prompt, tr("Let go of both sticks so they rest in the centre, then press "
                                           "Continue. Do not touch them until the next page."),
                        0, true});
        for (int i = 0; i < kCenterSamples; ++i) {
            command("sample");
            actions.append({Action::Pause, QString(), kCenterSamplePauseMs});
        }
    } else if (target == QLatin1String("stick-range")) {
        actions.append({Action::Rotate,
                        tr("Slowly rotate both sticks around their outer edge, pushed all the way, "
                           "until each has gone round %1 times. Then let go and press Continue.")
                            .arg(StickCoverage::kTargetCircles),
                        0, true});
    } else {
        const QString sides[] = {tr("L2"), tr("R2")};
        const QString positions[] = {tr("fully released"), tr("pressed halfway"), tr("pressed all the way")};
        for (int side = 1; side <= 2; ++side) {
            for (int position = 1; position <= 3; ++position) {
                actions.append({Action::Prompt,
                                tr("Hold %1 %2, then press Continue.")
                                    .arg(sides[side - 1], positions[position - 1]),
                                0, true});
                // The pad wants each position sampled twice.
                command("sample", position, side);
                command("sample", position, side);
            }
        }
    }
    actions.append({Action::Store, QString(), 0, true});
    return actions;
}

void CalibrationController::begin(const QString &target) {
    if (m_phase != Idle && m_phase != Done && m_phase != Failed) {
        return;
    }
    QString error;
    m_actions = buildActions(target, &error);
    m_target = target;
    m_message.clear();
    m_instruction.clear();
    m_startedOnPad = false;
    m_stepIndex = 0;
    m_stepCount = 0;
    for (const Action &action : m_actions) {
        if (action.page) {
            ++m_stepCount;
        }
    }
    if (m_actions.isEmpty()) {
        fail(error);
        return;
    }
    // Calibration is USB only; ask before showing the warning, so the user is
    // not asked to confirm something the daemon would then refuse.
    setPhase(Checking);
    send(kStatusCommand);
}

void CalibrationController::next() {
    if (!canContinue()) {
        return;
    }
    if (m_phase == Rotating) {
        m_rotateTicker->stop();
    }
    runNext();
}

void CalibrationController::store(bool permanent) {
    if (m_phase != ChooseStore) {
        return;
    }
    std::string error;
    const std::string wire =
        ds4ipc::build_calibrate(m_target.toStdString(), "store", 0, 0, permanent, &error);
    if (wire.empty()) {
        fail(QString::fromStdString(error));
        return;
    }
    m_storingPermanently = permanent;
    m_instruction = tr("Storing the calibration…");
    setPhase(Working);
    send(QString::fromStdString(wire));
}

void CalibrationController::cancel() {
    m_pauseTimer->stop();
    m_rotateTicker->stop();
    m_actions.clear();
    m_instruction.clear();
    const bool leftOpen = m_startedOnPad;
    setPhase(Idle);
    // The pad keeps an unstored calibration open until it resets, and the
    // next start is refused until then; it has a battery, so unplugging it
    // does not help.
    if (leftOpen) {
        resetController();
    }
}

void CalibrationController::resetController() {
    m_message = tr("Resetting the controller…");
    send(QLatin1String(ds4ipc::kResetControllerCommand));
    emit changed();
}

void CalibrationController::addStickSample(int lx, int ly, int rx, int ry) {
    if (m_phase != Rotating) {
        return;
    }
    const bool wasComplete = m_coverage.complete();
    m_coverage.add(static_cast<uint8_t>(qBound(0, lx, 255)), static_cast<uint8_t>(qBound(0, ly, 255)),
                   static_cast<uint8_t>(qBound(0, rx, 255)), static_cast<uint8_t>(qBound(0, ry, 255)));
    emit coverageChanged();
    if (m_coverage.complete() != wasComplete) {
        emit changed(); // canContinue
    }
}

void CalibrationController::runNext() {
    if (m_actions.isEmpty()) {
        return;
    }
    const Action action = m_actions.takeFirst();
    if (action.page) {
        ++m_stepIndex;
    }
    switch (action.kind) {
    case Action::Command:
        setPhase(Working);
        send(action.text);
        break;
    case Action::Pause:
        setPhase(Working);
        m_pauseTimer->start(action.pauseMs);
        break;
    case Action::Prompt:
        m_instruction = action.text;
        setPhase(Instruct);
        break;
    case Action::Rotate:
        m_instruction = action.text;
        m_coverage.reset();
        m_keepRotatingHint = false;
        m_rotateTimeUp = false;
        m_rotateClock.start();
        m_rotateTicker->start();
        emit coverageChanged();
        setPhase(Rotating);
        break;
    case Action::Store:
        m_instruction = tr("Calibration done. Store it until the controller next resets, or "
                           "permanently?");
        setPhase(ChooseStore);
        break;
    }
}

void CalibrationController::onReplyReady(const QString &command, bool ok, const QString &response) {
    const bool isReset = command == QLatin1String(ds4ipc::kResetControllerCommand);
    const bool failed = !ok || response.startsWith(QLatin1String("Error"));

    if (isReset) {
        m_message = response;
        if (!failed) {
            m_unsavedChanges = false;
            m_startedOnPad = false;
        }
        emit changed();
        return;
    }

    if (command == kStatusCommand) {
        if (m_phase != Checking) {
            return;
        }
        if (!ok) {
            fail(response);
            return;
        }
        const std::vector<ds4ipc::StatusField> fields = ds4ipc::parse_status(response.toStdString());
        const QString connection = QString::fromStdString(ds4ipc::status_value(fields, "Connection Type"));
        if (connection != QLatin1String("USB")) {
            fail(connection.isEmpty() || connection == QLatin1String("N/A")
                     ? tr("No controller connected. Plug it in with a USB cable.")
                     : tr("Calibration needs the controller on USB, but it is connected over %1. "
                          "Unplug its USB cable (or unpair the Bluetooth link), connect it by cable "
                          "and try again.")
                           .arg(connection));
            return;
        }
        setPhase(Confirm);
        return;
    }

    if (m_phase == Idle) {
        return; // cancelled while this was in flight
    }
    if (failed) {
        fail(response);
        return;
    }
    m_message = response;
    if (command.endsWith(QLatin1String(" start"))) {
        m_startedOnPad = true;
    }
    if (command.contains(QLatin1String(" store"))) {
        m_startedOnPad = false;
        // A permanent store leaves any other target stored temporarily as it was.
        if (!m_storingPermanently) {
            m_unsavedChanges = true;
        }
        setPhase(Done);
        return;
    }
    runNext();
}

void CalibrationController::fail(const QString &message) {
    m_pauseTimer->stop();
    m_rotateTicker->stop();
    m_actions.clear();
    m_message = message;
    setPhase(Failed);
}

void CalibrationController::setPhase(Phase phase) {
    m_phase = phase;
    emit changed();
}

void CalibrationController::send(const QString &command) { m_ipc->send(command); }
