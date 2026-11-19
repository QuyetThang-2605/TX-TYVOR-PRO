#ifndef TYVOR_PROTOCOL_H
#define TYVOR_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#define TYVOR_SYNC0 0x54u
#define TYVOR_SYNC1 0x59u
#define TYVOR_VERSION 2u
#define TYVOR_PAYLOAD_MAX 32u
#define TYVOR_HEADER_LEN 30u
#define TYVOR_FRAME_MIN (TYVOR_HEADER_LEN + 2u)
#define TYVOR_FRAME_MAX (TYVOR_FRAME_MIN + TYVOR_PAYLOAD_MAX)
#define TYVOR_FRAME_TIMEOUT_MS 50u
#define TYVOR_STALE_MS 2000u
enum { TYVOR_STATUS=1, TYVOR_UI_REQUEST=2, TYVOR_ACK=3, TYVOR_DIAGNOSTIC=4,
       TYVOR_HELLO=5, TYVOR_SESSION_ACK=6 };
enum { TYVOR_REQ_MODEL_INFO=1, TYVOR_REQ_PAGE=2, TYVOR_REQ_QUICKSET=3 };
/* TYVOR_REQ_QUICKSET arg encoding (1 byte):
 *   0x00..0x04 : Set TX Power index (100/250/400/500/1000 mW)
 *   0x10..0x13 : Set ELRS Packet Rate index (50/150/250/500 Hz)
 *   0x20/0x21  : Finder Beep OFF / ON
 *   0x30/0x31  : RF Output OFF / ON
 *   0x40/0x41  : Bind Mode STOP / START
 *   0x50..0x53 : ELRS Telem Ratio (1:2 / 1:16 / 1:64 / 1:128)
 *   0x60/0x61  : ELRS Model Match OFF / ON
 *   0x70/0x71  : RF Module Internal TX / External TX
 *   0x80/0x81  : Safety Throttle Lock OFF / ON
 *   0x90/0x91  : Stick Mode 1 / Mode 2
 */
enum { TYVOR_ROLE_CONTROL=1, TYVOR_ROLE_UI=2 };
enum { TYVOR_OK=0, TYVOR_ERR_FRAME=-1, TYVOR_ERR_CRC=-2,
       TYVOR_ERR_PAYLOAD=-3, TYVOR_ERR_SESSION=-4 };
typedef struct {
    uint64_t sender_id, receiver_id;
    uint32_t sender_epoch, receiver_epoch;
} tyvor_session_tag_t;
typedef struct {
    uint8_t version, type, sequence, length, payload[TYVOR_PAYLOAD_MAX];
    tyvor_session_tag_t session;
} tyvor_frame_t;
uint16_t tyvor_crc16(const uint8_t *p,size_t n);
int tyvor_payload_valid(uint8_t type,const uint8_t *payload,size_t length);
size_t tyvor_encode(uint8_t type,uint8_t sequence,const tyvor_session_tag_t *session,
                    const uint8_t *payload,uint8_t length,uint8_t *out,size_t cap);
int tyvor_decode(const uint8_t *in,size_t n,tyvor_frame_t *out);
typedef struct {
    uint32_t frames,crc_errors,frame_errors,semantic_errors,timeouts;
    uint32_t overflows,noise_bytes,duplicates,sequence_errors,missing;
} tyvor_protocol_stats_t;
typedef struct {
    uint8_t raw[TYVOR_FRAME_MAX];
    uint64_t arrival_ms[TYVOR_FRAME_MAX];
    size_t used;
    uint32_t timeout_ms;
    tyvor_protocol_stats_t stats;
} tyvor_parser_t;
/* Parser owns framing only. Session/sequence acceptance belongs to tyvor_link.
   Single owner, no reentry. Callback borrows frame until return.
   Callback time = local arrival time of the last byte, not delayed poll time. */
typedef void (*tyvor_frame_fn)(void *,const tyvor_frame_t *,uint64_t);
void tyvor_parser_init(tyvor_parser_t *p,uint32_t timeout_ms);
int tyvor_parser_feed(tyvor_parser_t *p,const uint8_t *bytes,size_t n,uint64_t now_ms,
                      tyvor_frame_fn callback,void *context);
void tyvor_parser_poll(tyvor_parser_t *p,uint64_t now_ms,tyvor_frame_fn callback,void *context);
void tyvor_parser_transport_error(tyvor_parser_t *p,int overflow);
#endif
