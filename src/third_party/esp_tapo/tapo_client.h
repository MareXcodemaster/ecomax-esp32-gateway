#pragma once

#include <string>
#include <vector>

#include "tapo_cipher.h"

namespace esphome {
namespace tapo {

// Abstraction for the underlying HTTP transport. The KLAP client uses this so
// the protocol logic is fully host-testable; implementations exist for
// ESPHome sockets (device) and POSIX sockets (host tests).
class TapoTransport {
 public:
  virtual ~TapoTransport() = default;

  struct Response {
    int status = -1;
    std::string body;        // may contain binary (NUL) bytes
    std::string set_cookie;  // raw Set-Cookie header value, or ""
  };

  // POST `path` (absolute, e.g. "/app/handshake1") with binary `body`,
  // including the optional `cookie` header value if non-empty.
  virtual Response post(const std::string &path, const std::string &body, const std::string &cookie) = 0;

  // Close the current TCP connection (if any). The next post() will reopen one.
  // Called at the start of each handshake so it always begins on a clean link.
  virtual void reset() = 0;
};

class TapoClient {
 public:
  void set_transport(TapoTransport *transport) { this->transport_ = transport; }

  ~TapoClient(){reset();}
  bool handshake(const std::vector<uint8_t> &auth_hash);
  std::string send_message(const std::string &message);

  void reset();
  bool is_ready() const { return this->cipher_ != nullptr; }
  const std::string &get_error_message() const { return this->error_message_; }

  static std::vector<uint8_t> random_bytes(size_t size);

 private:
  TapoTransport *transport_ = nullptr;
  TapoCipher *cipher_ = nullptr;
  std::string cookie_;  // TP_SESSIONID value
  std::string error_message_;

  bool handshake1(const std::vector<uint8_t> &local_seed, const std::vector<uint8_t> &auth_hash,
                  std::vector<uint8_t> &remote_seed);
  bool handshake2(const std::vector<uint8_t> &local_seed, const std::vector<uint8_t> &remote_seed,
                  const std::vector<uint8_t> &auth_hash);
  std::string cookie_header() const { return "TP_SESSIONID=" + this->cookie_; }
};

}  // namespace tapo
}  // namespace esphome
