#pragma once
#include "gui/tutorial.hpp"
namespace gui {
struct TutorialChecks {
    static bool run(Tutorial& tour,ADCSView& view,CFDView& cfd,DeveloperConsole& console,AccessibilityPanel& help,int& studio) {
        tutorialRequested=true;
        tour.prepare(view,cfd,console,help,studio);
        if(!tour.active() || tour.currentStep()!=0)return false;
        tour.goTo(-100);
        if(tour.currentStep()!=0)return false;
        tour.goTo(1000);
        if(tour.currentStep()!=Tutorial::totalSteps-1)return false;
        tour.finish(view,cfd,console,help,studio,true);
        if(tour.active() || tutorialRunning)return false;
        const auto before=tour.saved_;
        tutorialRequested=true;
        tour.prepare(view,cfd,console,help,studio);
        const auto after=tour.saved_;
        const bool restored=before.panels==after.panels && before.settings==after.settings && before.replay==after.replay &&
            before.hud==after.hud && before.paused==after.paused && before.bar==after.bar && before.studio==after.studio &&
            before.category==after.category && before.console==after.console && before.help==after.help &&
            before.flow==after.flow && before.controls==after.controls && before.benchmark==after.benchmark && before.cfdPaused==after.cfdPaused;
        tour.goTo(18);tour.prepare(view,cfd,console,help,studio);
        if(studio!=1)return false;
        tour.finish(view,cfd,console,help,studio,false);
        if(tour.active() || studio!=before.studio)return false;
        tutorialRequested=true;
        return restored;
    }
};
}
