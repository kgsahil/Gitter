#include "cli/commands/MergeBaseCommand.hpp"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "util/Logger.hpp"
#include "core/Repository.hpp"


namespace fs = std::filesystem;

namespace gitter {

   Expected<void> MergeBaseCommand::execute(const AppContext&, const std::vector<std::string>& args) {
            if (args.size() != 2) {
                return Error{ErrorCode::InvalidArgs,"Incorrect usage of the merge-base: branch params not correct" };
            }
            std::string branch1 = args[0];
            std::string branch2 = args[1];
            
            auto rootRes = Repository::instance().discoverRoot(fs::current_path());
            if (!rootRes) {
                return Error{ rootRes.error().code,rootRes.error().message};
            }

            auto commit1Res = Repository::instance().getBranchCommit(rootRes.value(), branch1);
            if (!commit1Res) return commit1Res.error();
            if (commit1Res.value().empty()) {
                return Error{ErrorCode::RefNotFound, "No commits on: " + branch1};
            }

            auto commit2Res = Repository::instance().getBranchCommit(rootRes.value(), branch2);
            if (!commit2Res) return commit2Res.error();
            if (commit2Res.value().empty()) {
                return Error{ErrorCode::RefNotFound, "No commit on: " + branch2};
            }

            auto lcnRes = Repository::instance().getMergeBase(rootRes.value(), commit1Res.value(), commit2Res.value());
            if (!lcnRes) {
                return lcnRes.error();
            }

            std::cout << lcnRes.value() << std::endl;

            return {};
   }
}

