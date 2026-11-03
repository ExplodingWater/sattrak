#include "gui/tutorial_state.hpp"
#include <chrono>
#include <iostream>
int main() {
    const auto root=std::filesystem::temp_directory_path()/
        ("sattrak-tutorial-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file=root/std::filesystem::path(u8"user-ä 日本")/"tutorial.txt";
    int failures=0;
    auto check=[&](bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';++failures;}};
    check(!gui::tutorialSeen(file),"Missing state must trigger first run");
    check(gui::saveTutorialState(file,false),"Save skip in Unicode directory");
    check(gui::tutorialSeen(file),"Skip must suppress next first-run tour");
    check(gui::saveTutorialState(file,true),"Save completion");
    check(gui::tutorialSeen(file),"Completed tour must stay dismissed");
    {std::ofstream out(file);out<<"corrupt";}
    check(!gui::tutorialSeen(file),"Corrupt preference must be recoverable");
    {std::ofstream out(file);}
    check(!gui::tutorialSeen(file),"Empty state must be recoverable");
    check(!gui::saveTutorialState(file/"child",true),"Unwritable destination must fail without throwing");
    check(!gui::saveTutorialState({},true),"Missing user directory must fail safely");
    std::error_code error;
    std::filesystem::remove_all(root,error);
    return failures?1:0;
}
