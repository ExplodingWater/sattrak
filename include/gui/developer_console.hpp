#pragma once
#include "gui/music_player.hpp"
#include <string>
#include <vector>
struct ImGuiInputTextCallbackData;
namespace gui {
class ActionBindings;
class ADCSView;
class DisplaySettings;
class DeveloperConsole {
public:
    bool isOpen() const { return open; }
    void attachActions(ActionBindings& actions) { actionBindings=&actions; }
    void draw(MusicPlayer& player, ADCSView& view, DisplaySettings& display);
private:
    friend struct ConsoleChecks;
    bool open=false,focus=false;
    ActionBindings* actionBindings=nullptr;
    char input[2048]{};
    struct Suggestion { std::string replacement,label; };
    std::vector<Suggestion> suggestions;
    int selected=0;
    bool scrollSuggestion=false;
    std::string suggestionInput;
    void refreshSuggestions(const std::string& text);
    static int complete(ImGuiInputTextCallbackData* data);
    std::vector<std::string> lines{"Type help for commands. Backtick (`) opens/closes the console. Up/Down selects a suggestion; Tab applies it."};
    void execute(MusicPlayer& player,ADCSView& view,DisplaySettings& display,const std::string& text);
};
}
