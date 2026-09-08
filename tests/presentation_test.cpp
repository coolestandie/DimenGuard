#include "dimenguard/command/parse.h"
#include "dimenguard/i18n/translator.h"

#include <array>
#include <gtest/gtest.h>
#include <string>

namespace dimenguard {

TEST(Translation, EveryMessageFormatsInBothLocales)
{
    const std::string value = "sample";
    for (int i = 0; i < static_cast<int>(Message::Count); ++i) {
        for (const auto locale : {Locale::English, Locale::Spanish}) {
            const auto message = static_cast<Message>(i);
            EXPECT_FALSE(messageText(message, locale).empty());
            EXPECT_NO_THROW(static_cast<void>(translate(message, locale, value, value, value, value, value, value,
                                                        value, value, value, value, value)));
        }
        const auto en = messageText(static_cast<Message>(i), Locale::English);
        const auto es = messageText(static_cast<Message>(i), Locale::Spanish);
        for (int argument = 0; argument <= 10; ++argument) {
            const auto placeholder = std::format("{{{}}}", argument);
            EXPECT_EQ(en.find(placeholder) != en.npos, es.find(placeholder) != es.npos);
        }
    }
}

TEST(Translation, LocaleUsesClientLanguageAndFallsBackToEnglish)
{
    EXPECT_EQ(parseLocale("es_MX"), Locale::Spanish);
    EXPECT_EQ(parseLocale("ES-es"), Locale::Spanish);
    EXPECT_EQ(parseLocale("es"), Locale::Spanish);
    EXPECT_EQ(parseLocale("estonian"), Locale::English);
    EXPECT_EQ(parseLocale("de_DE"), Locale::English);
    EXPECT_EQ(parseLocale(""), Locale::English);
    EXPECT_NE(translate(Message::Denied, Locale::English), translate(Message::Denied, Locale::Spanish));
}

TEST(Translation, ThemePreservesTextAndResetsFormatting)
{
    const auto result = Theme::decorate("Test");
    EXPECT_NE(result.find(Theme::Amethyst), result.npos);
    EXPECT_NE(result.find(Theme::Muted), result.npos);
    EXPECT_NE(result.find("Test"), result.npos);
    EXPECT_TRUE(result.ends_with(Theme::Reset));
}

TEST(CommandParsing, RejectsPartialAndOverflowingIntegers)
{
    EXPECT_EQ(parseInteger("-2147483648"), -2147483647 - 1);
    EXPECT_EQ(parseInteger("2147483647"), 2147483647);
    for (const auto text : {"", "12foo", "2147483648", "-2147483649", " 2", "1.5"}) {
        EXPECT_FALSE(parseInteger(text));
    }
}

TEST(CommandParsing, AcceptsOnlyCanonicalNonNilUuid)
{
    EXPECT_TRUE(isCanonicalUuid("12345678-abcd-1234-abcd-123456789012"));
    EXPECT_FALSE(isCanonicalUuid("IKyel0I"));
    EXPECT_FALSE(isCanonicalUuid("00000000-0000-0000-0000-000000000000"));
    EXPECT_FALSE(isCanonicalUuid("12345678-ABCD-1234-ABCD-123456789012"));
    EXPECT_FALSE(isCanonicalUuid("12345678xabcd-1234-abcd-123456789012"));
}

}  // namespace dimenguard
