/* Bounded inbound MapResponse authorization. Mattboxx Edition, MIT. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
struct pbuf;
typedef struct {
    bool authority_ready,peer_authority,filter_authority,self_authority;
    unsigned peers,expanded_rules,return_flows;
} ml_policy_status_t;
/* Non-identifying diagnostic counts; readiness is NOT a port grant. */
void ml_policy_get_status(ml_policy_status_t *out);
/* Age of the newest reply ADMITTED from remote ip:port (IPv4, network-order
 * bytes) on a flow this node sent, current epoch only. Evidence that the
 * service answered recently; never a grant and not per-LAN-client. */
bool ml_policy_reply_age(uint8_t protocol,const uint8_t remote_ip4[4],uint16_t remote_port,uint32_t *age_ms);
/* Single control-session writer; packet readers may run concurrently. */
bool ml_policy_apply(const cJSON *map);
/* Static reason code of the last rejection (diagnostics only). */
const char *ml_policy_last_reject(void);
void ml_policy_reset(void);
/* Transport interruption: block all data now, retain bounded return state
 * only for comparison with a fresh COMPLETE map. This is not offline access. */
void ml_policy_suspend(void);
/* True when complete authority was established after the last suspension or
 * reset (not just cached). Used so a stream continuing that same session can
 * apply deltas, while a transport loss still requires a fresh complete map. */
bool ml_policy_authority_active(void);
/* Cache reconciliation only: -1 unknown/changing authority, 0 obsolete, 1
 * currently visible key. Visibility NEVER grants a protocol/port permission. */
int ml_policy_peer_status(const uint8_t peer_key[32]);
/* Current visible NodeID/key only; never a port/protocol grant. */
int ml_policy_peer_key(uint64_t node_id,uint8_t peer_key[32]);
bool ml_policy_rx(const struct pbuf *packet, const uint8_t peer_key[32]);
bool ml_policy_tx(const struct pbuf *packet, const uint8_t peer_key[32]);
void ml_policy_sent(const struct pbuf *packet, const uint8_t peer_key[32]);
void ml_policy_sent_epoch(const struct pbuf *packet,const uint8_t peer_key[32],uint32_t expected_epoch);
/* Fragment retention is NOT a port grant. Every fragment needs current source
 * key ownership/local destination; reconstructed datagrams still pass RX. */
bool ml_policy_fragment_epoch(const uint8_t src[16],const uint8_t dst[16],
                              unsigned family,const uint8_t peer_key[32],uint32_t *out);
bool ml_policy_tx_fragment_epoch(const uint8_t src[16],const uint8_t dst[16],
                                 unsigned family,const uint8_t peer_key[32],uint32_t *out);
