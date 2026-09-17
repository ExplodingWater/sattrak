#pragma once
#include <string>
namespace gui {
struct RadioCommand {std::string name,file;float volume=.1f;double endAt=-1;};
bool parseRadioCommand(const std::string& input,RadioCommand& command,std::string& error);
}
