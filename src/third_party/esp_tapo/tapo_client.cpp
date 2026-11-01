#include "tapo_client.h"

#include "esp_random.h"

namespace esphome {
namespace tapo {

static std::string trim_copy(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) {
    return "";
  }
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

static std::string extract_session_cookie(const std::string &set_cookie) {
  size_t p = set_cookie.find("TP_SESSIONID=");
  if (p == std::string::npos) {
    return "";
  }
  p += 13;
  size_t end = set_cookie.find(';', p);
  if (end == std::string::npos) {
    end = set_cookie.size();
  }
  return trim_copy(set_cookie.substr(p, end - p));
}

std::vector<uint8_t> TapoClient::random_bytes(size_t size) {
  std::vector<uint8_t> data(size);
  esp_fill_random(data.data(),size);
  return data;
}

bool TapoClient::handshake(const std::vector<uint8_t> &auth_hash) {
  if(auth_hash.size()!=32||!this->transport_)return false;
  if (this->cipher_ != nullptr) {
    delete this->cipher_;
    this->cipher_ = nullptr;
  }
  this->cookie_.clear();
  // Start each handshake on a fresh connection (handshake1+2 share one TCP link).
  this->transport_->reset();

  auto local_seed = TapoClient::random_bytes(16);
  std::vector<uint8_t> remote_seed;
  if (!this->handshake1(local_seed, auth_hash, remote_seed)) {
    return false;
  }
  if (!this->handshake2(local_seed, remote_seed, auth_hash)) {
    return false;
  }

  this->cipher_ = new TapoCipher(local_seed, remote_seed, auth_hash);
  this->error_message_.clear();
  return true;
}

bool TapoClient::handshake1(const std::vector<uint8_t> &local_seed, const std::vector<uint8_t> &auth_hash,
                            std::vector<uint8_t> &remote_seed) {
  std::string body(local_seed.begin(), local_seed.end());
  auto response = this->transport_->post("/app/handshake1", body, "");
  if (response.status != 200) {
    if (response.status == -1) {
      this->error_message_ = "no connection to the device (check WiFi or IP)";
    } else {
      this->error_message_ =
          "handshake1 HTTP " + std::to_string(response.status) +
          " (is 'Third-Party Compatibility' enabled in the Tapo app?)";
    }
    return false;
  }
  if (response.body.size() != 48) {
    this->error_message_ = "handshake1 response too short";
    return false;
  }

  remote_seed.assign(response.body.begin(), response.body.begin() + 16);
  std::vector<uint8_t> server_hash(response.body.begin() + 16, response.body.end());

  auto local_hash = TapoCipher::sha256(TapoCipher::concat(TapoCipher::concat(local_seed, remote_seed), auth_hash));
  if (local_hash != server_hash) {
    this->error_message_ = "invalid credentials (Tapo app username/password)";
    return false;
  }

  this->cookie_ = extract_session_cookie(response.set_cookie);
  return true;
}

bool TapoClient::handshake2(const std::vector<uint8_t> &local_seed, const std::vector<uint8_t> &remote_seed,
                            const std::vector<uint8_t> &auth_hash) {
  auto payload = TapoCipher::sha256(TapoCipher::concat(TapoCipher::concat(remote_seed, local_seed), auth_hash));
  std::string body(payload.begin(), payload.end());
  auto response = this->transport_->post("/app/handshake2", body, this->cookie_header());
  if (response.status != 200) {
    std::string detail = response.body.size() <= 80 ? response.body : response.body.substr(0, 80);
    this->error_message_ = "handshake2 HTTP " + std::to_string(response.status) + " (" + detail + ")";
    return false;
  }
  return true;
}

std::string TapoClient::send_message(const std::string &message) {
  if (this->cipher_ == nullptr) {
    this->error_message_ = "no session (run the handshake first)";
    return "";
  }

  auto encrypted = this->cipher_->encrypt(message);
  std::string body(encrypted.body.begin(), encrypted.body.end());
  std::string path = "/app/request?seq=" + std::to_string(encrypted.seq);

  auto response = this->transport_->post(path, body, this->cookie_header());
  if (response.status != 200) {
    // 403 typically means the session is no longer valid -> re-handshake.
    if (response.status == -1) {
      this->error_message_ = "request: no connection (check WiFi or IP)";
    } else {
      this->error_message_ = "request HTTP " + std::to_string(response.status);
    }
    this->reset();
    return "";
  }

  std::string decrypted;
  if (!this->cipher_->decrypt(encrypted.seq, std::vector<uint8_t>(response.body.begin(), response.body.end()),
                              decrypted)) {
    this->error_message_ = "failed to decrypt the response";
    this->reset();
    return "";
  }
  return decrypted;
}

void TapoClient::reset() {
  if (this->cipher_ != nullptr) {
    delete this->cipher_;
    this->cipher_ = nullptr;
  }
  this->cookie_.clear();
}

}  // namespace tapo
}  // namespace esphome
