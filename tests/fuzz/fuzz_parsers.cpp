// libFuzzer target for every function that parses text from the wire or the
// kernel: STATE/TIMING/OUTPUT/NOTE lines, `status` responses and hid ids.
// Build with `make test-fuzz`; the sanitizers turn any overflow or UB the
// fuzzer reaches into a crash.
#include <cstddef>
#include <cstdint>
#include <string>

#include "hid-unbind-detect.h"
#include "ipc-client.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const std::string input(reinterpret_cast<const char *>(data), size);

    ds4ipc::InputState state;
    ds4ipc::parse_state(input, &state);
    ds4ipc::TimingStats timing;
    ds4ipc::parse_timing(input, &timing);
    ds4ipc::OutputState output;
    ds4ipc::parse_output(input, &output);
    std::string source, text;
    ds4ipc::parse_note(input, &source, &text);
    ds4ipc::status_value(ds4ipc::parse_status(input), "Hide Method");
    (void)ds4ipc::validate_name(input);

    unsigned bus = 0, vendor = 0, product = 0;
    parse_hid_id(input, bus, vendor, product);

    std::string trimmed = input;
    ds4ipc::trim_response(&trimmed);
    return 0;
}
