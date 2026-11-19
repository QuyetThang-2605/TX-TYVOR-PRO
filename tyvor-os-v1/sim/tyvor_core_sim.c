#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "tyvor_diagnostics.h"
static const char *state_name(unsigned value) {
    static const char *names[]={"STARTUP","DISARMED","BENCH_ARMED","FAILSAFE"};return value<4?names[value]:"UNKNOWN";
}
static int number(const char *s,long minimum,long maximum,long *v) {
    char *end;errno=0;long value=strtol(s,&end,10);
    if(errno || !*s || *end || value<minimum || value>maximum)return -1;
    *v=value;return 0;
}
static int axis_index(const char *s) {
    static const char *names[]={"roll","pitch","throttle","yaw"};
    for(unsigned i=0;i<4;++i)if(!strcmp(s,names[i]))return (int)i;
    return -1;
}
static void status(const tyvor_sim_t *s) {
    printf("BENCH time=%llu tick=%lu model=%lu safety=%s input=%u RF=DISABLED ui=%u session=%u\n",
        (unsigned long long)s->now,(unsigned long)s->core.ticks,(unsigned long)s->core.models.active.id,
        state_name(s->core.safety.state),s->core.inputs.valid,s->ui.state.valid,s->ui.link.ready);
    printf("channels:");for(unsigned i=0;i<8;++i)printf(" %d",s->core.channels[i]);puts("");
    printf("telemetry source=%u cache=%u generation=%lu frozen=%lu parts=%u tx_errors=%lu rejected=%lu\n",
        s->core.telemetry.values.valid,s->ui.state.snapshot.telemetry.valid,(unsigned long)s->ui.state.generation,
        (unsigned long)s->bridge.frozen.generation,s->ui.state.parts,(unsigned long)s->bridge.tx_errors,(unsigned long)s->ui.state.rejected);
}
static int expect(tyvor_sim_t *s,char **arg,unsigned count) {
    if(count==3 && !strcmp(arg[1],"safety"))return strcmp(state_name(s->core.safety.state),arg[2])?-1:0;
    long value;
    if(count==4 && !strcmp(arg[1],"channel")) {
        long channel;if(number(arg[2],1,8,&channel) || number(arg[3],-1000,1000,&value))return -1;
        return s->core.channels[channel-1]==value?0:-1;
    }
    if(count!=3 || number(arg[2],0,INT32_MAX,&value))return -1;
    unsigned long actual;
    if(!strcmp(arg[1],"rf"))actual=(unsigned)tyvor_core_rf_enabled();
    else if(!strcmp(arg[1],"input_valid"))actual=s->core.inputs.valid;
    else if(!strcmp(arg[1],"model_id"))actual=s->core.models.active.id;
    else if(!strcmp(arg[1],"ticks"))actual=s->core.ticks;
    else if(!strcmp(arg[1],"ui_valid"))actual=s->ui.state.valid;
    else if(!strcmp(arg[1],"ui_model"))actual=s->ui.state.snapshot.model_id;
    else if(!strcmp(arg[1],"ui_safety"))actual=s->ui.state.snapshot.safety_state;
    else if(!strcmp(arg[1],"telemetry_valid"))actual=s->core.telemetry.values.valid;
    else if(!strcmp(arg[1],"ui_telemetry_valid"))actual=s->ui.state.snapshot.telemetry.valid;
    else if(!strcmp(arg[1],"session"))actual=s->ui.link.ready;
    else return -1;
    return actual==(unsigned long)value?0:-1;
}
static int command(tyvor_sim_t *s,char *line) {
    char *args[8];unsigned count=0;char *part=strtok(line," \t\r\n");
    while(part && count<8){if(part[0]=='#')break;args[count++]=part;part=strtok(NULL," \t\r\n");}
    if(!count)return 0;
    long value;
    if(!strcmp(args[0],"advance") && count==2 && !number(args[1],0,60000,&value))return tyvor_sim_advance(s,(uint32_t)value);
    if(!strcmp(args[0],"gimbal") && count==3 && !number(args[2],-1000,1000,&value)) {
        int axis=axis_index(args[1]);if(axis<0)return -1;s->axes[axis]=(int16_t)value;return 0;
    }
    if(!strcmp(args[0],"switch") && count==3 && !number(args[2],0,2,&value)) {
        unsigned which;if(!strcmp(args[1],"arm") && value<2)which=0;else if(!strcmp(args[1],"mode"))which=1;else return -1;
        s->switches[which]=(int8_t)value;return 0;
    }
    if(!strcmp(args[0],"input") && count==2) {
        if(!strcmp(args[1],"loss"))s->input_loss=1;else if(!strcmp(args[1],"recover"))s->input_loss=0;else return -1;return 0;
    }
    if(!strcmp(args[0],"telemetry")) {
        if(count==2) {
            if(!strcmp(args[1],"off"))s->telemetry_loss=1;else if(!strcmp(args[1],"on"))s->telemetry_loss=0;else return -1;return 0;
        }
        if(count==3) {
            if(!strcmp(args[1],"battery") && !number(args[2],1,65535,&value))s->telemetry.values.battery_mv=(uint16_t)value;
            else if(!strcmp(args[1],"lq") && !number(args[2],0,100,&value))s->telemetry.values.lq=(uint8_t)value;
            else if(!strcmp(args[1],"rssi") && !number(args[2],-150,0,&value))s->telemetry.values.rssi_dbm=(int16_t)value;
            else return -1;
            return 0;
        }
        if(count==5 && !strcmp(args[1],"gps")) {
            long latitude,longitude,satellites;
            if(number(args[2],-900000000,900000000,&latitude) || number(args[3],-1800000000,1800000000,&longitude) || number(args[4],0,64,&satellites))return -1;
            s->telemetry.values.latitude_e7=(int32_t)latitude;s->telemetry.values.longitude_e7=(int32_t)longitude;s->telemetry.values.satellites=(uint8_t)satellites;return 0;
        }
        return -1;
    }
    if((!strcmp(args[0],"model") || !strcmp(args[0],"load") || !strcmp(args[0],"save")) && count==2 && !number(args[1],0,1,&value)) {
        return !strcmp(args[0],"save")?tyvor_core_save_model(&s->core,(unsigned)value,s->now):tyvor_sim_model(s,(unsigned)value);
    }
    if(!strcmp(args[0],"reject_model") && count==2 && !number(args[1],0,1,&value))return tyvor_sim_model(s,(unsigned)value)?0:-1;
    if(!strcmp(args[0],"reset") && count==2) {
        if(!strcmp(args[1],"stm32"))return tyvor_sim_reset_control(s);
        if(!strcmp(args[1],"esp32"))return tyvor_sim_reset_ui(s);
        if(!strcmp(args[1],"both"))return tyvor_sim_reset_control(s) || tyvor_sim_reset_ui(s)?-1:0;
        return -1;
    }
    if(!strcmp(args[0],"fault") && count==4 && !number(args[3],0,60000,&value)) {
        unsigned first=0,last=2;
        if(!strcmp(args[1],"c2u"))last=1;else if(!strcmp(args[1],"u2c"))first=1;else if(strcmp(args[1],"both"))return -1;
        for(unsigned i=first;i<last;++i) {
            tyvor_sim_wire_t *w=i?&s->ui_to_control:&s->control_to_ui;
            if(!strcmp(args[2],"loss"))w->loss_every=(uint32_t)value;
            else if(!strcmp(args[2],"corrupt"))w->corrupt_every=(uint32_t)value;
            else if(!strcmp(args[2],"duplicate"))w->duplicate_every=(uint32_t)value;
            else if(!strcmp(args[2],"delay"))w->delay_ms=(uint32_t)value;
            else if(!strcmp(args[2],"jitter"))w->jitter_ms=(uint32_t)value;
            else if(!strcmp(args[2],"chunk") && value>=1 && value<=64)w->chunk_size=(uint32_t)value;
            else if(!strcmp(args[2],"gap") && value<=1000)w->byte_gap_ms=(uint32_t)value;
            else if(!strcmp(args[2],"hold") && value<=1)w->hold=(uint8_t)value;
            else if(!strcmp(args[2],"partial") && value<=1)w->partial=(unsigned)value;
            else if(!strcmp(args[2],"fail") && value<=1)w->fail=(unsigned)value;
            else if(!strcmp(args[2],"clear")) {
                w->loss_every=w->corrupt_every=w->duplicate_every=w->delay_ms=w->jitter_ms=w->byte_gap_ms=0;
                w->hold=0;w->partial=w->fail=w->drop=0;w->chunk_size=7;tyvor_bench_wire_clear(w);
            } else return -1;
        }
        return 0;
    }
    if(!strcmp(args[0],"model_corrupt") && count==2 && !number(args[1],0,1,&value)) {s->storage.bytes[value][12]^=1;return 0;}
    if(!strcmp(args[0],"mixscroll") && count==2 && !number(args[1],0,15,&value)) {s->ui.state.mix_scroll=(uint8_t)value;return 0;}
    if(!strcmp(args[0],"diag") && count==1) {
        tyvor_diagnostics_t d;tyvor_diagnostics_init(&d,TYVOR_ROLE_UI,0x53494d,s->ui_resets-1,1);
        tyvor_diagnostics_update(&d,&s->ui.link,&s->ui_adapter,s->now);
        printf("DIAG software SIM fw=%s protocol=%u boot_reason=SIM resets_observed STM=%lu ESP=%lu lifetime=UNVERIFIED\n",
            TYVOR_FIRMWARE_VERSION,d.protocol,(unsigned long)(s->control_resets-1),(unsigned long)(s->ui_resets-1));
        printf("session=%u epoch=%lu/%lu TX_errors=%lu RX_errors=%lu CRC=%lu partial=%lu STATUS_seen=%u age=%llu heap/stack/WDT_runtime=UNVERIFIED\n",
            d.session_state,(unsigned long)d.local_epoch,(unsigned long)d.peer_epoch,(unsigned long)d.tx_errors,
            (unsigned long)d.rx_errors,(unsigned long)d.crc_errors,(unsigned long)s->ui_adapter.tx_partial,d.status_age_valid,(unsigned long long)d.status_age_ms);return 0;
    }
    if(!strcmp(args[0],"link") && count==2) {
        unsigned drop;if(!strcmp(args[1],"off"))drop=1;else if(!strcmp(args[1],"on"))drop=0;else return -1;
        s->control_to_ui.drop=drop;s->ui_to_control.drop=drop;return 0;
    }
    if(!strcmp(args[0],"page") && count==2 && !number(args[1],0,6,&value)) {
        tyvor_ui_view_t view;if(tyvor_ui_core_view((unsigned)value,&s->ui.state,&view))return -1;
        printf("%s | %s\n",view.title,view.banner);for(unsigned i=0;i<6;++i)puts(view.rows[i]);return 0;
    }
    if(!strcmp(args[0],"status") && count==1){status(s);return 0;}
    if(!strcmp(args[0],"expect"))return expect(s,args,count);
    return -1;
}
int main(int argc,char **argv) {
    FILE *input=stdin;int script=0;
    if(argc==3 && !strcmp(argv[1],"--script")) {input=fopen(argv[2],"r");script=1;if(!input){perror(argv[2]);return 2;}}
    else if(argc!=1){fprintf(stderr,"Usage: tyvor_core_sim [--script scenario.txt]\n");return 2;}
    static tyvor_sim_t sim;if(tyvor_sim_init(&sim))return 2;
    if(!script)puts("TYVOR software bench. Commands: gimbal, switch, advance, model, telemetry, input, reset, link, page, status, expect. Ctrl-Z ends.");
    char line[256];unsigned line_number=0;
    while(fgets(line,sizeof(line),input)) {
        ++line_number;
        if((!strchr(line,'\n') && !feof(input)) || command(&sim,line)) {
            fprintf(stderr,"Scenario FAIL at line %u, time %llu\n",line_number,(unsigned long long)sim.now);
            if(script)fclose(input);return 1;
        }
    }
    if(ferror(input)){if(script)fclose(input);return 2;}
    if(script){fclose(input);printf("SCENARIO PASS: %s (%u lines)\n",argv[2],line_number);}
    status(&sim);return 0;
}
