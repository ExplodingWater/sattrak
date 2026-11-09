#pragma once
#include "gui/update_checker.hpp"
#include <future>
#include <thread>
#include <chrono>
#include <iostream>
namespace gui {
struct UpdateChecks {
    static void notice(UpdateChecker& checker,bool available) {
        std::lock_guard lock(checker.state->mutex);
        checker.state->status=available?UpdateChecker::Status::Available:UpdateChecker::Status::Unavailable;
        checker.state->tag="v1.0.1";
        checker.state->url="https://github.com/ExplodingWater/sattrak/releases";
        checker.state->dismissed=false;
    }
    static int run() {
        int failures=0;
        auto check=[&](bool okay,const char* name){if(!okay){std::cerr<<name<<'\n';++failures;}};
        auto wait=[](UpdateChecker& checker) {
            for(int i=0;i<1000;++i) {
                {std::lock_guard lock(checker.state->mutex);if(checker.state->status!=UpdateChecker::Status::Checking)return true;}
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
        };
        UpdateChecker checker;
        std::promise<void> release;auto gate=release.get_future().share();
        checker.startWith([gate]{gate.wait();return "[]";});
        {std::lock_guard lock(checker.state->mutex);check(checker.state->status==UpdateChecker::Status::Checking,"Startup must remain responsive during a pending request");}
        checker.startWith([]()->std::string{throw std::runtime_error("Duplicate request");});
        release.set_value();check(wait(checker),"Worker completes");
        {std::lock_guard lock(checker.state->mutex);check(checker.state->status==UpdateChecker::Status::Current,"Concurrent checks are coalesced");}
        for(int kind:{0,1}) {
            checker.startWith([kind]()->std::string{if(kind==0)throw std::runtime_error("Simulated network failure");return "malformed JSON";});
            check(wait(checker),"Failure completes");
            std::lock_guard lock(checker.state->mutex);
            check(checker.state->status==UpdateChecker::Status::Unavailable,"Failure must not claim the app is current");
        }
        checker.startWith([]{return R"([{"tag_name":"v999.0.0","draft":false,"prerelease":false,"published_at":"2026-11-09"}])";});
        check(wait(checker),"Retry completes");
        {std::lock_guard lock(checker.state->mutex);check(checker.state->status==UpdateChecker::Status::Available,"Retry recovers after failure");}
        std::promise<void> unblock;auto blocked=unblock.get_future().share();
        std::promise<void> finished;auto finishedFuture=finished.get_future();
        {UpdateChecker temporary;temporary.startWith([blocked,&finished]{blocked.wait();finished.set_value();return "[]";});}
        unblock.set_value();
        check(finishedFuture.wait_for(std::chrono::seconds(1))==std::future_status::ready,"Pending worker survives checker destruction");
        if(!failures)std::cout<<"Update worker responsiveness, failure recovery and shutdown checks passed\n";
        return failures?1:0;
    }
};
}
