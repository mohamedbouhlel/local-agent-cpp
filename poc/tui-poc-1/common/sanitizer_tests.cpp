#include "terminal_sanitizer.hpp"
#include "ui_helpers.hpp"
#include <cassert>
#include <iostream>
#include <string>

int main(){
  const std::string cursor_text="aé漢";assert(tui_poc::prev_utf8(cursor_text,cursor_text.size())==3);assert(tui_poc::next_utf8(cursor_text,1)==3);
  assert((tui_poc::split_lines("a\n\n")==std::vector<std::string>{"a","",""}));
  tui_poc::Sanitizer s;std::string out;
  const std::string hostile="A\x1b[2JCSI_CUT\x1b[999;999H" "CURSOR_CUT\nB\x1b]0;TITLE_CUT\aC\x1b]2;OSC_ST_CUT\x1b\\D\x1b]8;;https://bad.invalid\aLINK_CUT\x1b]8;;\aE\x1bPDCS_CUT\x1b\\F\x01" "C0_CUT\x7f\xc2\x85" "C1_CUT\x85" "RAW_C1_CUT\xe2\x80\xae" "BIDI_CUT\xf0\x28\x8c\x28" "INVALID_CUT café 漢\n";
  for(char c:hostile)out+=s.feed(std::string(1,c));
  out+=s.feed("\x1b[31;",false);out+=s.feed("1mCSI_STYLE_CUT\x1b[0m",true);
  for(const auto* token:{"TITLE_CUT","OSC_ST_CUT","DCS_CUT"})if(out.find(token)!=std::string::npos){std::cerr<<"leaked terminal payload: "<<token<<"\n";return 1;}
  for(const auto* token:{"CSI_CUT","CURSOR_CUT","C0_CUT","C1_CUT","RAW_C1_CUT","BIDI_CUT","LINK_CUT","INVALID_CUT","CSI_STYLE_CUT"})assert(out.find(token)!=std::string::npos);
  assert(out.find('\x1b')==std::string::npos&&out.find('\x01')==std::string::npos&&out.find('\x7f')==std::string::npos);
  assert(out.find("\xc2\x85")==std::string::npos&&out.find("\xe2\x80\xae")==std::string::npos&&out.find('\x85')==std::string::npos);
  assert(out.find("\xf0\x28\x8c\x28")==std::string::npos);
  assert(out.find("A")!=std::string::npos&&out.find("café")!=std::string::npos&&out.find("漢")!=std::string::npos);
  assert(out.find("\xef\xbf\xbd")!=std::string::npos);
  std::cout<<"sanitizer corpus passed\n";
}
