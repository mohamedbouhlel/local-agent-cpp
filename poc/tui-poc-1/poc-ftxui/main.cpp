#include "event_queue.hpp"
#include "terminal_sanitizer.hpp"
#include "ui_helpers.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;
namespace {
void ack(int fd,const std::string&s){if(fd>=0){auto x=s+"\n";(void)::write(fd,x.data(),x.size());}}
std::string hex(const std::string&s){static const char*d="0123456789abcdef";std::string o;for(unsigned char c:s){o+=d[c>>4];o+=d[c&15];}return o;}
}
int main(int argc,char**argv) try {
  int fd=-1,pause_ms=0;bool throw_f12=false;
  for(int i=1;i<argc;i++){if(!std::strncmp(argv[i],"--protocol-fd=",14))fd=std::atoi(argv[i]+14);if(!std::strncmp(argv[i],"--consumer-pause-ms=",20))pause_ms=std::atoi(argv[i]+20);if(!std::strcmp(argv[i],"--throw-on-f12"))throw_f12=true;}
  if(!isatty(STDIN_FILENO)||!isatty(STDOUT_FILENO)){std::cerr<<"TTY requis\n";return 2;}
  if(const char* term=std::getenv("TERM");term&&std::string(term)=="dumb"){std::cerr<<"TERM=dumb incompatible avec ce POC\n";return 2;}
  auto screen=ScreenInteractive::Fullscreen();screen.TrackMouse(false);screen.HandlePipedInput(false);
  const std::vector<std::string> pages={"Conversation","Outils","Métriques","Diagnostics","Configuration","Aide"};
  int page=0,scroll=0,last_x=-1,last_y=-1;bool composer_focus=true;std::size_t received=0,frames=0;std::string input,transcript="Historique synthétique initial\n";
  tui_poc::Sanitizer sanitizer;tui_poc::EventQueue q;
  auto option=InputOption::Default();option.multiline=true;option.placeholder="Saisie multiligne; Enter ajoute une ligne (F9 soumet)";
  auto input_component=Input(&input,option);
  auto component=Renderer(input_component,[&]{
    const auto dim=Terminal::Size();++frames;if(dim.dimx!=last_x||dim.dimy!=last_y){last_x=dim.dimx;last_y=dim.dimy;ack(fd,"RESIZE "+std::to_string(dim.dimx)+"x"+std::to_string(dim.dimy));if(dim.dimx<=20||dim.dimy<=4)ack(fd,"LAYOUT tiny "+std::to_string(dim.dimx)+"x"+std::to_string(dim.dimy));}const bool tiny=dim.dimx<=20||dim.dimy<=4;const bool compact=dim.dimx<50||dim.dimy<12;
    Elements body;
    if(page==0){auto visible=tui_poc::visible_lines(transcript,scroll,std::max(1,dim.dimy-12));for(const auto&line:visible)body.push_back(text(line));body.push_back(text(scroll?"Historique: défilé vers le haut":"Historique: fin"));}
    else if(page==1)body={text("Outils synthétiques"),text("Aucun handler appelé dans ce POC.")};
    else if(page==2)body={text("Métriques du POC"),text("Deltas reçus: "+std::to_string(received)),text("Séquence synthétique, sans tokens ni moteur.")};
    else if(page==3)body={text("Diagnostics"),text(q.overflowed()?"Flux de présentation incomplet":"Aucun défaut de flux observé"),text("Sanitizer actif sur chaque fragment.")};
    else if(page==4)body={text("Configuration"),text("Backend: FTXUI v7.0.3"),text("Terminal: "+std::to_string(dim.dimx)+"x"+std::to_string(dim.dimy))};
    else body={text("Aide clavier"),text("F1-F6 vues | Tab focus | PgUp/PgDn historique"),text("Enter nouvelle ligne | F9 soumettre | F10 quitter")};
    Elements tabs;for(std::size_t i=0;i<pages.size();i++)tabs.push_back(text((int)i==page?"["+pages[i]+"]":" "+pages[i]+" "));
    Elements all={text("TUI POC FTXUI | F1-F6 vues | Tab focus | F9 soumettre | F10 quitter")|bold,
      text("Focus: "+std::string(composer_focus?"saisie":"navigation")+" | Vue: "+pages[page]),hbox(std::move(tabs))|xflex,separator()};
    if(tiny)all.push_back(text("Mode compact "+std::to_string(dim.dimx)+"x"+std::to_string(dim.dimy)));
    else if(compact)all.push_back(text("Mode compact "+std::to_string(dim.dimx)+"x"+std::to_string(dim.dimy)));
    all.insert(all.end(),body.begin(),body.end());
    all.push_back(separator());all.push_back(text("Saisie (curseur actif si focus saisie):"));all.push_back(input_component->Render());
    return vbox(std::move(all))|border;
  });
  component=CatchEvent(component,[&](Event e){
    if(e==Event::F1||e==Event::F2||e==Event::F3||e==Event::F4||e==Event::F5||e==Event::F6){page=e==Event::F1?0:e==Event::F2?1:e==Event::F3?2:e==Event::F4?3:e==Event::F5?4:5;ack(fd,"PANEL "+std::to_string(page));return true;}
    if(e==Event::Tab){composer_focus=!composer_focus;ack(fd,std::string("FOCUS ")+(composer_focus?"composer":"navigation"));return true;}
    if(!composer_focus){if(e==Event::ArrowDown||e==Event::ArrowRight){page=(page+1)%6;ack(fd,"PANEL "+std::to_string(page));return true;}if(e==Event::ArrowUp||e==Event::ArrowLeft){page=(page+5)%6;ack(fd,"PANEL "+std::to_string(page));return true;}if(e.is_character()||e==Event::Return)return true;}
    if(page==0&&!composer_focus&&(e==Event::PageUp||e==Event::PageDown)){scroll=std::max(0,scroll+(e==Event::PageUp?8:-8));ack(fd,"SCROLL "+std::to_string(scroll));return true;}
    if(e==Event::F9){ack(fd,"SUBMIT_HEX "+hex(input));input.clear();return true;}
    if(e==Event::F10||e==Event::Escape){screen.Exit();return true;}
    if(e==Event::F12&&throw_f12)throw std::runtime_error("exception contrôlée du POC");
    if(e==Event::Custom){if(pause_ms>0){std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms));pause_ms=0;}tui_poc::Event ev;while(q.pop(ev)){if(ev.terminal){transcript+=sanitizer.feed("",true);ack(fd,"TURN_FINISHED "+std::to_string(ev.sequence));}else{auto safe=sanitizer.feed(ev.text);transcript+=safe;++received;if(ev.sequence<10&&!safe.empty())ack(fd,"SAFE_HEX "+hex(safe));ack(fd,"ACK "+std::to_string(ev.sequence));}}if(q.overflowed())ack(fd,"OVERFLOW "+std::to_string(q.dropped()));return true;}
    return false;
  });
  std::atomic<bool> overflow_notified=false;
  std::jthread producer([&](std::stop_token st){for(unsigned long long i=0;i<1000&&!st.stop_requested();i++){std::string payload="delta-"+std::to_string(i)+"\n";if(i==2)payload="avant\x1b";if(i==3)payload="]0;POC_UNTRUSTED_TITLE\aapres\n";if(i==4)payload="\x1b[2JCSI_CUT\n";if(i==5)payload="\x1bPDCS_CUT\x1b\\\n";if(i==6)payload="C0_\x01_CUT\n";if(i==7)payload="C1_\xc2\x85_CUT_BIDI_\xe2\x80\xae_END\n";if(i==8)payload="INVALID_\xf0\x28\x8c\x28_END\n";if(i==9)payload="UTF8 café 漢\n";const bool accepted=q.push({i,payload,false});if(accepted)screen.PostEvent(Event::Custom);else if(!overflow_notified.exchange(true))screen.PostEvent(Event::Custom);std::this_thread::sleep_for(std::chrono::milliseconds(10));}q.push({1000,"",true});screen.PostEvent(Event::Custom);});
  screen.Loop(component);producer.request_stop();ack(fd,"FRAMES "+std::to_string(frames));ack(fd,"EXIT normal");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"POC FTXUI: %s\n",e.what());return 2;}
