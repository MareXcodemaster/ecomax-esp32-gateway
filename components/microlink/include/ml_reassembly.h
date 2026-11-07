/* Bounded plaintext tunnel reassembly, before policy/NAT. SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stdbool.h>
struct pbuf;
/* Consumes input on ALL fragment outcomes. Complete result owns a new pbuf;
 * non-fragments return the original. NULL means retained or refused. */
struct pbuf *ml_reassemble_rx(struct pbuf *packet,const uint8_t peer_key[32]);
/* 0 = not fragmented (normal policy required), 1 = owned TCP/UDP fragment,
 * -1 = refused. Never borrows/consumes the caller's output pbuf. */
int ml_fragment_tx_check(const struct pbuf *packet,const uint8_t peer_key[32]);
/* Call once with the actual WG transport handoff result. Only a complete
 * set of successfully handed-off fragments may create bounded return state. */
void ml_fragment_tx_result(const struct pbuf *packet,const uint8_t peer_key[32],bool success);
/* Lifecycle reset, no allocation; also gives the linker a strong dependency. */
void ml_reassembly_clear(void);
