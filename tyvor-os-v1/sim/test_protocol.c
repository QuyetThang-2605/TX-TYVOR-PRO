#include <stdio.h>
#include <stdlib.h>
#include "test_transport.h"
#define SF (TYVOR_FRAME_MIN+TYVOR_STATUS_LEN)
#include <string.h>
#include "tyvor_protocol.h"
#include "tyvor_status.h"
#include "tyvor_bridge.h"
#include "tyvor_ui.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d CHECK failed: %s\n", __FILE__, __LINE__, #x); return 0; } } while (0)
static const tyvor_status_t mock={.arm_valid=0, .arm=0, .lq=99, .flags=7, .battery_mv=8100};
static uint8_t packet[TYVOR_FRAME_MAX];
static test_pair_t pair;
#define parser pair.b.parser
static tyvor_snapshot_t snapshot;
static tyvor_frame_t last;
static unsigned delivered;
static void receive(void *ctx, const tyvor_frame_t *f, uint64_t now) {
    (void)ctx; last=*f; ++delivered;
    tyvor_status_t s;
    if (f->type == TYVOR_STATUS && tyvor_status_unpack(&s, f->payload, f->length) == 0)
        (void)tyvor_snapshot_set(&snapshot, &s, now);
}
static size_t encode_test(uint8_t type,uint8_t seq,const uint8_t *b,uint8_t n,uint8_t *out,size_t cap) {
    const tyvor_session_tag_t tag={pair.a.local_id,pair.a.peer_id,pair.a.local_epoch,pair.a.peer_epoch};
    return tyvor_encode(type,seq,&tag,b,n,out,cap);
}
static size_t make(uint8_t seq) {
    uint8_t bytes[6];
    if (tyvor_status_pack(&mock, bytes, sizeof(bytes)) != 0) return 0;
    return encode_test(TYVOR_STATUS, seq, bytes, sizeof(bytes), packet, sizeof(packet));
}
static void reset(void) {
    if (test_pair_init(&pair,1,2)) {fputs("Fixture handshake failed\n",stderr);abort();}
    pair.b.receive=receive;pair.b.receive_context=NULL;
    memset(&parser.stats,0,sizeof(parser.stats));
    tyvor_snapshot_init(&snapshot); delivered=0;
    memset(&last, 0, sizeof(last)); (void)make(7);
}
static void feed(const uint8_t *b, size_t n, uint64_t now) {
    (void)tyvor_link_feed(&pair.b,b,n,now);
}
static void fix_crc(uint8_t *b, size_t n) {
    uint16_t c=tyvor_crc16(b+2, n-4); b[n-2]=(uint8_t)c; b[n-1]=(uint8_t)(c >> 8);
}
static int roundtrip(void) {
    tyvor_frame_t f; tyvor_status_t s;
    CHECK(make(7) == SF); CHECK(tyvor_decode(packet, SF, &f) == 0);
    CHECK(f.type == TYVOR_STATUS && f.sequence == 7 && f.length == 6 && f.version == TYVOR_VERSION);
    CHECK(f.session.sender_id==1 && f.session.receiver_id==2 && f.session.sender_epoch==1 && f.session.receiver_epoch==1);
    CHECK(tyvor_status_unpack(&s, f.payload, f.length) == 0);
    CHECK(s.arm_valid == 0 && s.arm == 0 && s.battery_mv == 8100 && s.lq == 99 && s.flags == 7);
    return 1;
}
static int golden_crc(void) {
    const uint8_t body[]={0x54,0x59,0x2,0x1,0x7,0x6,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x0,0x0,0xa4,0x1f,0x63,0x7,0xa9,0x21};
    CHECK(tyvor_crc16((const uint8_t *)"123456789",9)==0x29b1);
    CHECK(tyvor_crc16(NULL,0)==0xffff && tyvor_crc16(NULL,1)==0);
    CHECK(make(7)==sizeof(body));CHECK(!memcmp(packet,body,sizeof(body)));return 1;
}
static int crc_error(void) {
    tyvor_frame_t f; packet[TYVOR_HEADER_LEN]^=1;
    CHECK(tyvor_decode(packet, SF, &f) == TYVOR_ERR_CRC);
    feed(packet, SF, 0); CHECK(delivered == 0 && parser.stats.crc_errors == 1);
    return 1;
}
static int truncated(void) {
    tyvor_frame_t f;
    for (size_t n=0; n<SF; ++n) CHECK(tyvor_decode(packet, n, &f) != 0);
    feed(packet, SF-1, 0); CHECK(delivered == 0);
    return 1;
}
static int header_error(void) {
    tyvor_frame_t f;
    packet[0]=0; CHECK(tyvor_decode(packet, SF, &f) == TYVOR_ERR_FRAME);
    (void)make(7); packet[2]=TYVOR_VERSION+1; fix_crc(packet, SF);
    CHECK(tyvor_decode(packet, SF, &f) == TYVOR_ERR_FRAME);
    feed(packet, SF, 0); (void)make(7); feed(packet, SF, 1);
    CHECK(delivered == 1 && parser.stats.frame_errors > 0); return 1;
}
static int payload_oversize(void) {
    uint8_t bytes[TYVOR_PAYLOAD_MAX+1]={0}; tyvor_frame_t f;
    CHECK(encode_test(TYVOR_DIAGNOSTIC, 0, bytes, TYVOR_PAYLOAD_MAX+1, packet, sizeof(packet)) == 0);
    (void)make(7); packet[5]=TYVOR_PAYLOAD_MAX+1;
    CHECK(tyvor_decode(packet, SF, &f) == TYVOR_ERR_FRAME);
    feed(packet, 6, 0); (void)make(7); feed(packet, SF, 1);
    CHECK(delivered == 1 && parser.used == 0); return 1;
}
static int null_buffers(void) {
    tyvor_frame_t f; uint8_t bytes[6]={0}; tyvor_status_t s;
    CHECK(encode_test(TYVOR_STATUS, 0, bytes, 6, NULL, TYVOR_FRAME_MAX) == 0);
    CHECK(encode_test(TYVOR_STATUS, 0, NULL, 6, packet, TYVOR_FRAME_MAX) == 0);
    CHECK(tyvor_decode(NULL, SF, &f) == TYVOR_ERR_FRAME);
    CHECK(tyvor_decode(packet, SF, NULL) == TYVOR_ERR_FRAME);
    CHECK(tyvor_parser_feed(NULL, packet, SF, 0, NULL, NULL) == -1);
    CHECK(tyvor_parser_feed(&parser, NULL, 1, 0, NULL, NULL) == -1);
    CHECK(tyvor_parser_feed(&parser, NULL, 0, 0, NULL, NULL) == 0);
    CHECK(tyvor_status_pack(NULL, bytes, 6) == -1);
    CHECK(tyvor_status_pack(&mock, NULL, 6) == -1);
    CHECK(tyvor_status_unpack(NULL, bytes, 6) == -1);
    CHECK(tyvor_status_unpack(&s, NULL, 6) == -1);
    CHECK(tyvor_snapshot_set(NULL, &mock, 0) == -1);
    tyvor_parser_init(NULL, 0); tyvor_parser_poll(NULL, 0, NULL, NULL);
    tyvor_parser_transport_error(NULL, 0); return 1;
}
static int output_capacity(void) {
    uint8_t b[TYVOR_FRAME_MAX+2],bytes[TYVOR_PAYLOAD_MAX]={0};memset(b,0xcc,sizeof(b));
    for(size_t cap=0;cap<TYVOR_FRAME_MAX;++cap){
        CHECK(!encode_test(TYVOR_DIAGNOSTIC,0,bytes,TYVOR_PAYLOAD_MAX,b+1,cap));
        for(size_t i=0;i<sizeof(b);++i)CHECK(b[i]==0xcc);
    }
    CHECK(encode_test(TYVOR_DIAGNOSTIC,0,bytes,TYVOR_PAYLOAD_MAX,b+1,TYVOR_FRAME_MAX)==TYVOR_FRAME_MAX);
    CHECK(b[0]==0xcc && b[TYVOR_FRAME_MAX+1]==0xcc);
    CHECK(tyvor_status_pack(&mock,bytes,5)==-1);return 1;
}
static int payload_types(void) {
    tyvor_frame_t f;
    packet[3]=99; fix_crc(packet, SF);
    CHECK(tyvor_decode(packet, SF, &f) == TYVOR_ERR_FRAME);
    CHECK(encode_test(99, 0, NULL, 0, packet, TYVOR_FRAME_MAX) == 0);
    uint8_t req[]={TYVOR_REQ_PAGE, 6};
    CHECK(encode_test(TYVOR_UI_REQUEST, 0, req, 2, packet, TYVOR_FRAME_MAX) == TYVOR_FRAME_MIN+2);
    req[1]=7; CHECK(encode_test(TYVOR_UI_REQUEST, 0, req, 2, packet, TYVOR_FRAME_MAX) == 0);
    req[0]=99; CHECK(!tyvor_payload_valid(TYVOR_ACK, req, 2)); return 1;
}
static int semantic_status(void) {
    tyvor_status_t s=mock;
    s.arm_valid=2; CHECK(!tyvor_status_valid(&s));
    s=mock; s.arm=1; CHECK(!tyvor_status_valid(&s));
    s=mock; s.lq=101; CHECK(!tyvor_status_valid(&s));
    s=mock; s.flags=0x80; CHECK(!tyvor_status_valid(&s));
    s=mock; s.flags=0; CHECK(!tyvor_status_valid(&s));
    s=mock; s.battery_mv=0; CHECK(!tyvor_status_valid(&s));
    s=mock; s.arm_valid=1; CHECK(!tyvor_status_valid(&s));
    packet[TYVOR_HEADER_LEN+4]=101; fix_crc(packet, SF);
    tyvor_frame_t f; memset(&f, 0xaa, sizeof(f)); tyvor_frame_t before=f;
    CHECK(tyvor_decode(packet, SF, &f) == TYVOR_ERR_PAYLOAD);
    CHECK(memcmp(&f, &before, sizeof(f)) == 0);
    feed(packet, SF, 0); CHECK(delivered == 0 && parser.stats.semantic_errors == 1); return 1;
}
static int parser_timeout(void) {
    feed(packet, 6, 100); tyvor_parser_poll(&parser, 149, receive, NULL);
    CHECK(parser.used == 6 && parser.stats.timeouts == 0);
    tyvor_parser_poll(&parser, 150, receive, NULL);
    CHECK(parser.used == 0 && parser.stats.timeouts == 1);
    feed(packet, SF, 151); CHECK(delivered == 1); return 1;
}
static int uart_noise(void) {
    const uint8_t noise[]={0,0x54,0x54,0x00,0x59,0xff,0x54};
    feed(noise, sizeof(noise), 0); feed(packet, SF, 1);
    CHECK(delivered == 1 && parser.stats.noise_bytes >= 6); return 1;
}
static int payload_header(void) {
    uint8_t inner[TYVOR_FRAME_MIN],outer[TYVOR_FRAME_MAX];
    CHECK(encode_test(TYVOR_DIAGNOSTIC,7,NULL,0,inner,sizeof(inner))==sizeof(inner));
    CHECK(encode_test(TYVOR_DIAGNOSTIC,9,inner,sizeof(inner),outer,sizeof(outer))==sizeof(outer));
    feed(outer,sizeof(outer),0);CHECK(delivered==1 && last.length==sizeof(inner));
    CHECK(!memcmp(last.payload,inner,sizeof(inner)));return 1;
}
static int nested_recovery(void) {
    uint8_t inner[TYVOR_FRAME_MIN],outer[TYVOR_FRAME_MAX];
    CHECK(encode_test(TYVOR_DIAGNOSTIC,7,NULL,0,inner,sizeof(inner))==sizeof(inner));
    CHECK(encode_test(TYVOR_DIAGNOSTIC,9,inner,sizeof(inner),outer,sizeof(outer))==sizeof(outer));
    outer[sizeof(outer)-1]^=1;feed(outer,sizeof(outer),0);
    CHECK(delivered==1 && last.sequence==7 && last.length==0 && parser.stats.crc_errors==1);return 1;
}
static int false_length(void) {
    uint8_t outer[TYVOR_FRAME_MAX],b[TYVOR_PAYLOAD_MAX]={0};
    CHECK(encode_test(TYVOR_DIAGNOSTIC,1,b,sizeof(b),outer,sizeof(outer))==sizeof(outer));
    CHECK(encode_test(TYVOR_DIAGNOSTIC,7,NULL,0,packet,sizeof(packet))==TYVOR_FRAME_MIN);
    feed(outer,TYVOR_HEADER_LEN,0);feed(packet,TYVOR_FRAME_MIN,1);
    CHECK(delivered==0 && parser.used==TYVOR_HEADER_LEN+TYVOR_FRAME_MIN);
    tyvor_parser_poll(&parser,50,receive,NULL);
    CHECK(delivered==1 && parser.used==0 && parser.stats.timeouts==1);
    CHECK(encode_test(TYVOR_DIAGNOSTIC,8,NULL,0,packet,sizeof(packet))==TYVOR_FRAME_MIN);
    feed(packet,TYVOR_FRAME_MIN,51);CHECK(delivered==2 && parser.used==0);return 1;
}
static int consecutive(void) {
    uint8_t joined[2*SF]; memcpy(joined, packet, SF); (void)make(8); memcpy(joined+SF, packet, SF);
    feed(joined, sizeof(joined), 0); CHECK(delivered == 2 && parser.used == 0); return 1;
}
static int truncated_recovery(void) {
    for (size_t cut=1;cut<SF;++cut) {
        reset(); feed(packet,cut,0); feed(packet,SF,1);
        CHECK(delivered == 1 && parser.used == 0);
    }
    return 1;
}
static int absolute_deadline(void) {
    feed(packet,6,100); feed(packet+6,1,149);
    tyvor_parser_poll(&parser,150,receive,NULL);
    CHECK(parser.used == 0 && parser.stats.timeouts == 1 && delivered == 0);
    return 1;
}
static int sequence_half_range(void) {
    (void)make(0); feed(packet,SF,0); (void)make(128); feed(packet,SF,1);
    CHECK(delivered == 1 && parser.stats.sequence_errors == 1);
    (void)make(127); feed(packet,SF,2);
    CHECK(delivered == 2 && parser.stats.missing == 126 && parser.stats.sequence_errors == 2);
    return 1;
}
static int split_every_boundary(void) {
    for (size_t split=0; split<=SF; ++split) {
        reset(); feed(packet, split, 0); feed(packet+split, SF-split, 1);
        CHECK(delivered == 1 && parser.used == 0);
    }
    return 1;
}
static int duplicate_sequence(void) {
    feed(packet, SF, 1); feed(packet, SF, 1000);
    CHECK(delivered == 1 && parser.stats.duplicates == 1 && snapshot.received_ms == 1);
    CHECK(!tyvor_snapshot_poll(&snapshot, 2001, 2000)); return 1;
}
static int sequence_wrap(void) {
    const uint8_t sequences[]={254,255,0,1};
    for (size_t i=0; i<sizeof(sequences); ++i) { (void)make(sequences[i]); feed(packet,SF,i); }
    CHECK(delivered == 4 && !parser.stats.sequence_errors && !parser.stats.missing); return 1;
}
static int sequence_abnormal(void) {
    (void)make(10); feed(packet, SF, 0); (void)make(13); feed(packet, SF, 1);
    CHECK(delivered == 2 && parser.stats.missing == 2 && parser.stats.sequence_errors == 1);
    (void)make(12); feed(packet, SF, 2);
    CHECK(delivered == 2 && parser.stats.sequence_errors == 2 && snapshot.received_ms == 1); return 1;
}
static int independent_sequences(void) {
    feed(packet,SF,0);uint8_t diag[TYVOR_FRAME_MIN];
    CHECK(encode_test(TYVOR_DIAGNOSTIC,7,NULL,0,diag,sizeof(diag))==sizeof(diag));
    feed(diag,sizeof(diag),1);CHECK(delivered==2 && !parser.stats.duplicates);return 1;
}
static int stale_snapshot(void) {
    CHECK(!snapshot.valid && !snapshot.received_once);
    feed(packet,SF,0); CHECK(snapshot.valid && snapshot.received_once && snapshot.received_ms == 0);
    CHECK(tyvor_snapshot_poll(&snapshot,1999,2000));
    CHECK(!tyvor_snapshot_poll(&snapshot,2000,2000));
    CHECK(!snapshot.status.arm_valid && !snapshot.status.arm && !snapshot.status.battery_mv && !snapshot.status.lq && !snapshot.status.flags);
    CHECK(snapshot.received_once && snapshot.received_ms == 0); return 1;
}
static int clock_regression(void) {
    feed(packet,6,100); tyvor_parser_poll(&parser,99,receive,NULL);
    CHECK(parser.used == 0 && parser.stats.timeouts == 1);
    CHECK(tyvor_snapshot_set(&snapshot,&mock,100) == 0);
    CHECK(!tyvor_snapshot_poll(&snapshot,99,2000)); return 1;
}
static int transport_overflow(void) {
    feed(packet,6,0); tyvor_parser_transport_error(&parser,1);
    CHECK(parser.used == 0 && parser.stats.overflows == 1);
    feed(packet,SF,1); CHECK(delivered == 1);
    tyvor_parser_transport_error(&parser,0); feed(packet,SF,2);
    CHECK(delivered == 1 && parser.stats.duplicates == 1 && parser.stats.frame_errors == 1); return 1;
}
static int max_and_empty(void) {
    uint8_t data[TYVOR_PAYLOAD_MAX];memset(data,0x54,sizeof(data));tyvor_frame_t f;
    CHECK(encode_test(TYVOR_DIAGNOSTIC,0,data,sizeof(data),packet,sizeof(packet))==TYVOR_FRAME_MAX);
    CHECK(!tyvor_decode(packet,sizeof(packet),&f) && f.length==sizeof(data));feed(packet,sizeof(packet),0);
    CHECK(encode_test(TYVOR_DIAGNOSTIC,1,NULL,0,packet,sizeof(packet))==TYVOR_FRAME_MIN);
    feed(packet,TYVOR_FRAME_MIN,1);CHECK(delivered==2 && !parser.used);return 1;
}
static int crc_all_bits(void) {
    uint8_t original[SF]; memcpy(original,packet,SF); tyvor_frame_t f;
    for (size_t byte=2; byte<SF; ++byte) for (unsigned bit=0; bit<8; ++bit) {
        memcpy(packet,original,SF); packet[byte]^=(uint8_t)(1u << bit);
        CHECK(tyvor_decode(packet,SF,&f) != 0);
    }
    return 1;
}
static int bounded_noise_recovery(void) {
    uint32_t rng=1234567;
    for (unsigned i=0;i<10000;++i) {
        rng=rng*1664525u+1013904223u; uint8_t byte=(uint8_t)(rng >> 24);
        feed(&byte,1,i); CHECK(parser.used <= TYVOR_FRAME_MAX);
    }
    tyvor_parser_poll(&parser,10050,receive,NULL);
    (void)make(7); feed(packet,SF,10051);
    CHECK(delivered == 1 && parser.used == 0); return 1;
}
static int bridge_tx(void) {
    tyvor_bridge_t b;tyvor_link_t ui;test_wire_t ab={0},ba={0};
    CHECK(!tyvor_bridge_init(&b,test_write,&ab,10,0));
    CHECK(!tyvor_link_init(&ui,TYVOR_ROLE_UI,20,test_write,&ba,NULL,NULL,0));
    test_pump(&b.link,&ui,&ab,&ba,0);CHECK(b.link.ready && ui.ready);
    uint32_t before=b.link.stats.tx_frames;
    CHECK(!tyvor_bridge_publish(&b,&mock));test_deliver(&ab,&ui,1);
    CHECK(ui.snapshot.valid && ui.snapshot.status.battery_mv==8100 && b.link.stats.tx_frames==before+1);
    ab.mode=1;CHECK(tyvor_bridge_publish(&b,&mock)==-1 && b.link.stats.tx_errors==1);
    ab.mode=2;CHECK(tyvor_bridge_publish(&b,&mock)==-1 && b.link.stats.tx_errors==2 && b.link.stats.tx_partial==1);
    CHECK(tyvor_bridge_publish(NULL,&mock)!=0 && tyvor_bridge_publish(&b,NULL)!=0);
    CHECK(tyvor_bridge_init(NULL,NULL,NULL,10,0)==-1);return 1;
}
static int bridge_requests(void) {
    tyvor_bridge_t b;tyvor_link_t ui;test_wire_t ab={0},ba={0};uint8_t req[2]={TYVOR_REQ_PAGE,0};
    CHECK(!tyvor_bridge_init(&b,test_write,&ab,10,0));
    CHECK(!tyvor_link_init(&ui,TYVOR_ROLE_UI,20,test_write,&ba,NULL,NULL,0));
    test_pump(&b.link,&ui,&ab,&ba,0);CHECK(b.link.ready && ui.ready);
    for(unsigned i=0;i<3;++i){
        CHECK(!tyvor_link_send(&ui,TYVOR_UI_REQUEST,req,2));
        CHECK(ba.count==1);CHECK(!tyvor_bridge_receive(&b,ba.packet[0],4,i*50));
        CHECK(!tyvor_bridge_receive(&b,ba.packet[0]+4,ba.size[0]-4,i*50));ba.count=0;
    }
    CHECK(b.requests==3 && b.rate_limited==1 && ab.count==2);
    tyvor_frame_t f;CHECK(!tyvor_decode(ab.packet[1],ab.size[1],&f));
    CHECK(f.type==TYVOR_ACK && f.payload[1]==1);return 1;
}
static int ui_states(void) {
    tyvor_ui_view_t view;
    for (unsigned page=0;page<7;++page) CHECK(tyvor_ui_build(page,&snapshot,&view) == 0);
    CHECK(tyvor_ui_build(7,&snapshot,&view) == -1);
    CHECK(tyvor_ui_build(0,NULL,&view) == -1);
    CHECK(tyvor_ui_build(0,&snapshot,NULL) == -1);
    CHECK(tyvor_ui_build(0,&snapshot,&view) == 0 && strstr(view.banner,"NO DATA"));
    feed(packet,SF,0);
    CHECK(tyvor_ui_build(0,&snapshot,&view) == 0 && strstr(view.banner,"MOCK"));
    CHECK(strcmp(view.rows[2],"ARM: UNKNOWN") == 0 && strstr(view.rows[3],"8100"));
    (void)tyvor_snapshot_poll(&snapshot,2000,2000);
    CHECK(tyvor_ui_build(0,&snapshot,&view) == 0 && strstr(view.banner,"STALE"));
    CHECK(strcmp(view.rows[2],"ARM: UNKNOWN") == 0 && strcmp(view.rows[3],"Battery: UNKNOWN") == 0);
    return 1;
}
struct test { const char *name; int (*run)(void); };
#define TEST(name) {#name,name}
static const struct test tests[]={
    TEST(roundtrip),TEST(golden_crc),TEST(crc_error),TEST(truncated),TEST(header_error),
    TEST(payload_oversize),TEST(null_buffers),TEST(output_capacity),TEST(payload_types),
    TEST(semantic_status),TEST(parser_timeout),TEST(uart_noise),TEST(payload_header),
    TEST(nested_recovery),TEST(false_length),TEST(consecutive),TEST(truncated_recovery),
    TEST(absolute_deadline),TEST(sequence_half_range),TEST(split_every_boundary),
    TEST(duplicate_sequence),TEST(sequence_wrap),TEST(sequence_abnormal),TEST(independent_sequences),
    TEST(stale_snapshot),TEST(clock_regression),TEST(transport_overflow),TEST(max_and_empty),
    TEST(crc_all_bits),TEST(bounded_noise_recovery),TEST(bridge_tx),TEST(bridge_requests),TEST(ui_states)
};
int main(int argc,char **argv) {
    unsigned passed=0,failed=0;
    for (size_t i=0;i<sizeof(tests)/sizeof(tests[0]);++i) {
        if (argc == 2 && strcmp(argv[1],tests[i].name) != 0) continue;
        reset(); int ok=tests[i].run();
        printf("%s %s\n",ok ? "PASS" : "FAIL",tests[i].name);
        if (ok) ++passed; else ++failed;
    }
    printf("%u PASS / %u FAIL; NDEBUG=%s\n",passed,failed,
#ifdef NDEBUG
        "ON"
#else
        "OFF"
#endif
    );
    return failed || !passed ? 1 : 0;
}
