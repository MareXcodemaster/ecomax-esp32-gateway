/* Merge deltas before queueing, resolve to latest metadata before installation.
 * One coordinator writer, bounded cache; no NVS/socket/packet-path work here.
 * SPDX-License-Identifier: MIT */
#include "ml_peer_sync.h"
#include "ml_policy.h"
#include <stdlib.h>
#include <string.h>
#ifdef ML_PEER_SYNC_HOST_TEST
#include <pthread.h>
static pthread_mutex_t sync_lock=PTHREAD_MUTEX_INITIALIZER;
#define SLOCK() pthread_mutex_lock(&sync_lock)
#define SUNLOCK() pthread_mutex_unlock(&sync_lock)
extern void *peer_sync_test_alloc(size_t);
#define SALLOC(n) peer_sync_test_alloc(n)
#else
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static portMUX_TYPE sync_lock=portMUX_INITIALIZER_UNLOCKED;
#define SLOCK() taskENTER_CRITICAL(&sync_lock)
#define SUNLOCK() taskEXIT_CRITICAL(&sync_lock)
#define SALLOC(n) heap_caps_malloc((n),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)
#endif
typedef struct {bool used;ml_peer_update_t desired;} sync_entry;
static sync_entry *sync_entries;
bool ml_peer_sync_record(const ml_peer_update_t *u){
    if(!u||!u->has_node_id)return false;
    uint8_t key[32];int authority=ml_policy_peer_key(u->node_id,key);
    if(authority<0)return false;
    if(!authority)return true; /* Revoked/expired peer: manager independently refuses it. */
    static const uint8_t zero[32]={0};
    if(memcmp(u->public_key,zero,32)&&memcmp(u->public_key,key,32))return false;
    /* Only the coordinator allocates; readers see NULL until fully zeroed. */
    if(!sync_entries){sync_entry *fresh=SALLOC(ML_MAX_PEERS*sizeof *fresh);if(!fresh)return false;
        memset(fresh,0,ML_MAX_PEERS*sizeof *fresh);SLOCK();sync_entries=fresh;SUNLOCK();}
    int slot=-1;
    /* Writer-only selection. Calls to policy never occur under the mailbox
     * lock, avoiding policy/mailbox lock inversion with the WG consumer. */
    for(int i=0;i<ML_MAX_PEERS;i++)if(sync_entries[i].used&&sync_entries[i].desired.node_id==u->node_id){slot=i;break;}
    if(slot<0)for(int i=0;i<ML_MAX_PEERS;i++)if(!sync_entries[i].used||ml_policy_peer_status(sync_entries[i].desired.public_key)==0){slot=i;break;}
    if(slot<0)return false;
    SLOCK();sync_entry *e=&sync_entries[slot];
    if(u->action==ML_PEER_ADD){
        bool keep_online=e->used&&!memcmp(e->desired.public_key,key,32)&&e->desired.has_online&&!u->has_online;
        bool online=e->desired.online;
        e->desired=*u;e->used=true;e->desired.has_endpoints=e->desired.has_derp_region=true;
        if(keep_online){e->desired.has_online=true;e->desired.online=online;}
    }
    else if(u->action==ML_PEER_UPDATE_ENDPOINT && e->used && e->desired.node_id==u->node_id&&!memcmp(e->desired.public_key,key,32)){
        if(u->has_endpoints){e->desired.endpoint_count=u->endpoint_count;memcpy(e->desired.endpoints,u->endpoints,sizeof u->endpoints);}
        if(u->has_derp_region)e->desired.derp_region=u->derp_region;
        if(u->has_online){e->desired.has_online=true;e->desired.online=u->online;}
    }else{SUNLOCK();return false;} /* A delta without its full base needs refresh. */
    SUNLOCK();return true;
}
bool ml_peer_sync_resolve(ml_peer_update_t *u){
    if(!u||!u->has_node_id)return false;
    uint8_t key[32];if(ml_policy_peer_key(u->node_id,key)!=1)return false;
    static const uint8_t zero[32]={0};
    if(memcmp(u->public_key,zero,32)&&memcmp(u->public_key,key,32))return false;
    bool found=false;int action=u->action;
    SLOCK();if(sync_entries)for(int i=0;i<ML_MAX_PEERS;i++){
        const sync_entry *e=&sync_entries[i];
        if(e->used&&e->desired.node_id==u->node_id&&!memcmp(e->desired.public_key,key,32)){*u=e->desired;u->action=action;found=true;break;}
    }SUNLOCK();
    uint8_t current[32];return found&&ml_policy_peer_key(u->node_id,current)==1&&!memcmp(key,current,32);
}
