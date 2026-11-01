#include "tapo_cipher.h"

#include <iomanip>
#include <sstream>

#include <mbedtls/aes.h>
#include <mbedtls/md.h>

namespace esphome {
namespace tapo {

TapoCipher::TapoCipher(const std::vector<uint8_t> &local_seed, const std::vector<uint8_t> &remote_seed,
                       const std::vector<uint8_t> &auth_hash) {
  auto local_hash = TapoCipher::concat(TapoCipher::concat(local_seed, remote_seed), auth_hash);
  this->key_ = this->key_derive(local_hash);
  auto iv_seq_pair = this->iv_derive(local_hash);
  this->iv_ = iv_seq_pair.first;
  this->seq_ = iv_seq_pair.second;
  this->sig_ = this->sig_derive(local_hash);
}

TapoCipher::EncryptedMessage TapoCipher::encrypt(const std::string &data) {
  this->seq_ += 1;
  auto iv_seq_data = this->iv_seq(this->seq_);

  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);
  mbedtls_aes_setkey_enc(&aes_ctx, this->key_.data(), this->key_.size() * 8);

  auto padded_data = this->pad(data);
  std::vector<uint8_t> encrypted_data(padded_data.size());
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, padded_data.size(), iv_seq_data.data(), padded_data.data(),
                        encrypted_data.data());
  mbedtls_aes_free(&aes_ctx);

  auto seq_bytes = TapoCipher::to_bytes(this->seq_);
  auto combined = TapoCipher::concat(TapoCipher::concat(this->sig_, seq_bytes), encrypted_data);
  auto signature = TapoCipher::sha256(combined);

  EncryptedMessage result;
  result.body = TapoCipher::concat(signature, encrypted_data);
  result.seq = this->seq_;
  return result;
}

bool TapoCipher::decrypt(uint32_t seq, const std::vector<uint8_t> &cipher_data, std::string &out) {
  if (cipher_data.size() < 32) {
    return false;
  }

  std::vector<uint8_t> encrypted_data(cipher_data.begin() + 32, cipher_data.end());
  if (encrypted_data.empty() || (encrypted_data.size() % 16) != 0) {
    return false;
  }

  auto signature=sha256(concat(this->sig_,to_bytes(seq),encrypted_data));
  if(signature.size()!=32)return false;
  unsigned difference=0;for(size_t i=0;i<32;i++)difference|=signature[i]^cipher_data[i];
  if(difference)return false;

  auto iv_seq_data = this->iv_seq(seq);

  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);
  mbedtls_aes_setkey_dec(&aes_ctx, this->key_.data(), this->key_.size() * 8);

  std::vector<uint8_t> decrypted_data(encrypted_data.size());
  int rc = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, encrypted_data.size(), iv_seq_data.data(),
                                 encrypted_data.data(), decrypted_data.data());
  mbedtls_aes_free(&aes_ctx);

  if (rc != 0) {
    return false;
  }

  return this->unpad(decrypted_data, out);
}

static std::vector<uint8_t> digest(mbedtls_md_type_t type, const std::vector<uint8_t> &data) {
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(type);
  if (info == nullptr) {
    return {};
  }
  std::vector<uint8_t> hash(mbedtls_md_get_size(info));
  mbedtls_md(info, data.data(), data.size(), hash.data());
  return hash;
}

std::vector<uint8_t> TapoCipher::sha1(const std::vector<uint8_t> &data) {
  return digest(MBEDTLS_MD_SHA1, data);
}

std::vector<uint8_t> TapoCipher::sha256(const std::vector<uint8_t> &data) {
  return digest(MBEDTLS_MD_SHA256, data);
}

std::vector<uint8_t> TapoCipher::concat(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
  std::vector<uint8_t> combined(a);
  combined.insert(combined.end(), b.begin(), b.end());
  return combined;
}

std::vector<uint8_t> TapoCipher::concat(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b,
                                        const std::vector<uint8_t> &c) {
  std::vector<uint8_t> combined(a);
  combined.insert(combined.end(), b.begin(), b.end());
  combined.insert(combined.end(), c.begin(), c.end());
  return combined;
}

std::string TapoCipher::to_hex_string(const std::vector<uint8_t> &data) {
  std::ostringstream oss;
  for (uint8_t byte : data) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  }
  return oss.str();
}

std::vector<uint8_t> TapoCipher::key_derive(const std::vector<uint8_t> &local_hash) {
  auto input = TapoCipher::concat({'l', 's', 'k'}, local_hash);
  auto hash = TapoCipher::sha256(input);
  return std::vector<uint8_t>(hash.begin(), hash.begin() + 16);
}

std::pair<std::vector<uint8_t>, uint32_t> TapoCipher::iv_derive(const std::vector<uint8_t> &local_hash) {
  auto input = TapoCipher::concat({'i', 'v'}, local_hash);
  auto hash = TapoCipher::sha256(input);
  std::vector<uint8_t> iv(hash.begin(), hash.begin() + 12);
  uint32_t seq = (static_cast<uint32_t>(hash[28]) << 24) | (static_cast<uint32_t>(hash[29]) << 16) |
                 (static_cast<uint32_t>(hash[30]) << 8) | (static_cast<uint32_t>(hash[31]));
  return {iv, seq};
}

std::vector<uint8_t> TapoCipher::sig_derive(const std::vector<uint8_t> &local_hash) {
  auto input = TapoCipher::concat({'l', 'd', 'k'}, local_hash);
  auto hash = TapoCipher::sha256(input);
  return std::vector<uint8_t>(hash.begin(), hash.begin() + 28);
}

std::vector<uint8_t> TapoCipher::iv_seq(uint32_t seq) {
  auto seq_bytes = TapoCipher::to_bytes(seq);
  return TapoCipher::concat(this->iv_, seq_bytes);
}

std::vector<uint8_t> TapoCipher::pad(const std::string &data) {
  size_t padding_len = 16 - (data.size() % 16);
  std::vector<uint8_t> padded(data.begin(), data.end());
  padded.insert(padded.end(), padding_len, static_cast<uint8_t>(padding_len));
  return padded;
}

bool TapoCipher::unpad(const std::vector<uint8_t> &data, std::string &out) {
  if (data.empty()) {
    return false;
  }

  uint8_t padding_len = data.back();
  if (padding_len == 0 || padding_len > 16 || padding_len > data.size()) {
    return false;
  }

  for (size_t i = data.size() - padding_len; i < data.size(); i++) {
    if (data[i] != padding_len) {
      return false;
    }
  }

  out.assign(data.begin(), data.end() - padding_len);
  return true;
}

std::vector<uint8_t> TapoCipher::to_bytes(uint32_t value) {
  return {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8),
          static_cast<uint8_t>(value)};
}

}  // namespace tapo
}  // namespace esphome
