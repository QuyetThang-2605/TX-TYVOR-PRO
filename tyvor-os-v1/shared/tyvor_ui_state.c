#include "tyvor_ui_state.h"
#include <string.h>
#include <stdio.h>
static void invalidate(tyvor_ui_state_t *s) {
    memset(&s->snapshot,0,sizeof(s->snapshot));s->valid=0;s->ack_valid=0;s->mix_valid=0;s->mix_mask=0;
}
void tyvor_ui_state_init(tyvor_ui_state_t *s) {if(s)memset(s,0,sizeof(*s));}
void tyvor_ui_state_sync(tyvor_ui_state_t *s,const tyvor_link_t *l,uint64_t now) {
    if(!s || !l)return;
    if(!l->ready || !s->ready || s->local_id!=l->local_id || s->peer_id!=l->peer_id ||
        s->local_epoch!=l->local_epoch || s->peer_epoch!=l->peer_epoch) {
        invalidate(s);s->parts=0;s->generation_seen=0;
    }
    s->ready=l->ready;s->local_id=l->local_id;s->peer_id=l->peer_id;
    s->local_epoch=l->local_epoch;s->peer_epoch=l->peer_epoch;
    s->disconnected=(uint8_t)(!l->ready || (now>=l->last_peer_ms && now-l->last_peer_ms>=TYVOR_SESSION_IDLE_MS));
    tyvor_ui_state_poll(s,now);
}
static void expire_telemetry(tyvor_telemetry_view_t *v,unsigned i) {
    v->valid&=(uint8_t)~(1u<<i);v->age_ms[i]=UINT16_MAX;
    if(i==0)v->battery_mv=0;
    else if(i==1)v->lq=0;
    else if(i==2)v->rssi_dbm=0;
    else {v->latitude_e7=0;v->longitude_e7=0;v->satellites=0;}
}
void tyvor_ui_state_poll(tyvor_ui_state_t *s,uint64_t now) {
    if(!s)return;
    if(now<s->last_poll_ms) {invalidate(s);s->parts=0;s->generation_seen=0;}
    s->last_poll_ms=now;
    if(s->parts && (now<s->pending_started_ms || now-s->pending_started_ms>=TYVOR_UI_PARTIAL_MS)) {
        s->parts=0;++s->partial_timeouts;
    }
    if(s->valid && now>=s->expires_ms)invalidate(s);
    if(s->mix_valid && now>=s->mix_expires_ms)s->mix_valid=0;
    if(s->ack_valid && (now<s->last_ack_ms || now-s->last_ack_ms>=TYVOR_UI_CORE_STALE_MS))s->ack_valid=0;
    if(!s->valid)return;
    if(s->snapshot.input_valid && now>=s->input_expires_ms) {
        s->snapshot.input_valid=0;memset(s->snapshot.inputs,0,sizeof(s->snapshot.inputs));memset(s->snapshot.switches,0,sizeof(s->snapshot.switches));
        s->snapshot.outputs_valid=0;memset(s->snapshot.channels,0,sizeof(s->snapshot.channels));
    }
    for(unsigned i=0;i<4;++i)if((s->snapshot.telemetry.valid&(1u<<i)) && now>=s->telemetry_expires_ms[i])expire_telemetry(&s->snapshot.telemetry,i);
}
static uint64_t deadline(uint64_t now,uint32_t age,uint32_t ttl) {
    return age>=ttl?now:now+(ttl-age);
}
void tyvor_ui_state_receive(tyvor_ui_state_t *s,const tyvor_link_t *l,const tyvor_frame_t *f,uint64_t now) {
    if(!s || !l || !f)return;
    /* Parser recovery can deliver a frame whose last byte arrived before the
       latest owner poll. Keep its age, without inventing clock regression. */
    uint64_t owner_now=now<s->last_poll_ms?s->last_poll_ms:now;
    tyvor_ui_state_sync(s,l,owner_now);
    if(!s->ready)return;
    if(f->version!=TYVOR_VERSION || f->session.sender_id!=l->peer_id || f->session.receiver_id!=l->local_id ||
        f->session.sender_epoch!=l->peer_epoch || f->session.receiver_epoch!=l->local_epoch ||
        !tyvor_payload_valid(f->type,f->payload,f->length)) {++s->rejected;return;}
    if(f->type==TYVOR_ACK) {s->ack_valid=1;s->ack_opcode=f->payload[0];s->ack_result=f->payload[1];s->last_ack_ms=now;return;}
    if(f->type!=TYVOR_DIAGNOSTIC || !f->length || f->payload[0]!=TYVOR_CORE_MAGIC)return;
    tyvor_core_snapshot_t candidate=s->pending;unsigned kind;uint16_t capture_age;
    if(tyvor_core_wire_decode(&candidate,f->payload,f->length,&kind,&capture_age)) {++s->rejected;return;}
    if(kind==TYVOR_CORE_MIXES) {
        if(!s->valid || candidate.generation!=s->snapshot.generation) {++s->rejected;return;}
        if(s->mix_generation!=candidate.generation) {s->mix_generation=candidate.generation;s->mix_mask=0;s->mix_valid=0;s->mix_count=candidate.mix_count;}
        uint8_t mix_bit=(uint8_t)(1u<<candidate.mix_page);
        if(s->mix_count!=candidate.mix_count || (s->mix_mask&mix_bit)) {++s->rejected;return;}
        for(unsigned row=0;row<2;++row) {unsigned index=2u*candidate.mix_page+row;if(index<candidate.mix_count)s->mixes[index]=candidate.mixes[index];}
        s->mix_mask|=mix_bit;
        if(s->mix_mask==(uint8_t)((1u<<((s->mix_count+1u)/2u))-1u)) {s->mix_valid=1;s->mix_expires_ms=s->expires_ms;}
        return;
    }
    uint32_t baseline=s->generation;
    if(s->generation_seen) {
        uint32_t delta=candidate.generation-baseline;
        if(delta>=UINT32_C(0x80000000) || (!delta && !s->parts)) {++s->rejected;return;}
    }
    uint8_t bit=(uint8_t)(1u<<(kind-1));
    if(s->parts && candidate.generation==s->pending.generation && (s->parts&bit)) {++s->rejected;return;}
    if(!s->parts || candidate.generation!=s->pending.generation) {
        memset(&s->pending,0,sizeof(s->pending));s->parts=0;s->pending_started_ms=now;
        s->pending_expires_ms=now+TYVOR_UI_CORE_STALE_MS;
        /* Decode again into a clean generation; never mix old parts. */
        if(tyvor_core_wire_decode(&s->pending,f->payload,f->length,&kind,&capture_age))return;
    } else s->pending=candidate;
    s->generation_seen=1;s->generation=candidate.generation;
    uint64_t expires=deadline(now,capture_age,TYVOR_UI_CORE_STALE_MS);
    if(expires<s->pending_expires_ms)s->pending_expires_ms=expires;
    if(kind==TYVOR_CORE_MODEL && s->valid) {
        s->snapshot.model_id=s->pending.model_id;
        memcpy(s->snapshot.model_name,s->pending.model_name,sizeof(s->snapshot.model_name));
    }
    if(kind==TYVOR_CORE_INPUTS) {
        s->pending_input_expires_ms=deadline(now,(uint32_t)capture_age+s->pending.input_age_ms,TYVOR_UI_CORE_STALE_MS);
        /* Zero-latency fast path: apply fresh gimbal inputs to active snapshot immediately */
        if(s->valid && s->pending.input_valid) {
            memcpy(s->snapshot.inputs,s->pending.inputs,sizeof(s->snapshot.inputs));
            memcpy(s->snapshot.switches,s->pending.switches,sizeof(s->snapshot.switches));
            s->snapshot.input_valid=1;
            s->input_expires_ms=s->pending_input_expires_ms;
        }
    }
    if(kind==TYVOR_CORE_OUTPUTS && s->valid) {
        memcpy(s->snapshot.channels,s->pending.channels,sizeof(s->snapshot.channels));
        s->snapshot.outputs_valid=s->pending.outputs_valid;
    }
    if(kind==TYVOR_CORE_TELEMETRY)for(unsigned i=0;i<4;++i)
        s->pending_telemetry_expires_ms[i]=deadline(now,(uint32_t)capture_age+s->pending.telemetry.age_ms[i],tyvor_telemetry_ttl(i));
    s->parts|=bit;
    if(s->parts==31) {
        if(now<s->pending_expires_ms) {
            s->snapshot=s->pending;s->snapshot.captured_ms=now;s->expires_ms=s->pending_expires_ms;
            s->input_expires_ms=s->pending_input_expires_ms;
            memcpy(s->telemetry_expires_ms,s->pending_telemetry_expires_ms,sizeof(s->telemetry_expires_ms));
            s->valid=1;++s->commits;
            if(s->mix_generation!=s->snapshot.generation) {s->mix_valid=0;s->mix_mask=0;}
        }
        s->parts=0;tyvor_ui_state_poll(s,owner_now);
    }
}
static const char *safety_name(unsigned s) {
    static const char *names[]={"STARTUP","DISARMED","BENCH ARMED","FAILSAFE"};return s<4?names[s]:"UNKNOWN";
}
int tyvor_ui_core_view(unsigned page,const tyvor_ui_state_t *s,tyvor_ui_view_t *v) {
    static const char *titles[]={"HOME","MODEL","INPUTS","MIXES","OUTPUTS","TELEMETRY","SETTINGS"};
    if(!s || !v || page>=7)return -1;
    memset(v,0,sizeof(*v));v->title=titles[page];
    const tyvor_core_snapshot_t *c=&s->snapshot;
    const char *quality=!s->ready || s->disconnected?"DISCONNECTED":!s->valid?"STALE / INVALID":
        (c->safety_reasons&TYVOR_SAFE_MODEL)?"INVALID MODEL / SAFE":
        c->safety_state!=TYVOR_BENCH_ARMED?"VALID / SAFE":"VALID / BENCH ARMED";
    (void)snprintf(v->banner,sizeof(v->banner),"BENCH %s",quality);
    for(unsigned i=0;i<TYVOR_UI_ROWS;++i)(void)snprintf(v->rows[i],sizeof(v->rows[i]),"UNKNOWN");
    if(page==6) {
        (void)snprintf(v->rows[0],sizeof(v->rows[0]),"v2 session: %s",s->ready && !s->disconnected?"READY":"DISCONNECTED");
        (void)snprintf(v->rows[1],sizeof(v->rows[1]),"Epochs: %lu / %lu",(unsigned long)s->local_epoch,(unsigned long)s->peer_epoch);
        (void)snprintf(v->rows[2],sizeof(v->rows[2]),"Core cache: %s",s->valid?"VALID":"STALE / INVALID");
        (void)snprintf(v->rows[3],sizeof(v->rows[3]),"Commits %lu rejected %lu",(unsigned long)s->commits,(unsigned long)s->rejected);
        if(s->valid)(void)snprintf(v->rows[4],sizeof(v->rows[4]),"Safety %s lock=%u",safety_name(c->safety_state),c->throttle_lock);
        (void)snprintf(v->rows[5],sizeof(v->rows[5]),"Read only / RF OFF / pins UNVERIFIED");return 0;
    }
    if(!s->valid)return 0;
    switch(page) {
    case 0:
        (void)snprintf(v->rows[0],sizeof(v->rows[0]),"Model: %s",c->model_name);
        (void)snprintf(v->rows[1],sizeof(v->rows[1]),"Logic: %s",safety_name(c->safety_state));
        (void)snprintf(v->rows[2],sizeof(v->rows[2]),"Input: %s",c->input_valid?"VALID":"STALE / INVALID");
        (void)snprintf(v->rows[3],sizeof(v->rows[3]),"Tick: %lu lock=%u",(unsigned long)c->ticks,c->throttle_lock);
        (void)snprintf(v->rows[4],sizeof(v->rows[4]),"RF OFF / memory outputs only");
        (void)snprintf(v->rows[5],sizeof(v->rows[5]),"Aircraft ARM: UNKNOWN");break;
    case 1:
        (void)snprintf(v->rows[0],sizeof(v->rows[0]),"%s",c->model_name);
        (void)snprintf(v->rows[1],sizeof(v->rows[1]),"ID %lu revision %lu",(unsigned long)c->model_id,(unsigned long)c->model_revision);
        (void)snprintf(v->rows[2],sizeof(v->rows[2]),"Model: %s",(c->safety_reasons&TYVOR_SAFE_MODEL)?"INVALID MODEL":"VALID");
        (void)snprintf(v->rows[3],sizeof(v->rows[3]),"STM32 authority / local selection");break;
    case 2:
        if(c->input_valid) {
            static const char *names[]={"Roll","Pitch","Throttle","Yaw"};
            for(unsigned i=0;i<4;++i)(void)snprintf(v->rows[i],sizeof(v->rows[i]),"%s: %d / 1000",names[i],c->inputs[i]);
            (void)snprintf(v->rows[4],sizeof(v->rows[4]),"ARM switch: %d",c->switches[0]);
            (void)snprintf(v->rows[5],sizeof(v->rows[5]),"Mode/kill switch: %d",c->switches[1]);
        }break;
    case 3:
        if(!(c->safety_reasons&TYVOR_SAFE_MODEL) && s->mix_valid && s->mix_generation==c->generation) {
            (void)snprintf(v->rows[0],sizeof(v->rows[0]),"VALID / %u terms / B=logical bus",s->mix_count);
            for(unsigned row=0;row<5;++row) {
                unsigned i=s->mix_scroll+row;if(i>=s->mix_count)break;
                const tyvor_mix_view_t *x=&s->mixes[i];
                (void)snprintf(v->rows[row+1],sizeof(v->rows[0]),"%u I%u>B%u %s W%d O%d T%d",i+1,x->source+1,x->destination+1,
                    x->operation?"REP":"ADD",x->weight,x->offset,x->trim);
            }
        }break;
    case 4:
        if(c->outputs_valid)for(unsigned i=0;i<4;++i)(void)snprintf(v->rows[i],sizeof(v->rows[i]),"CH%u %d   CH%u %d",2*i+1,c->channels[2*i],2*i+2,c->channels[2*i+1]);
        (void)snprintf(v->rows[4],sizeof(v->rows[4]),"%s / RF OFF",c->outputs_valid?"VALID memory channels":"STALE / INVALID channels");break;
    case 5: {
        const tyvor_telemetry_view_t *t=&c->telemetry;
        (void)snprintf(v->rows[0],sizeof(v->rows[0]),"Battery: STALE / INVALID");
        (void)snprintf(v->rows[1],sizeof(v->rows[1]),"LQ: STALE / INVALID");
        (void)snprintf(v->rows[2],sizeof(v->rows[2]),"RSSI: STALE / INVALID");
        (void)snprintf(v->rows[3],sizeof(v->rows[3]),"GPS: STALE / INVALID");
        if(t->valid&1)(void)snprintf(v->rows[0],sizeof(v->rows[0]),"Battery VALID: %u mV",t->battery_mv);
        if(t->valid&2)(void)snprintf(v->rows[1],sizeof(v->rows[1]),"LQ VALID: %u%%",t->lq);
        if(t->valid&4)(void)snprintf(v->rows[2],sizeof(v->rows[2]),"RSSI VALID: %d dBm",t->rssi_dbm);
        if(t->valid&8) {
            (void)snprintf(v->rows[3],sizeof(v->rows[3]),"Lat E7: %ld",(long)t->latitude_e7);
            (void)snprintf(v->rows[4],sizeof(v->rows[4]),"Lon E7: %ld sats=%u",(long)t->longitude_e7,t->satellites);
        }
        (void)snprintf(v->rows[5],sizeof(v->rows[5]),"MOCK / RTT bounded freshness");break;
    }
    default:break;
    }
    return 0;
}
