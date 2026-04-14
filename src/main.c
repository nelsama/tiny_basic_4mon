/*=============================================================================
  6502 TINY BASIC - Intérprete C89 para cc65/6502 + Tang Nano 9K
  Versión: 8.1 (STOP en lugar de QUIT, C89 Bulletproof, LOAD Binario)
  Compilar: cl65 -c -t none -O --cpu 6502 -I src -I include -o build/main.o src/main.c
=============================================================================*/

#include <stdint.h>

/*-----------------------------------------------------------------------------
  TIPOS EXPLÍCITOS
-----------------------------------------------------------------------------*/
typedef unsigned char  u8;
typedef unsigned int   u16;
typedef signed int     s16;

/*-----------------------------------------------------------------------------
  PUNTEROS A ROM API
-----------------------------------------------------------------------------*/
static void  (*rom_putc)(char)      = (void (*)(char))0xBF18;
static char  (*rom_getc)(void)      = (char (*)(void))0xBF1B;
static u8    (*rom_rx_ready)(void)  = (u8 (*)(void))0xBF21;
static void  (*rom_delay)(u16)      = (void (*)(u16))0xBF33;
static u8    (*rom_sd_init)(void)   = (u8 (*)(void))0xBF00;
static u8    (*rom_mfs_mount)(void) = (u8 (*)(void))0xBF03;
static u8    (*rom_mfs_open)(const char*) = (u8 (*)(const char*))0xBF06;
static void  (*rom_mfs_close)(void) = (void (*)(void))0xBF0C;
static u16   (*rom_mfs_get_size)(void) = (u16 (*)(void))0xBF0F;
static u16   (*rom_mfs_read_ext)(void) = (u16 (*)(void))0xBF27;

static volatile u8 *zp_buf_lo = (u8*)0xF0;
static volatile u8 *zp_buf_hi = (u8*)0xF1;
static volatile u8 *zp_len_lo = (u8*)0xF2;
static volatile u8 *zp_len_hi = (u8*)0xF3;

/*-----------------------------------------------------------------------------
  CONFIGURACIÓN Y GLOBALES
-----------------------------------------------------------------------------*/
#define PROG_SIZE       5000
#define VAR_COUNT       26
#define FOR_STACK_SIZE  8
#define LED_PORT        (*(volatile u8*)0xC001)
#define LED_CONF        (*(volatile u8*)0xC003)

static s16      vars[VAR_COUNT];
static char     prog[PROG_SIZE];
static char     *tp;
static char     *cur_line;
static volatile u8 running;
static u8       do_goto;
static u16      current_line_num;
static u8       run_abort;

struct for_entry { u8 var_idx; s16 end_val; char *line_ptr; u16 line_num; };
static struct for_entry for_stack[FOR_STACK_SIZE];
static u8 for_depth = 0;
static u8 for_looping = 0;

/*-----------------------------------------------------------------------------
  HELPERS
-----------------------------------------------------------------------------*/
static u8 my_strlen(const char *s) { u8 n=0; while(*s++) n++; return n; }

static void my_memmove(char *dst, const char *src, u16 n) {
    if(dst<src) while(n--) *dst++=*src++; else {dst+=n;src+=n;while(n--) *--dst=*--src;}
}

static u16 parse_linenum(const char *s) { u16 v=0; while(*s>='0'&&*s<='9') v=v*10+(*s++-'0'); return v; }

static u8 match_cmd(const char *cmd, u8 len) {
    u8 i=0; char c;
    while(i < len) { if(tp[i]!=cmd[i]) return 0; i++; }
    c = tp[len];
    return (c=='\0'||c==' '||c=='\r'||c=='\n'||c==':');
}

static void outc(char c) { if(c=='\n') rom_putc('\r'); rom_putc(c); }
static void outs(const char *s) { while(*s) rom_putc(*s++); }
static void outn(s16 n) {
    char buf[7]; u8 i=0;
    if(n<0){rom_putc('-');n=-n;} if(n==0)rom_putc('0');
    else { while(n>0){buf[i++]=(char)(n%10+'0');n/=10;} while(i>0)rom_putc(buf[--i]); }
}
static u8 read_uart(void) { while(!rom_rx_ready()); return rom_getc(); }
static void wait_ms(u16 ms) { rom_delay(ms); }

/*-----------------------------------------------------------------------------
  PARSER
-----------------------------------------------------------------------------*/
static void skip(void) { while(*tp==' ') tp++; }
static s16 expr(void);

static s16 factor(void) {
    s16 v=0, sign=1, addr;
    skip();
    if(*tp=='-'){sign=-1;tp++;skip();}
    if(*tp=='('){tp++;v=expr();skip();if(*tp==')')tp++;}
    else if(*tp>='A'&&*tp<='Z') v=vars[*tp++-'A'];
    else if(*tp>='0'&&*tp<='9'){while(*tp>='0'&&*tp<='9')v=v*10+(*tp++-'0');}
    else if(tp[0]=='P'&&tp[1]=='E'&&tp[2]=='E'&&tp[3]=='K'&&tp[4]=='('){
        tp+=5;addr=expr();skip();if(*tp==')'){tp++;v=*(volatile signed char*)(u16)addr;}
    }
    return v*sign;
}

static s16 term(void) {
    s16 a=factor(); char op; s16 b;
    skip();
    while(*tp=='*'||*tp=='/'||*tp=='%'){op=*tp++;b=factor();skip();
        if(op=='*')a*=b;else if(op=='/')a=b?a/b:0;else a=b?a%b:0;}
    return a;
}

static s16 expr(void) {
    s16 a=term(); char op; s16 b;
    skip();
    while(*tp=='+'||*tp=='-'){op=*tp++;b=term();skip();a=(op=='+')?a+b:a-b;}
    return a;
}

/*-----------------------------------------------------------------------------
  MOTOR DE EJECUCIÓN
-----------------------------------------------------------------------------*/
static void exec_stmt(void);

static void exec_stmt(void) {
    s16 a,res,n,addr,start,end_val; u8 vi,i,top,req_vi,err,fi;
    char buf[8],fname[16]; char *s,*p,*save; u16 target,fsize,rd; int done;

    while(1){
        skip(); if(!*tp||*tp=='\r'||*tp=='\n') return;
        if(match_cmd("REM",3)){while(*tp&&*tp!='\r'&&*tp!='\n')tp++; return;}
        if(match_cmd("PRINT",5)){tp+=5;skip();if(*tp=='"'){tp++;while(*tp&&*tp!='"')outc(*tp++);if(*tp=='"')tp++;}else outn(expr());outs("\r\n");goto next;}
        if(match_cmd("IF",2)){tp+=2;a=expr();res=0;skip();
            if(tp[0]=='='&&tp[1]=='='){tp+=2;res=(a==expr());}else if(*tp=='='){tp++;res=(a==expr());}
            else if(*tp=='>'){tp++;res=(a>expr());}else if(*tp=='<'){tp++;res=(a<expr());}
            skip();if(match_cmd("THEN",4)){tp+=4;skip();if(res){if(*tp>='0'&&*tp<='9'){target=(u16)expr();p=prog;
                while(*p){if(*p>='0'&&*p<='9'&&parse_linenum(p)==target){cur_line=p;do_goto=1;return;}p+=my_strlen(p)+1;}
                outs("LINE NOT FOUND\r\n");run_abort=1;return;}exec_stmt();if(do_goto)return;}
                else{while(*tp&&*tp!='\r'&&*tp!='\n'&&*tp!=':')tp++;}}goto next;}
        if(match_cmd("GOTO",4)){tp+=4;skip();target=(u16)expr();p=prog;while(*p){if(*p>='0'&&*p<='9'&&parse_linenum(p)==target){cur_line=p;do_goto=1;return;}p+=my_strlen(p)+1;}outs("LINE NOT FOUND\r\n");run_abort=1;return;}
        if(match_cmd("INPUT",5)){tp+=5;skip();if(*tp>='A'&&*tp<='Z'){vi=(u8)(*tp++-'A');i=0;outs("? ");while(1){u8 c=read_uart();if(c=='\r'||c=='\n'){outs("\r\n");break;}if(c==0x08||c==0x7F){if(i>0){i--;outs("\b \b");}}else if((c>='0'&&c<='9')||(c=='-'&&i==0)){if(i<7){buf[i++]=(char)c;outc(c);}}}buf[i]='\0';n=0;s=buf;if(*s=='-'){n=-1;s++;}while(*s>='0'&&*s<='9')n=n*10+(*s++-'0');vars[vi]=n;}goto next;}
        if(match_cmd("LEDS",4)){tp+=4;skip();LED_PORT=(u8)expr();goto next;}
        if(match_cmd("POKE",4)){tp+=4;skip();addr=(u16)expr();skip();if(*tp==',')tp++;*(volatile u8*)addr=(u8)expr();goto next;}
        if(match_cmd("WAIT",4)){tp+=4;skip();if(*tp=='\r'||*tp=='\n'||*tp==':'){outs("WAIT EXPECTS ARG\r\n");run_abort=1;return;}wait_ms((u16)expr());goto next;}
        if(match_cmd("GET",3)){tp+=3;skip();if(*tp>='A'&&*tp<='Z')vars[*tp++-'A']=rom_rx_ready()?(s16)rom_getc():0;goto next;}
        if(match_cmd("FREE",4)){u16 used=0;p=prog;while(*p){used+=my_strlen(p)+1;p+=my_strlen(p)+1;}outn((s16)(PROG_SIZE-used));outs(" BYTES FREE\r\n");goto next;}
        if(match_cmd("NEW",3)){prog[0]='\0';for_depth=0;for_looping=0;outs("OK\r\n");goto next;}
        if(match_cmd("LIST",4)){p=prog;while(*p){outs(p);outs("\r\n");p+=my_strlen(p)+1;}goto next;}
        
        /* --- STOP (Detiene RUN, vuelve a READY) --- */
        if(match_cmd("STOP",4)){run_abort=1; return;}
        
        /* --- QUIT (Sale al monitor) --- */
        if(match_cmd("QUIT",4)){outs("BYE\r\n");running=0;asm("JMP $8000");return;}
        
        if(match_cmd("LOAD",4)){tp+=4;skip();fi=0;if(*tp=='"')tp++;while(*tp&&*tp!='"'&&*tp!=' '&&*tp!='\r'&&*tp!='\n'&&fi<15)fname[fi++]=*tp++;fname[fi]='\0';err=0;fsize=0;rd=0;rom_sd_init();if(rom_mfs_mount()!=0)err=1;if(!err){if(rom_mfs_open(fname)!=0)err=2;else{fsize=rom_mfs_get_size();if(fsize==0||fsize>=PROG_SIZE)err=3;else{*zp_buf_lo=(u8)(u16)prog;*zp_buf_hi=(u8)((u16)prog>>8);*zp_len_lo=(u8)fsize;*zp_len_hi=(u8)(fsize>>8);rd=rom_mfs_read_ext();prog[rd]='\0';if(rd!=fsize)err=4;}}}rom_mfs_close();if(err){outs("LOAD FAIL (");if(err==1)outs("SD/MOUNT");else if(err==2)outs("FILE NOT FOUND");else if(err==3)outs("EMPTY/LARGE");else outs("READ ERR");outs(")\r\n");run_abort=1;return;}outs("LOADED ");outn((s16)rd);outs(" BYTES\r\n");for_depth=0;for_looping=0;goto next;}
        if(match_cmd("FOR",3)){tp+=3;skip();if(for_looping){for_looping=0;goto next;}if(*tp>='A'&&*tp<='Z'){vi=*tp++-'A';skip();if(*tp=='='){tp++;start=expr();skip();if(tp[0]=='T'&&tp[1]=='O'&&(tp[2]==' '||tp[2]=='\r'||tp[2]=='\n'||tp[2]==':')){tp+=2;skip();end_val=expr();skip();if(for_depth<FOR_STACK_SIZE){vars[vi]=start;for_stack[for_depth].var_idx=vi;for_stack[for_depth].end_val=end_val;for_stack[for_depth].line_ptr=cur_line;for_stack[for_depth].line_num=current_line_num;for_depth++;}else{outs("FOR STACK FULL\r\n");run_abort=1;}goto next;}}}outs("SYNTAX ERROR IN FOR\r\n");run_abort=1;return;}
        if(match_cmd("NEXT",4)){tp+=4;skip();if(for_depth>0){top=for_depth-1;vi=for_stack[top].var_idx;if(*tp>='A'&&*tp<='Z'){req_vi=*tp-'A';if(req_vi!=vi){outs("NEXT WITHOUT FOR\r\n");run_abort=1;return;}tp++;skip();}vars[vi]+=1;end_val=for_stack[top].end_val;done=(vars[vi]>end_val);if(done)for_depth--;else{cur_line=for_stack[top].line_ptr;do_goto=1;for_looping=1;}goto next;}else{outs("NEXT WITHOUT FOR\r\n");run_abort=1;return;}}
        if(match_cmd("LET",3)){tp+=3;skip();}
        if(*tp>='A'&&*tp<='Z'){save=tp;vi=(u8)(*tp++-'A');skip();if(*tp=='='){tp++;vars[vi]=expr();goto next;}tp=save;}
        outs("SYNTAX ERROR IN LINE ");outn(current_line_num);outs("\r\n");run_abort=1;return;
    next: skip(); if(*tp==':'){tp++;continue;} return; }
}

/*-----------------------------------------------------------------------------
  GESTIÓN DE LÍNEAS
-----------------------------------------------------------------------------*/
static void add_line(u16 num, const char *line) {
    u8 len; char *p,*ins,*end,*next;
    len=my_strlen(line)+1;p=prog;ins=0;
    while(*p){if(parse_linenum(p)==num){next=p+my_strlen(p)+1;end=prog;while(*end)end+=my_strlen(end)+1;end++;my_memmove(p,next,(u16)(end-next));p=prog;continue;}p+=my_strlen(p)+1;}
    p=prog;while(*p){if(parse_linenum(p)>num){ins=p;break;}p+=my_strlen(p)+1;}if(!ins)ins=p;
    end=prog;while(*end)end+=my_strlen(end)+1;if((u16)(end-prog)+len>=PROG_SIZE){outs("MEM FULL\r\n");return;}
    my_memmove(ins+len,ins,(u16)(end-ins+1));{u8 i=0;while(i<len){ins[i]=line[i];i++;}}
}

/*-----------------------------------------------------------------------------
  MAIN
-----------------------------------------------------------------------------*/
int main(void) {
    u8 i=0; char line[64]; u8 idx=0; char c;
    LED_CONF=0xC0;LED_PORT=0x00;prog[0]='\0';while(i<VAR_COUNT){vars[i]=0;i++;}
    running=1;current_line_num=0;run_abort=0;for_depth=0;for_looping=0;
    outs("\r\n6502 TINY BASIC V8.1 (STOP CMD)\r\nREADY\r\n");
    while(running){
        outs("> ");idx=0;while(1){c=read_uart();if(c=='\r'||c=='\n'){line[idx]='\0';outs("\r\n");break;}if(c==0x08||c==0x7F){if(idx>0){idx--;outs("\b \b");}}else if(c>=0x20&&c<=0x7E){if(idx<63){if(c>='a'&&c<='z')c-=32;line[idx]=c;outc(c);idx++;}}}
        if(idx==0)continue;tp=line;
        if(*tp>='0'&&*tp<='9')add_line(parse_linenum(tp),line);
        else if(match_cmd("RUN",3)){cur_line=prog;do_goto=0;run_abort=0;for_depth=0;for_looping=0;while(*cur_line&&running&&!run_abort){current_line_num=parse_linenum(cur_line);tp=cur_line;while(*tp>='0'&&*tp<='9')tp++;exec_stmt();if(do_goto){do_goto=0;continue;}if(run_abort)break;cur_line+=my_strlen(cur_line)+1;}}
        else{current_line_num=0;exec_stmt();}
    } return 0;
}