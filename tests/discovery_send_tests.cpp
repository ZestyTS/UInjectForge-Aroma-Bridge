#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

static std::atomic<uint32_t> sReplyCount{0}, sBeaconCount{0}, sSendFailureCount{0}, sLastSendBytes{0};
static std::atomic<int32_t> sLastSendError{0};
static constexpr size_t kMaxDiscoveryPayloadBytes = 1280;
static unsigned sendCalls = 0;
static bool fullSend = true;
static ssize_t fakeResult = -1;
static int fakeError = 0;
static ssize_t fake_sendto(int, const void *, size_t length, int, const sockaddr *, socklen_t) {
    ++sendCalls;
    errno = fakeError;
    return fullSend ? static_cast<ssize_t>(length) : fakeResult;
}
#define sendto fake_sendto
#include "discovery_send.inc"
#undef sendto

int main() {
    const sockaddr_in target{};
    const std::string payload = "{}";
    send_discovery_datagram(4, target, payload, false);
    assert(sReplyCount == 1 && sBeaconCount == 0 && sSendFailureCount == 0 && sLastSendBytes == 2);
    send_discovery_datagram(4, target, payload, true);
    assert(sReplyCount == 1 && sBeaconCount == 1);
    fullSend = false;
    for (const auto error : {EAGAIN, EMSGSIZE, ENOTSOCK}) {
        fakeError = error;
        send_discovery_datagram(4, target, payload, false);
        assert(sLastSendError == error && sReplyCount == 1 && sBeaconCount == 1);
    }
    fakeResult = 1;
    send_discovery_datagram(4, target, payload, true);
    assert(sLastSendError == EIO && sSendFailureCount == 4 && sBeaconCount == 1);
    fullSend = true;
    send_discovery_datagram(4, target, payload, false);
    assert(sLastSendError == 0 && sReplyCount == 2 && sSendFailureCount == 4);
    const auto callsBefore = sendCalls;
    send_discovery_datagram(4, target, "", false);
    send_discovery_datagram(4, target, std::string(kMaxDiscoveryPayloadBytes + 1, 'x'), false);
    assert(sendCalls == callsBefore && sLastSendError == EMSGSIZE && sSendFailureCount == 6 && sReplyCount == 2);
    send_discovery_datagram(4, target, std::string(kMaxDiscoveryPayloadBytes, 'x'), false);
    assert(sendCalls == callsBefore + 1 && sLastSendError == 0 && sReplyCount == 3);
    std::puts("Bridge production discovery send: success, failure, short-send and recovery passed.");
}
