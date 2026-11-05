#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace tui_poc {
inline std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> out; std::size_t from=0;
  while (true) { auto end=text.find('\n',from); if(end==std::string::npos){out.push_back(text.substr(from));break;}out.push_back(text.substr(from,end-from));from=end+1; }
  return out;
}
inline std::vector<std::string> visible_lines(const std::string& text,int offset,int height) {
  auto lines=split_lines(text); if(height<=0)return{};
  const auto end=lines.size()-std::min(lines.size(),static_cast<std::size_t>(offset));
  const auto begin=end>static_cast<std::size_t>(height)?end-static_cast<std::size_t>(height):0;
  return {lines.begin()+static_cast<std::ptrdiff_t>(begin),lines.begin()+static_cast<std::ptrdiff_t>(end)};
}
inline bool continuation(unsigned char c){return (c&0xc0)==0x80;}
inline std::size_t prev_utf8(const std::string&s,std::size_t p){if(!p)return 0;--p;while(p>0&&continuation(static_cast<unsigned char>(s[p])))--p;return p;}
inline std::size_t next_utf8(const std::string&s,std::size_t p){if(p>=s.size())return s.size();++p;while(p<s.size()&&continuation(static_cast<unsigned char>(s[p])))++p;return p;}
}
