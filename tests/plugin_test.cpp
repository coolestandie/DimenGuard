#include "dimenguard/command/catalog.h"

#include <algorithm>
#include <endstone/permissions/permission.h>
#include <endstone/plugin/plugin.h>
#include <endstone/version.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

extern "C" endstone::Plugin *init_endstone_plugin();

namespace dimenguard {
namespace {

class PluginContractTest : public testing::Test {
protected:
    void SetUp() override
    {
        // Exercise the actual shared-library entry point without server injection or onEnable().
        plugin_.reset(init_endstone_plugin());
        ASSERT_NE(plugin_, nullptr);
    }

    std::unique_ptr<endstone::Plugin> plugin_;
};

TEST_F(PluginContractTest, EntryPointConstructsDisabledPluginWithPersonalIdentity)
{
    EXPECT_FALSE(plugin_->isEnabled());
    const auto &description = plugin_->getDescription();
    EXPECT_EQ(description.getName(), "dimenguard");
    EXPECT_EQ(description.getVersion(), "0.2.0");
    EXPECT_EQ(description.getAPIVersion(), ENDSTONE_API_VERSION);
    EXPECT_EQ(description.getAuthors(), std::vector<std::string>{"coolestandie"});
    EXPECT_TRUE(description.getContributors().empty());
    EXPECT_EQ(description.getPrefix(), "DimenGuard");
}

TEST_F(PluginContractTest, CommandMetadataPreservesAliasAndPublicEntryPermission)
{
    const auto commands = plugin_->getDescription().getCommands();
    ASSERT_EQ(commands.size(), 1);
    const auto &command = commands.front();
    EXPECT_EQ(command.getName(), "dg");
    EXPECT_EQ(command.getAliases(), std::vector<std::string>{"dimenguard"});
    EXPECT_EQ(command.getPermissions(), std::vector<std::string>{"dimenguard.use"});
    auto actual_usages = command.getUsages();
    auto expected_usages = nativeUsages();
    std::ranges::sort(actual_usages);
    std::ranges::sort(expected_usages);
    ASSERT_EQ(actual_usages.size(), 11);
    EXPECT_EQ(actual_usages, expected_usages);
    for (const auto &usage : actual_usages) {
        EXPECT_EQ(usage.find("argument1"), std::string::npos);
        EXPECT_EQ(usage.find("[action: str]"), std::string::npos);
        EXPECT_EQ(usage.find("UUID"), std::string::npos);
    }
    EXPECT_FALSE(command.isRegistered());
}

TEST_F(PluginContractTest, AdministrationAndBypassHaveSeparatePermissionDefaults)
{
    auto permissions = plugin_->getDescription().getPermissions();
    ASSERT_EQ(permissions.size(), 3);
    const auto check_permission = [&](const std::string &name, endstone::PermissionDefault expected_default) {
        const auto found = std::ranges::find(permissions, name, &endstone::Permission::getName);
        ASSERT_NE(found, permissions.end()) << name;
        EXPECT_EQ(found->getDefault(), expected_default) << name;
        EXPECT_TRUE(found->getChildren().empty()) << name;
    };
    check_permission("dimenguard.use", endstone::PermissionDefault::True);
    check_permission("dimenguard.command", endstone::PermissionDefault::Operator);
    check_permission("dimenguard.bypass", endstone::PermissionDefault::False);
}

}
}
