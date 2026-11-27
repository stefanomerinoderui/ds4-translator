#ifndef DS4_UI_CALIBRATIONCONTROLLER_H
#define DS4_UI_CALIBRATIONCONTROLLER_H

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

#include "ipcclient.h"
#include "stick-coverage.h"

// Drives one stick or trigger calibration of the physical controller, as a
// wizard: the same daemon steps `ds4-ctl calibrate` sends (start, samples,
// store; built by ds4ipc::build_calibrate), with the terminal prompts turned
// into pages the user moves through.
//
// The flow follows the GUI calibrators (dualshock-tools above all): a
// warning to confirm first, one instruction per page with the sticks drawn
// live, a stick-range page that only unlocks once both sticks have been
// rotated through enough full circles, and an explicit choice at the end
// between storing until the next reset and storing permanently.
//
// Like ds4-ctl it checks the controller is on USB before asking anything:
// the daemon refuses calibration over Bluetooth.
class CalibrationController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum Phase {
        Idle,       // no wizard open
        Checking,   // asking the daemon how the pad is connected
        Confirm,    // warning shown, waiting for the user to go ahead
        Working,    // a command is in flight, or a pause between samples
        Instruct,   // an instruction is shown, waiting for Continue
        Rotating,   // stick range: waiting for the sticks to go round
        ChooseStore,// last page: store until reset, or permanently
        Done,
        Failed,
    };
    Q_ENUM(Phase)

    Q_PROPERTY(Phase phase READ phase NOTIFY changed)
    Q_PROPERTY(QString target READ target NOTIFY changed)
    // "Step N of M" over the pages the user sees.
    Q_PROPERTY(int stepIndex READ stepIndex NOTIFY changed)
    Q_PROPERTY(int stepCount READ stepCount NOTIFY changed)
    Q_PROPERTY(QString instruction READ instruction NOTIFY changed)
    // The daemon's reply to the last command, or what went wrong.
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool canContinue READ canContinue NOTIFY changed)

    // Stick range only.
    Q_PROPERTY(double rangeProgress READ rangeProgress NOTIFY coverageChanged)
    Q_PROPERTY(int leftCircles READ leftCircles NOTIFY coverageChanged)
    Q_PROPERTY(int rightCircles READ rightCircles NOTIFY coverageChanged)
    Q_PROPERTY(int targetCircles READ targetCircles CONSTANT)
    // Set when the sticks have barely moved a few seconds in.
    Q_PROPERTY(bool keepRotatingHint READ keepRotatingHint NOTIFY coverageChanged)

    // A calibration was stored without "permanent": the pad uses it until it
    // resets or goes into standby. Cleared by a reset or a permanent store.
    Q_PROPERTY(bool unsavedChanges READ unsavedChanges NOTIFY changed)
    // True while the wizard has sent "start" and not yet stored, so cancelling
    // leaves the pad mid-calibration.
    Q_PROPERTY(bool startedOnPad READ startedOnPad NOTIFY changed)

    explicit CalibrationController(QObject *parent = nullptr);

    Phase phase() const { return m_phase; }
    QString target() const { return m_target; }
    int stepIndex() const { return m_stepIndex; }
    int stepCount() const { return m_stepCount; }
    QString instruction() const { return m_instruction; }
    QString message() const { return m_message; }
    bool canContinue() const;
    double rangeProgress() const { return m_coverage.progress(); }
    int leftCircles() const { return m_coverage.circles(StickCoverage::Left); }
    int rightCircles() const { return m_coverage.circles(StickCoverage::Right); }
    int targetCircles() const { return StickCoverage::kTargetCircles; }
    bool keepRotatingHint() const { return m_keepRotatingHint; }
    bool unsavedChanges() const { return m_unsavedChanges; }
    bool startedOnPad() const { return m_startedOnPad; }

    Q_INVOKABLE QStringList targets() const;

    // Opens the wizard for a target (stick-center | stick-range | triggers).
    Q_INVOKABLE void begin(const QString &target);
    // Confirm, Instruct and Rotating pages: go on.
    Q_INVOKABLE void next();
    // ChooseStore page.
    Q_INVOKABLE void store(bool permanent);
    // Closes the wizard. If a calibration was started on the pad and not
    // stored, also resets the pad to end it.
    Q_INVOKABLE void cancel();
    // Reboots the pad: undoes anything not stored permanently.
    Q_INVOKABLE void resetController();

    // Live physical stick values (0..255) while the range page is up.
    Q_INVOKABLE void addStickSample(int lx, int ly, int rx, int ry);

signals:
    void changed();
    void coverageChanged();

private slots:
    void onReplyReady(const QString &command, bool ok, const QString &response);

private:
    struct Action {
        enum Kind { Command, Prompt, Rotate, Pause, Store };
        Kind kind;
        QString text;    // Prompt: the instruction; Command: the wire command
        int pauseMs = 0; // Pause
        bool page = false; // counts as a "Step N of M" page
    };

    QList<Action> buildActions(const QString &target, QString *error) const;
    void runNext();
    void fail(const QString &message);
    void setPhase(Phase phase);
    void send(const QString &command);

    IpcClient *m_ipc;
    QTimer *m_pauseTimer;
    QTimer *m_rotateTicker;
    QElapsedTimer m_rotateClock;

    Phase m_phase = Idle;
    QString m_target;
    QList<Action> m_actions;
    int m_stepIndex = 0;
    int m_stepCount = 0;
    QString m_instruction;
    QString m_message;
    bool m_storingPermanently = false;

    StickCoverage m_coverage;
    bool m_keepRotatingHint = false;
    bool m_rotateTimeUp = false;
    bool m_unsavedChanges = false;
    bool m_startedOnPad = false;
};

#endif // DS4_UI_CALIBRATIONCONTROLLER_H
