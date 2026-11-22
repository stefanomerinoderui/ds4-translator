#include "singleinstance.h"

#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <QStandardPaths>

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// One socket per user session. The runtime directory is per-user and cleaned
// up at logout, which is exactly the lifetime this guard should have.
const char *const kSocketName = "ds4-translator-ui.socket";

// The running instance answers immediately or not at all; this is a local
// socket to a process that is already up, so there is nothing slow to wait
// for, and a hung peer must not delay the launch.
constexpr int kConnectTimeoutMs = 300;

const char *const kShowMessage = "show\n";

// Creates `path` for this user alone and verifies it really is that: a
// directory (not a symlink) owned by us with no group/other access. The
// fallback location is under the world-writable temp dir, where another user
// could otherwise pre-create the name and then hijack or block the socket.
bool ensurePrivateDirectory(const QString &path) {
    const QByteArray native = QFile::encodeName(path);
    if (::mkdir(native.constData(), 0700) != 0 && errno != EEXIST) {
        return false;
    }
    struct stat info;
    if (::lstat(native.constData(), &info) != 0) {
        return false;
    }
    return S_ISDIR(info.st_mode) && info.st_uid == ::geteuid() && (info.st_mode & 077) == 0;
}

} // namespace

SingleInstance::SingleInstance(QObject *parent) : QObject(parent) {
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *connection = m_server.nextPendingConnection()) {
            // The message itself carries no information beyond "someone tried
            // to launch me again", so it is not read: the connection is the
            // signal.
            connection->deleteLater();
            emit showRequested();
        }
    });
}

QString SingleInstance::socketPath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty()) {
        // No XDG_RUNTIME_DIR (a bare login shell, some containers). TempLocation
        // is world-writable, so the name is qualified with the user id to keep
        // two users' sockets apart.
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
              QLatin1String("/ds4-translator-ui-") + QString::number(::geteuid());
        if (!ensurePrivateDirectory(dir)) {
            return QString(); // not ours to use; callers treat this as "no guard"
        }
    }
    return dir + QLatin1Char('/') + QLatin1String(kSocketName);
}

bool SingleInstance::notifyRunningInstance() {
    const QString path = socketPath();
    if (path.isEmpty()) {
        return false;
    }
    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        return false;
    }
    socket.write(kShowMessage);
    socket.waitForBytesWritten(kConnectTimeoutMs);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen() {
    const QString path = socketPath();
    if (path.isEmpty()) {
        return false;
    }
    if (m_server.listen(path)) {
        return true;
    }
    // A socket file left behind by an instance that was killed rather than
    // shut down. Nothing answered it a moment ago (notifyRunningInstance()
    // runs first), so it is stale and safe to take over.
    QLocalServer::removeServer(path);
    return m_server.listen(path);
}
