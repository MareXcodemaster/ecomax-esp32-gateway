#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace tapo {

// KLAP v2 encryption session (AES-128-CBC + PKCS7 + sha256 signature).
// Ported from omegahiro/tapo-esp32 / python-kasa KlapTransportV2.
class TapoCipher {
 public:
  TapoCipher(const std::vector<uint8_t> &local_seed, const std::vector<uint8_t> &remote_seed,
             const std::vector<uint8_t> &auth_hash);

  struct EncryptedMessage {
    std::vector<uint8_t> body;  // 32-byte sha256 signature + ciphertext
    uint32_t seq;
  };

  EncryptedMessage encrypt(const std::string &data);
  bool decrypt(uint32_t seq, const std::vector<uint8_t> &cipher_data, std::string &out);

  static std::vector<uint8_t> sha1(const std::vector<uint8_t> &data);
  static std::vector<uint8_t> sha256(const std::vector<uint8_t> &data);
  static std::vector<uint8_t> concat(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b);
  static std::vector<uint8_t> concat(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b,
                                     const std::vector<uint8_t> &c);
  static std::string to_hex_string(const std::vector<uint8_t> &data);

 private:
  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
  std::vector<uint8_t> sig_;
  uint32_t seq_;

  std::vector<uint8_t> key_derive(const std::vector<uint8_t> &local_hash);
  std::pair<std::vector<uint8_t>, uint32_t> iv_derive(const std::vector<uint8_t> &local_hash);
  std::vector<uint8_t> sig_derive(const std::vector<uint8_t> &local_hash);
  std::vector<uint8_t> iv_seq(uint32_t seq);
  std::vector<uint8_t> pad(const std::string &data);
  bool unpad(const std::vector<uint8_t> &data, std::string &out);
  static std::vector<uint8_t> to_bytes(uint32_t value);
};

}  // namespace tapo
}  // namespace esphome
