#include "event_queue.hpp"
#include "terminal_sanitizer.hpp"
#include "ui_helpers.hpp"
#include <ncurses.h>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <climits>
#include <cwchar>
#include <stdexcept>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <cwchar>
#include <sstream>
#include <vector>
#include <termios.h>

namespace {
struct Terminal {
  termios saved{}; bool have{};
  Terminal() {
    have = tcgetattr(STDIN_FILENO, &saved) == 0;
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) throw std::runtime_error("TTY requis");
    if(const char* term=std::getenv("TERM");term&&std::string(term)=="dumb") throw std::runtime_error("TERM=dumb incompatible avec ce POC");
    if (!initscr()) throw std::runtime_error("initscr a échoué");
    cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(1);
  }
  ~Terminal() { endwin(); if (have) tcsetattr(STDIN_FILENO, TCSANOW, &saved); }
};
void ack(int fd, const std::string& s) { if(fd>=0){ auto x=s+"\n"; (void)::write(fd,x.data(),x.size()); } }
std::string hex(const std::string& s) { static const char* d="0123456789abcdef"; std::string o; for(unsigned char c:s){o+=d[c>>4];o+=d[c&15];} return o; }
void erase_before(std::string& s,std::size_t& cursor){if(cursor){auto from=tui_poc::prev_utf8(s,cursor);s.erase(from,cursor-from);cursor=from;}}
void erase_after(std::string& s,std::size_t cursor){if(cursor<s.size())s.erase(cursor,tui_poc::next_utf8(s,cursor)-cursor);}
std::size_t display_width(const std::string&s){std::mbstate_t st{};std::size_t col=0;for(std::size_t i=0;i<s.size();){wchar_t wc;auto n=std::mbrtowc(&wc,s.data()+i,s.size()-i,&st);if(n==(std::size_t)-1||n==(std::size_t)-2){++i;++col;st={};}else{int w=::wcwidth(wc);if(w>0)col+=(std::size_t)w;i+=n?n:1;}}return col;}
void move_vertical(const std::string&s,std::size_t& cursor,int direction){
  auto line_start=s.rfind('\n',cursor?cursor-1:0);line_start=line_start==std::string::npos?0:line_start+1;
  auto col=display_width(s.substr(line_start,cursor-line_start));
  if(direction<0){if(line_start==0)return;auto prev_end=line_start-1;auto prev_start=s.rfind('\n',prev_end?prev_end-1:0);prev_start=prev_start==std::string::npos?0:prev_start+1;cursor=prev_start;while(cursor<prev_end){auto next=tui_poc::next_utf8(s,cursor);if(display_width(s.substr(prev_start,next-prev_start))>col)break;cursor=next;}}
  else{auto end=s.find('\n',cursor);if(end==std::string::npos)return;auto next_start=end+1;cursor=next_start;while(cursor<s.size()&&s[cursor]!='\n'){auto next=tui_poc::next_utf8(s,cursor);if(display_width(s.substr(next_start,next-next_start))>col)break;cursor=next;}}
}
}
int main(int argc,char**argv) try {
  int fd=-1,pause_ms=0; bool throw_f12=false; for(int i=1;i<argc;i++){if(!std::strncmp(argv[i],"--protocol-fd=",14))fd=std::atoi(argv[i]+14); if(!std::strncmp(argv[i],"--consumer-pause-ms=",20))pause_ms=std::atoi(argv[i]+20); if(!std::strcmp(argv[i],"--throw-on-f12"))throw_f12=true;}
  std::setlocale(LC_ALL, ""); Terminal term; timeout(20); std::vector<std::string> pages={"Conversation","Outils","Métriques","Diagnostics","Configuration","Aide"}; int page=0,scroll=0;bool composer_focus=true;std::string input,transcript="Historique synthétique initial\n";std::size_t cursor=0,received=0; tui_poc::Sanitizer sanitizer; tui_poc::EventQueue q; bool quit=false;
  std::jthread producer([&](std::stop_token st){for(unsigned long long i=0;i<1000&&!st.stop_requested();i++){std::string payload="delta-"+std::to_string(i)+"\n"; if(i==2)payload="avant\x1b"; if(i==3)payload="]0;POC_UNTRUSTED_TITLE\aapres\n"; if(i==4)payload="\x1b[2JCSI_CUT\n";if(i==5)payload="\x1bPDCS_CUT\x1b\\\n";if(i==6)payload="C0_\x01_CUT\n";if(i==7)payload="C1_\xc2\x85_CUT_BIDI_\xe2\x80\xae_END\n";if(i==8)payload="INVALID_\xf0\x28\x8c\x28_END\n";if(i==9)payload="UTF8 café 漢\n";q.push({i,payload,false}); std::this_thread::sleep_for(std::chrono::milliseconds(10));} q.push({1000,"",true});});
  bool overflow_reported=false,dirty=true;std::size_t frames=0;int last_rows=-1,last_cols=-1;
  while(!quit){ int rows,cols; getmaxyx(stdscr,rows,cols);const bool tiny=cols<=20||rows<=4;const bool compact=cols<50||rows<12;
    if(rows!=last_rows||cols!=last_cols){last_rows=rows;last_cols=cols;ack(fd,"RESIZE "+std::to_string(cols)+"x"+std::to_string(rows));if(cols<=20||rows<=4)ack(fd,"LAYOUT tiny "+std::to_string(cols)+"x"+std::to_string(rows));dirty=true;}
    if(dirty){erase();
    mvaddnstr(0,0,"TUI POC ncursesw | F1-F6 vues | Tab focus | F9 submit | F10 sortie",cols);
    if(rows>1){std::string status="Vue: "+pages[page]+" | Focus: "+(composer_focus?"saisie":"navigation");mvaddnstr(1,0,status.c_str(),cols);}
    if(tiny){if(rows>2){auto compact_label="Mode compact "+std::to_string(cols)+"x"+std::to_string(rows);mvaddnstr(2,0,compact_label.c_str(),cols);}if(rows>3){auto lines=tui_poc::split_lines(input);auto row=lines.empty()?std::string{}:lines.back();mvaddnstr(3,0,row.c_str(),cols);auto prefix=input.substr(0,cursor);auto split=prefix.rfind('\n');auto current=split==std::string::npos?prefix:prefix.substr(split+1);move(3,std::min<int>((int)display_width(current),cols-1));}}
    else {
      if(rows>2){std::string tabs;for(std::size_t i=0;i<pages.size();i++)tabs+=(static_cast<int>(i)==page?"[":" ")+pages[i]+(static_cast<int>(i)==page?"]":" ");mvaddnstr(2,0,tabs.c_str(),cols);}
      int body_top=3,composer_top=std::max(body_top,rows-6),body_height=std::max(1,composer_top-body_top-1);
      if(compact&&rows>3){auto label="Compact "+std::to_string(cols)+"x"+std::to_string(rows);mvaddnstr(body_top,0,label.c_str(),cols);}
      if(page==0){auto visible=tui_poc::visible_lines(transcript,scroll,body_height);for(std::size_t i=0;i<visible.size();i++)mvaddnstr(body_top+(int)i,0,visible[i].c_str(),cols);}
      else {std::vector<std::string> info={page==1?"Outils synthétiques: aucun handler":page==2?"Métriques du POC":page==3?"Diagnostics / sanitizer":page==4?"Configuration / dimensions":"Aide clavier",page==2?"Deltas reçus: "+std::to_string(received):"F1 retour conversation; PgUp/PgDn scroll"};for(std::size_t i=0;i<info.size()&&body_top+(int)i<composer_top;i++)mvaddnstr(body_top+(int)i,0,info[i].c_str(),cols);}
      mvaddnstr(composer_top,0,(std::string("Saisie ")+(composer_focus?"[FOCUS]":"")+" Enter=LF F9=envoyer").c_str(),cols);
      auto inlines=tui_poc::split_lines(input);if(inlines.empty())inlines.emplace_back();auto prefix=input.substr(0,cursor);auto prefix_lines=tui_poc::split_lines(prefix);int cursor_line=(int)prefix_lines.size()-1;int visible_input=std::max(1,rows-composer_top-2);int first=std::max(0,(int)inlines.size()-visible_input);for(int i=first;i<(int)inlines.size()&&composer_top+1+i-first<rows;i++)mvaddnstr(composer_top+1+i-first,0,inlines[i].c_str(),cols);
      int cx=(int)display_width(prefix_lines.back());move(std::min(rows-1,composer_top+1+cursor_line-first),std::min(cols-1,cx));
    }
    refresh();++frames;dirty=false;}
    tui_poc::Event ev; while(q.pop(ev)){dirty=true;if(ev.terminal){transcript+=sanitizer.feed("",true);ack(fd,"TURN_FINISHED "+std::to_string(ev.sequence));}else {auto safe=sanitizer.feed(ev.text); transcript+=safe; ++received;if(ev.sequence<10&&!safe.empty())ack(fd,"SAFE_HEX "+hex(safe));ack(fd,"ACK "+std::to_string(ev.sequence));}}
    if(q.overflowed()&&!overflow_reported){ack(fd,"OVERFLOW "+std::to_string(q.dropped()));overflow_reported=true;}
    if(pause_ms>0){std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms)); pause_ms=0;}
    wint_t key{}; int rc=get_wch(&key); if(rc==ERR)continue;
    if(rc==KEY_CODE_YES){if(key>=KEY_F(1)&&key<=KEY_F(6)){page=(int)key-KEY_F(1);ack(fd,"PANEL "+std::to_string(page));dirty=true;}else if(key==KEY_F(9)){ack(fd,"SUBMIT_HEX "+hex(input));input.clear();cursor=0;dirty=true;}else if(key==KEY_F(10)||key==27)quit=true;else if(key==KEY_F(12)&&throw_f12)throw std::runtime_error("exception contrôlée du POC");else if(key==KEY_RESIZE){dirty=true;}else if(key==KEY_PPAGE&&!composer_focus&&page==0){scroll+=8;ack(fd,"SCROLL "+std::to_string(scroll));dirty=true;}else if(key==KEY_NPAGE&&!composer_focus&&page==0){scroll=std::max(0,scroll-8);ack(fd,"SCROLL "+std::to_string(scroll));dirty=true;}else if(!composer_focus&&(key==KEY_DOWN||key==KEY_RIGHT)){page=(page+1)%6;ack(fd,"PANEL "+std::to_string(page));dirty=true;}else if(!composer_focus&&(key==KEY_UP||key==KEY_LEFT)){page=(page+5)%6;ack(fd,"PANEL "+std::to_string(page));dirty=true;}else if(composer_focus&&key==KEY_LEFT){cursor=tui_poc::prev_utf8(input,cursor);dirty=true;}else if(composer_focus&&key==KEY_RIGHT){cursor=tui_poc::next_utf8(input,cursor);dirty=true;}else if(composer_focus&&key==KEY_UP){move_vertical(input,cursor,-1);dirty=true;}else if(composer_focus&&key==KEY_DOWN){move_vertical(input,cursor,1);dirty=true;}else if(composer_focus&&key==KEY_HOME){auto p=input.rfind('\n',cursor?cursor-1:0);cursor=p==std::string::npos?0:p+1;dirty=true;}else if(composer_focus&&key==KEY_END){auto p=input.find('\n',cursor);cursor=p==std::string::npos?input.size():p;dirty=true;}else if(composer_focus&&key==KEY_BACKSPACE){erase_before(input,cursor);dirty=true;}else if(composer_focus&&key==KEY_DC){erase_after(input,cursor);dirty=true;}continue;}
    if(key==27){continue;}if(key==9){composer_focus=!composer_focus;ack(fd,std::string("FOCUS ")+(composer_focus?"composer":"navigation"));dirty=true;}else if(composer_focus&&(key==10||key==13)){input.insert(cursor,1,'\n');++cursor;dirty=true;}else if(composer_focus&&(key==127||key==KEY_BACKSPACE)){erase_before(input,cursor);dirty=true;}else if(composer_focus&&key>=32&&key<=0x10ffff){wchar_t wc=(wchar_t)key;char b[MB_LEN_MAX];mbstate_t state{};size_t n=wcrtomb(b,wc,&state);if(n!=(size_t)-1){input.insert(cursor,b,n);cursor+=n;dirty=true;}}
  }
  producer.request_stop(); ack(fd,"FRAMES "+std::to_string(frames));ack(fd,"EXIT normal"); return 0;
} catch(const std::exception&e){std::fprintf(stderr,"POC ncursesw: %s\n",e.what());return 2;}
