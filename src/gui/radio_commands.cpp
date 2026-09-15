#include "gui/radio_commands.hpp"
#include <vector>
#include <cmath>
#include <cctype>
#include <stdexcept>
namespace gui {
namespace {
double number(const std::string& s){size_t n=0;double v=std::stod(s,&n);if(n!=s.size()||!std::isfinite(v))throw std::runtime_error("Invalid number");return v;}
double timeLimit(const std::string& s){
    try{size_t begin=0;std::vector<std::string> parts;for(;;){auto end=s.find(':',begin);parts.push_back(s.substr(begin,end-begin));if(end==std::string::npos)break;begin=end+1;}
        if(parts.size()>3)return -1;double result=0;
        for(size_t i=0;i<parts.size();++i){double v=number(parts[i]);if(v<0||(i>0&&v>=60)||(i+1<parts.size()&&std::floor(v)!=v))return -1;result=result*60+v;}
        return result>0&&std::isfinite(result)?result:-1;
    }catch(...){return -1;}
}
}
bool parseRadioCommand(const std::string& input,RadioCommand& out,std::string& error){
    try{
        std::vector<std::string> words;std::string word;bool quoted=false,started=false;
        for(char c:input){if(c=='"'){quoted=!quoted;started=true;}else if(std::isspace(static_cast<unsigned char>(c))&&!quoted){if(started){words.push_back(word);word.clear();started=false;}}else{word+=c;started=true;}}
        if(quoted)throw std::runtime_error("Unclosed quote in command.");if(started)words.push_back(word);
        if(words.empty())throw std::runtime_error("Enter a command; try help.");
        RadioCommand command;command.name=words[0];
        if(command.name=="radio.play"){
            if(words.size()<2||words[1].empty()||words[1].starts_with("--"))throw std::runtime_error("Usage: radio.play \"file\" [--volume 0..2] [--endat seconds|MM:SS|HH:MM:SS]");
            command.file=words[1];
            for(size_t i=2;i<words.size();++i){const auto flag=words[i];
                if(flag=="--volume"){
                    if(++i>=words.size())throw std::runtime_error("--volume needs a value from 0 to 2.");double v=number(words[i]);if(v<0||v>2)throw std::runtime_error("Volume must be between 0 and 2.");command.volume=float(v);
                }else if(flag=="--endat"){
                    command.endAt=-1;if(i+1<words.size()&&!words[i+1].starts_with("--"))command.endAt=timeLimit(words[++i]);
                }else throw std::runtime_error("Unknown flag: "+flag);
            }
        }else {
            if(command.name!="help"&&command.name!="clear"&&command.name!="radio.stop"&&command.name!="radio.mute"&&command.name!="radio.unmute"&&command.name!="radio.status")throw std::runtime_error("Unknown command. Type help.");
            if(words.size()!=1)throw std::runtime_error("This command takes no arguments.");
        }
        out=command;error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
