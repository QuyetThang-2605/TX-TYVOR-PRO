#include "tyvor_sim_runtime.h"
#include <stdio.h>
#include <string.h>
static void escape(FILE *f,const char *text) {
    for(;*text;++text)switch(*text) {case '&':fputs("&amp;",f);break;case '<':fputs("&lt;",f);break;case '>':fputs("&gt;",f);break;case '"':fputs("&quot;",f);break;default:fputc(*text,f);break;}
}
static int output(const char *dir,const char *name,unsigned page,const tyvor_ui_state_t *s) {
    char path[1024];if(snprintf(path,sizeof(path),"%s/%s-%u.svg",dir,name,page)>=(int)sizeof(path))return -1;
    FILE *f=fopen(path,"w");if(!f)return -1;tyvor_ui_view_t v;
    if(tyvor_ui_core_view(page,s,&v)){fclose(f);return -1;}
    fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"368\" height=\"448\" viewBox=\"0 0 368 448\">\n<title>",f);escape(f,v.title);
    fputs("</title><rect width=\"368\" height=\"448\" fill=\"#101924\"/><g font-family=\"Segoe UI,Arial\" fill=\"#e9f1fa\">",f);
    fputs("<text x=\"16\" y=\"38\" font-size=\"22\" font-weight=\"bold\">",f);escape(f,v.title);fputs("</text>",f);
    fputs("<rect x=\"12\" y=\"52\" width=\"344\" height=\"36\" rx=\"6\" fill=\"#244059\"/><text x=\"20\" y=\"75\" font-size=\"13\">",f);escape(f,v.banner);fputs("</text>",f);
    for(unsigned i=0;i<6;++i){fprintf(f,"<text x=\"16\" y=\"%u\" font-size=\"12\">",124+44*i);escape(f,v.rows[i]);fputs("</text>",f);}
    fputs("<text x=\"16\" y=\"426\" font-size=\"11\" fill=\"#9fb4c8\">SOFTWARE BENCH | READ ONLY | NO RF</text></g></svg>\n",f);
    return fclose(f)?-1:0;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    static tyvor_sim_t sim;
    const char *states[]={"valid","stale","disconnected","safe","invalid-model"};
    for(unsigned state=0;state<5;++state) {
        if(tyvor_sim_init(&sim) || tyvor_sim_advance(&sim,300))return 1;
        if(state==0) {sim.switches[0]=1;if(tyvor_sim_advance(&sim,300))return 1;}
        else if(state==1 || state==2) {sim.control_to_ui.drop=1;sim.ui_to_control.drop=1;if(tyvor_sim_advance(&sim,state==1?600:2200))return 1;}
        else if(state==4) {sim.storage.bytes[0][12]^=1;if(tyvor_sim_reset_control(&sim) || tyvor_sim_advance(&sim,600))return 1;}
        for(unsigned page=0;page<7;++page)if(output(argv[1],states[state],page,&sim.ui.state))return 1;
    }
    puts("Generated 35 actual Core/UI SVG views: 7 pages x VALID/STALE/DISCONNECTED/SAFE/INVALID MODEL; no panel hardware.");return 0;
}
