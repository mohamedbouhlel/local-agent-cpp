#pragma once
#include <string>

namespace tui_poc {
// Stateful filter for untrusted terminal text. Escape strings are discarded
// across fragment boundaries; malformed UTF-8 is replaced, never forwarded.
class Sanitizer {
 public:
  std::string feed(const std::string& input, bool final = false) {
    std::string out;
    for (unsigned char c : input) {
      if (utf8_remaining_ > 0) {
        if ((c & 0xc0) == 0x80) {
          utf8_bytes_.push_back(static_cast<char>(c));
          if (utf8_bytes_.size() == 2) {
            const auto lead=static_cast<unsigned char>(utf8_bytes_[0]);
            if ((lead==0xe0&&c<0xa0)||(lead==0xed&&c>0x9f)||(lead==0xf0&&c<0x90)||(lead==0xf4&&c>0x8f)) {
              out += replacement(); utf8_remaining_=0; utf8_bytes_.clear(); continue;
            }
          }
          if (--utf8_remaining_ == 0) {
            unsigned int cp=static_cast<unsigned char>(utf8_bytes_[0])&((utf8_bytes_.size()==2)?0x1f:(utf8_bytes_.size()==3)?0x0f:0x07);
            for(std::size_t i=1;i<utf8_bytes_.size();++i)cp=(cp<<6)|(static_cast<unsigned char>(utf8_bytes_[i])&0x3f);
            const bool control=(cp>=0x80&&cp<=0x9f)||(cp>=0x202a&&cp<=0x202e)||(cp>=0x2066&&cp<=0x2069);
            if(!control)out+=utf8_bytes_;
            utf8_bytes_.clear();
          }
          continue;
        }
        out += replacement(); utf8_remaining_=0; utf8_bytes_.clear();
      }
      switch (state_) {
        case 0:
          if(c==0x1b)state_=1;
          else if(c<0x20&&c!='\n'&&c!='\t'){}
          else if(c==0x7f||(c>=0x80&&c<=0x9f)){}
          else if(c>=0xc2&&c<=0xdf)start_utf8(c,1);
          else if(c>=0xe0&&c<=0xef)start_utf8(c,2);
          else if(c>=0xf0&&c<=0xf4)start_utf8(c,3);
          else if(c>=0x80)out+=replacement();
          else out.push_back(static_cast<char>(c));
          break;
        case 1: if(c=='[')state_=2;else if(c==']')state_=3;else if(c=='P'||c=='^'||c=='_')state_=4;else state_=0;break;
        case 2: if(c>=0x40&&c<=0x7e)state_=0;break;
        case 3: if(c==7||c==0x9c)state_=0;else if(c==0x1b)state_=5;break;
        case 4: if(c==0x9c)state_=0;else if(c==0x1b)state_=5;break;
        case 5: state_=(c=='\\'||c==0x9c)?0:4;break;
      }
    }
    if(final){if(utf8_remaining_)out+=replacement();state_=0;utf8_remaining_=0;utf8_bytes_.clear();}
    return out;
  }
 private:
  static std::string replacement(){return "\xef\xbf\xbd";}
  void start_utf8(unsigned char lead,int continuation_count){utf8_bytes_.assign(1,static_cast<char>(lead));utf8_remaining_=continuation_count;}
  int state_{};int utf8_remaining_{};std::string utf8_bytes_;
};
}
