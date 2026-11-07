/* Current metadata mailbox, independent of packet permission. SPDX-License-Identifier: MIT */
#pragma once
#ifndef ML_PEER_SYNC_HOST_TEST
#include "microlink_internal.h"
#endif
bool ml_peer_sync_record(const ml_peer_update_t *update);
bool ml_peer_sync_resolve(ml_peer_update_t *update);
