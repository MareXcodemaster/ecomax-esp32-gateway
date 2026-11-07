/* Owned plaintext reassembly before RX policy; successful-TX observation.
 * SPDX-License-Identifier: MIT. Two slots/direction, lazy PSRAM, no retained
 * pbufs/timers/sockets/waiting locks. Final RX policy remains mandatory. */
#include "ml_reassembly.h"
#include "ml_policy.h"
#include <stdlib.h>
#include <string.h>
#include "lwip/pbuf.h"
#ifdef ML_REASSEMBLY_HOST_TEST
extern void *reassembly_test_alloc(size_t);
extern uint32_t policy_test_now(void);
#define RALLOC(n) reassembly_test_alloc(n)
#define RNOW() policy_test_now()
#else
#include "esp_heap_caps.h"
#include "esp_timer.h"
#define RALLOC(n) heap_caps_malloc((n),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)
#define RNOW() ((uint32_t)(esp_timer_get_time()/1000))
#endif
#define RMAX 4096
#define RHEADER 296
#define RSLOTS 2
#define RAGE 5000
#define RFRAGMENTS 32
typedef struct {
    uint8_t header[RHEADER],src[16],dst[16];
    unsigned family,protocol,off,hlen,start,total;
    uint32_t id,epoch;
    bool more;
} fragment_view;
typedef struct {
    bool used,first,last;
    uint8_t family,protocol,key[32],src[16],dst[16],header[RHEADER];
    uint32_t id,epoch,began;
    unsigned hlen,total,received,count,high;
    uint8_t payload[RMAX],seen[RMAX/8];
} reassembly_slot;
static reassembly_slot *rpools[2];
static unsigned rbusy;
static unsigned r16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static uint16_t rchecksum(const uint8_t *p,unsigned n){
    unsigned sum=0;for(unsigned i=0;i+1<n;i+=2)sum+=r16(p+i);
    if(n&1)sum+=(unsigned)p[n-1]<<8;
    while(sum>>16)sum=(sum&65535)+(sum>>16);
    return (uint16_t)~sum;
}
/* Inspect only bounded unfragmentable headers. A normal packet is returned
 * unchanged for the ordinary policy decoder, never "allowed" by this parser. */
static int fragment_parse(const struct pbuf *packet,fragment_view *v){
    memset(v,0,sizeof *v);uint8_t *h=v->header;
    if(!packet||pbuf_copy_partial(packet,h,20,0)!=20)return 0;
    v->family=h[0]>>4;
    if(v->family==4){
        unsigned flags=r16(h+6);if(!(flags&0x3fff))return 0;
        if(h[0]!=0x45||(flags&0xc000)||rchecksum(h,20))return -1;
        v->total=r16(h+2);v->off=v->hlen=20;v->start=(flags&0x1fff)*8;v->more=(flags&0x2000)!=0;
        v->protocol=h[9];v->id=r16(h+4);memcpy(v->src,h+12,4);memcpy(v->dst,h+16,4);
        h[2]=h[3]=h[6]=h[7]=h[10]=h[11]=0;
    }else if(v->family==6){
        if(pbuf_copy_partial(packet,h,40,0)!=40)return -1;
        v->total=40+r16(h+4);unsigned next=h[6],off=40,previous=6,walked=0;
        while(next==0||next==60){
            uint8_t ext[2];
            if(++walked>3||off>288||off+8>v->total||pbuf_copy_partial(packet,ext,2,off)!=2)return -1;
            unsigned n=((unsigned)ext[1]+1)*8;
            if(n>256||off+n>RHEADER||off+n>v->total||pbuf_copy_partial(packet,h+off,(uint16_t)n,(uint16_t)off)!=n)return -1;
            if(next==0&&off!=40)return -1;
            for(unsigned i=2;i<n;){
                unsigned option=h[off+i++];if(!option)continue;
                if(i>=n)return -1;
                unsigned size=h[off+i++];
                if(i+size>n||(option!=1&&!(next==0&&option==5&&size==2)))return -1;
                i+=size;
            }
            next=ext[0];previous=off;off+=n;
        }
        if(next!=44)return 0;
        uint8_t frag[8];
        if(off+8>v->total||pbuf_copy_partial(packet,frag,8,(uint16_t)off)!=8)return -1;
        unsigned flags=r16(frag+2);
        if(!flags&&!frag[1])return 0; /* Atomic, normal decoder validates. */
        if(frag[1]||(flags&6)||!r16(h+4))return -1;
        v->off=off+8;v->hlen=off;v->start=flags&0xfff8;v->more=(flags&1)!=0;
        v->protocol=frag[0];v->id=((uint32_t)frag[4]<<24)|((uint32_t)frag[5]<<16)|((uint32_t)frag[6]<<8)|frag[7];
        memcpy(v->src,h+8,16);memcpy(v->dst,h+24,16);
        h[4]=h[5]=0;h[previous]=(uint8_t)v->protocol; /* Remove the fragment header. */
    }else return 0;
    if(v->total>packet->tot_len||v->total<=v->off||v->hlen>=RMAX)return -1;
    unsigned n=v->total-v->off;
    if(n>RMAX-v->hlen||v->start>RMAX-v->hlen-n||(v->more&&(n&7)))return -1;
    if(v->protocol!=6&&v->protocol!=17&&v->protocol!=(v->family==4?1:58)
       &&!(v->family==6&&v->protocol==60))return -1;
    return 1;
}
static bool fragment_authority(fragment_view *v,const uint8_t key[32],bool tx){
    return tx?ml_policy_tx_fragment_epoch(v->src,v->dst,v->family,key,&v->epoch)
             :ml_policy_fragment_epoch(v->src,v->dst,v->family,key,&v->epoch);
}
void ml_reassembly_clear(void){
    if(__atomic_exchange_n(&rbusy,1,__ATOMIC_ACQUIRE))return;
    for(unsigned d=0;d<2;d++)if(rpools[d])memset(rpools[d],0,RSLOTS*sizeof *rpools[d]);
    __atomic_store_n(&rbusy,0,__ATOMIC_RELEASE);
}
/* Caller keeps its pbuf. Every retained byte is copied into owned PSRAM.
 * RX and TX have separate bounded capacity. A failed TX discards its batch. */
static struct pbuf *fragment_collect(const struct pbuf *packet,fragment_view *v,const uint8_t key[32],bool tx,bool success){
    if(!fragment_authority(v,key,tx))return NULL;
    if(__atomic_exchange_n(&rbusy,1,__ATOMIC_ACQUIRE))return NULL;
    reassembly_slot **pool=&rpools[tx?1:0];
    if(!*pool){
        if(!success)goto unlock;
        *pool=RALLOC(RSLOTS*sizeof **pool);if(!*pool)goto unlock;
        memset(*pool,0,RSLOTS*sizeof **pool);
    }
    uint32_t now=RNOW();int slot=-1,empty=-1;
    for(unsigned i=0;i<RSLOTS;i++){
        reassembly_slot *s=&(*pool)[i];
        if(s->used&&(now-s->began>=RAGE||s->epoch!=v->epoch))memset(s,0,sizeof *s);
        if(!s->used){if(empty<0)empty=(int)i;continue;}
        if(s->id==v->id&&s->family==v->family&&!memcmp(s->key,key,32)&&!memcmp(s->src,v->src,16)&&!memcmp(s->dst,v->dst,16))slot=(int)i;
    }
    if(!success){if(slot>=0)memset(&(*pool)[slot],0,sizeof **pool);goto unlock;}
    if(slot<0){
        if(empty<0)goto unlock;
        slot=empty;reassembly_slot *s=&(*pool)[slot];s->used=true;s->family=(uint8_t)v->family;s->protocol=(uint8_t)v->protocol;
        s->id=v->id;s->epoch=v->epoch;s->began=now;s->hlen=v->hlen;
        memcpy(s->src,v->src,16);memcpy(s->dst,v->dst,16);memcpy(s->key,key,32);memcpy(s->header,v->header,v->hlen);
    }
    reassembly_slot *s=&(*pool)[slot];unsigned length=v->total-v->off,end=v->start+length;
    if(s->protocol!=v->protocol||s->hlen!=v->hlen||memcmp(s->header,v->header,v->hlen)||s->count>=RFRAGMENTS
       ||(s->last&&(end>s->total||(!v->more&&end!=s->total)))||(!v->more&&s->high>end))goto discard;
    for(unsigned i=v->start;i<end;i++)if(s->seen[i/8]&(1u<<(i%8)))goto discard;
    if(pbuf_copy_partial(packet,s->payload+v->start,(uint16_t)length,(uint16_t)v->off)!=length)goto discard;
    for(unsigned i=v->start;i<end;i++)s->seen[i/8]|=(uint8_t)(1u<<(i%8));
    s->count++;s->received+=length;if(end>s->high)s->high=end;
    if(!v->start)s->first=true;
    if(!v->more){s->last=true;s->total=end;}
    if(s->first&&s->last&&s->received==s->total){
        struct pbuf *result=pbuf_alloc(PBUF_RAW,(uint16_t)(s->hlen+s->total),PBUF_RAM);
        if(!result)goto discard;
        if(v->family==4){unsigned n=s->hlen+s->total;s->header[2]=(uint8_t)(n>>8);s->header[3]=(uint8_t)n;
            uint16_t checksum=rchecksum(s->header,20);s->header[10]=(uint8_t)(checksum>>8);s->header[11]=(uint8_t)checksum;
        }else{unsigned payload=s->hlen-40+s->total;s->header[4]=(uint8_t)(payload>>8);s->header[5]=(uint8_t)payload;}
        memcpy(result->payload,s->header,s->hlen);memcpy((uint8_t*)result->payload+s->hlen,s->payload,s->total);
        uint32_t old_epoch=v->epoch;bool current=fragment_authority(v,key,tx)&&v->epoch==old_epoch;
        memset(s,0,sizeof *s);__atomic_store_n(&rbusy,0,__ATOMIC_RELEASE);
        if(!current){pbuf_free(result);return NULL;}return result;
    }
    goto unlock;
discard:
    memset(s,0,sizeof *s);
unlock:
    __atomic_store_n(&rbusy,0,__ATOMIC_RELEASE);return NULL;
}
struct pbuf *ml_reassemble_rx(struct pbuf *packet,const uint8_t key[32]){
    fragment_view v;int kind=fragment_parse(packet,&v);
    if(!kind)return packet;
    struct pbuf *result=kind>0?fragment_collect(packet,&v,key,false,true):NULL;
    if(packet)pbuf_free(packet);
    return result;
}
int ml_fragment_tx_check(const struct pbuf *packet,const uint8_t key[32]){
    fragment_view v;int kind=fragment_parse(packet,&v);
    if(kind<=0)return kind;
    /* Fragmented ICMP errors cannot bypass related-error validation. */
    return (v.protocol==6||v.protocol==17)&&fragment_authority(&v,key,true)?1:-1;
}
void ml_fragment_tx_result(const struct pbuf *packet,const uint8_t key[32],bool success){
    fragment_view v;
    if(fragment_parse(packet,&v)!=1||(v.protocol!=6&&v.protocol!=17))return;
    struct pbuf *complete=fragment_collect(packet,&v,key,true,success);
    if(complete){ml_policy_sent_epoch(complete,key,v.epoch);pbuf_free(complete);}
}
