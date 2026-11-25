#ifndef BT_SUPPRESSION_H
#define BT_SUPPRESSION_H
// Bookkeeping for the Bluetooth connection the daemon unbinds when the same
// pad is plugged in over USB (hid-playstation refuses a second connection for
// a MAC it already holds, so Bluetooth has to let go for USB to bind). The
// Bluetooth hid_device then sits driverless, and someone has to bind it back
// once the USB connection is gone -- or the pad sends nothing until it is
// re-paired.
//
// The rule that matters: Bluetooth only gets its driver back once no USB
// connection for a DS4 is left. While USB is there, hid-playstation refuses
// the bind ("File exists"), so giving it back early does nothing except lose
// track of it. That is what happened when the daemon dropped its connection
// on its own (a type, backend or hide-method change, release-physical) while
// on USB: it rebound Bluetooth, the bind failed, the record was cleared, and
// pulling the cable afterwards left the pad dead.
#include <functional>
#include <string>

class BtSuppression {
public:
    struct Ops {
        // A USB DS4 hid_device that is not our own emulated pad exists.
        std::function<bool()> usb_present;
        // The hid_device still exists (the pad has not left Bluetooth).
        std::function<bool(const std::string& hid_id)> exists;
        // Binds it to hid-playstation; true once it has a driver.
        std::function<bool(const std::string& hid_id)> bind;
    };

    explicit BtSuppression(Ops ops) : ops_(std::move(ops)) {}

    // Bluetooth was unbound so USB can take over.
    void suppress(const std::string& bt_hid_id) { hid_id_ = bt_hid_id; }

    // Hands Bluetooth back if USB is gone; otherwise keeps it for later (see
    // poll()). For any moment the daemon lets go of a connection: the USB
    // side dropping, or a type/backend change or release on our side.
    void release() {
        if (!hid_id_.empty() && !ops_.usb_present()) restore();
    }

    // Called periodically: catches USB leaving while no connection of ours
    // was there to notice (released, or between two connections).
    void poll() { release(); }

    // Unconditional attempt, for shutdown: the daemon will not be around to
    // retry.
    void restore_now() {
        if (!hid_id_.empty()) restore();
    }

    bool pending() const { return !hid_id_.empty(); }
    const std::string& hid_id() const { return hid_id_; }

private:
    // Forgotten once bound, or once the pad is gone from Bluetooth altogether
    // (nothing left to bind). A refused bind is kept and retried by poll().
    void restore() {
        if (!ops_.exists(hid_id_) || ops_.bind(hid_id_)) hid_id_.clear();
    }

    Ops ops_;
    std::string hid_id_;
};

#endif // BT_SUPPRESSION_H
