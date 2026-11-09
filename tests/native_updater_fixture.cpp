// Isolated native installation fixture. Never points at a production install.
#include "gui/native_update.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    try {
        gui::nativeupdate::configureRuntime();
        const auto root=gui::nativeupdate::installedFolder();
        if(root.empty())throw std::runtime_error("Fixture is not in a managed installation");
        if(argc==6 && std::string(argv[1])=="--schedule") {
            gui::nativeupdate::launchOnExit(root,argv[2],argv[3],std::stoull(argv[4]),argv[5]);
        } else if(argc==1) {
            std::cout<<root.string()<<'\n'<<std::filesystem::current_path().string()<<'\n';
        } else return 2;
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
