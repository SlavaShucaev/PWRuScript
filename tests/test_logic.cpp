// Тесты чистой логики установщика: SHA-256 и файл сумм, параметр запуска Proton
// в localconfig.vdf, проверка старой правки exe. Без сети и без файлов игры.
//   cmake -DPWRU_BUILD_TESTS=ON .. && cmake --build . && ./pwru_tests

#include "langpatch.h"
#include "proton.h"
#include "sha256.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n";   \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                                \
    do {                                                                              \
        ++g_checks;                                                                   \
        const auto va = (a);                                                          \
        const auto vb = (b);                                                          \
        if (!(va == vb)) {                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b  \
                      << "\n  got:      " << va << "\n  expected: " << vb << "\n";    \
            ++g_failures;                                                             \
        }                                                                             \
    } while (0)

static std::string Replace(std::string s, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

static std::size_t Count(const std::string& s, const std::string& what) {
    std::size_t n = 0, pos = 0;
    while ((pos = s.find(what, pos)) != std::string::npos) { ++n; pos += what.size(); }
    return n;
}

static void TestSha256() {
    CHECK_EQ(sha256::OfString(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(sha256::OfString("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(sha256::OfString("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    CHECK_EQ(sha256::OfString(std::string(1000000, 'a')),
             std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));

    // Порциями любого размера — то же, что целиком (границы блока 55/56/63/64 байта).
    const std::string fox = "The quick brown fox jumps over the lazy dog";
    for (std::size_t chunk : {1u, 7u, 55u, 64u}) {
        sha256::Hasher h;
        for (std::size_t i = 0; i < fox.size(); i += chunk) {
            h.Update(fox.data() + i, std::min(chunk, fox.size() - i));
        }
        CHECK_EQ(sha256::ToHex(h.Digest()),
                 std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
    }
    for (std::size_t len : {55u, 56u, 63u, 64u, 65u, 119u, 120u, 128u}) {
        const std::string m(len, 'x');
        sha256::Hasher h;
        for (char ch : m) h.Update(&ch, 1);
        CHECK_EQ(sha256::ToHex(h.Digest()), sha256::OfString(m));
    }
}

static void TestParseSums() {
    const auto sums = sha256::ParseSums(
        "C57D7E92019708B614C90FA3685CD644F543A60153FB99EC9B67C381A245FB2A  PW_RU_Voice.pak\r\n"
        "fc6a3d0fff933962afccbe038a6259ae6fd308057a771f1638cca5f8c59f6094 *PW_RU_Subs.pak\n"
        "# комментарий\n"
        "не сумма\n"
        "35c207762b07da5dc56f5b5ad8a1722e57065787af5ea008177c32d80794912a  PW_RU_UE4SS.zip");
    CHECK_EQ(sums.size(), std::size_t(3));
    CHECK_EQ(sums.at("PW_RU_Voice.pak"), std::string("c57d7e92019708b614c90fa3685cd644f543a60153fb99ec9b67c381a245fb2a"));
    CHECK_EQ(sums.at("PW_RU_Subs.pak"), std::string("fc6a3d0fff933962afccbe038a6259ae6fd308057a771f1638cca5f8c59f6094"));
    CHECK_EQ(sums.at("PW_RU_UE4SS.zip"), std::string("35c207762b07da5dc56f5b5ad8a1722e57065787af5ea008177c32d80794912a"));
    CHECK(sha256::ParseSums("").empty());
}

static void TestOverride() {
    CHECK(proton::HasOverride("WINEDLLOVERRIDES=\"dwmapi=n,b\" %command%"));
    CHECK(proton::HasOverride("WINEDLLOVERRIDES=\"d3d11=n,b;dwmapi=n\" %command%"));
    CHECK(proton::HasOverride("WINEDLLOVERRIDES=dwmapi=native %command%"));
    CHECK(proton::HasOverride("winedlloverrides='xinput1_3,dwmapi=n,b' gamemoderun %command%"));
    CHECK(proton::HasOverride("PROTON_LOG=1 WINEDLLOVERRIDES=\"dwmapi.dll=n,b\" %command%"));
    CHECK(!proton::HasOverride("WINEDLLOVERRIDES=\"dwmapi=b\" %command%"));
    CHECK(!proton::HasOverride("WINEDLLOVERRIDES=\"d3d11=n,b\" %command%"));
    CHECK(!proton::HasOverride("%command% -dwmapi=n"));
    CHECK(!proton::HasOverride(""));

    const std::string line = proton::kLaunchLine;
    CHECK_EQ(proton::MergeOptions(""), line);
    CHECK_EQ(proton::MergeOptions("  "), line);
    CHECK_EQ(proton::MergeOptions("gamemoderun %command%"),
             std::string("WINEDLLOVERRIDES=\"dwmapi=n,b\" gamemoderun %command%"));
    CHECK_EQ(proton::MergeOptions("-dx11"), line + " -dx11");
    CHECK_EQ(proton::MergeOptions("WINEDLLOVERRIDES=\"d3d11=n,b\" %command%"),
             std::string("WINEDLLOVERRIDES=\"d3d11=n,b;dwmapi=n,b\" %command%"));
    CHECK_EQ(proton::MergeOptions("WINEDLLOVERRIDES=d3d11=n,b %command%"),
             std::string("WINEDLLOVERRIDES=d3d11=n,b;dwmapi=n,b %command%"));
    CHECK_EQ(proton::MergeOptions("WINEDLLOVERRIDES=\"\" %command%"), line);
    CHECK_EQ(proton::MergeOptions(line), line);
    for (const char* opt : {"", "gamemoderun %command%", "-dx11", "WINEDLLOVERRIDES=\"d3d11=n,b\" %command%",
                            "WINEDLLOVERRIDES=d3d11=n,b %command%", "mangohud %command% -windowed"}) {
        CHECK(proton::HasOverride(proton::MergeOptions(opt)));
    }
}

static const char* kVdf =
    "\"UserLocalConfigStore\"\n"
    "{\n"
    "\t\"Software\"\n"
    "\t{\n"
    "\t\t\"Valve\"\n"
    "\t\t{\n"
    "\t\t\t\"Steam\"\n"
    "\t\t\t{\n"
    "\t\t\t\t\"apps\"\n"
    "\t\t\t\t{\n"
    "\t\t\t\t\t\"440\"\n"
    "\t\t\t\t\t{\n"
    "\t\t\t\t\t\t\"LaunchOptions\"\t\t\"-novid \\\"quoted\\\" C:\\\\path\"\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t\t\"895870\"\n"
    "\t\t\t\t\t{\n"
    "\t\t\t\t\t\t\"LastPlayed\"\t\t\"1731700000\"\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t}\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t}\n"
    "\t\"friends\"\n"
    "\t{\n"
    "\t\t\"895870\"\n"
    "\t\t{\n"
    "\t\t\t\"LaunchOptions\"\t\t\"decoy\"\n"
    "\t\t}\n"
    "\t}\n"
    "}\n";

static void TestVdf() {
    const std::string vdf = kVdf;
    const auto app = proton::FindApp(vdf);
    CHECK(app.appFound);
    CHECK(!app.hasOptions);                                   // «decoy» в friends — не наш блок

    const auto other = proton::FindApp(vdf, "440");
    CHECK(other.hasOptions);
    CHECK_EQ(other.options, std::string("-novid \"quoted\" C:\\path"));

    const auto upd = proton::WithOverride(vdf);
    CHECK(upd.has_value());
    if (upd) {
        const auto app2 = proton::FindApp(*upd);
        CHECK(app2.hasOptions);
        CHECK_EQ(app2.options, std::string(proton::kLaunchLine));
        CHECK(proton::HasOverride(app2.options));
        CHECK(upd->find("\t\t\t\t\t{\n\t\t\t\t\t\t\"LaunchOptions\"\t\t\"WINEDLLOVERRIDES=\\\"dwmapi=n,b\\\" %command%\"\n"
                        "\t\t\t\t\t\t\"LastPlayed\"") != std::string::npos);
        CHECK(upd->find("\"decoy\"") != std::string::npos);  // чужие блоки не тронуты
        CHECK_EQ(proton::FindApp(*upd, "440").options, other.options);
        const auto again = proton::WithOverride(*upd);       // повторно — без изменений
        CHECK(again.has_value() && *again == *upd);
    }

    // Уже есть свои параметры — дописываем, не стирая.
    const std::string vdf2 = Replace(vdf, "\"LastPlayed\"\t\t\"1731700000\"",
                                     "\"LaunchOptions\"\t\t\"gamemoderun %command%\"");
    const auto upd2 = proton::WithOverride(vdf2);
    CHECK(upd2.has_value());
    if (upd2) {
        CHECK_EQ(proton::FindApp(*upd2).options, std::string("WINEDLLOVERRIDES=\"dwmapi=n,b\" gamemoderun %command%"));
        CHECK_EQ(upd2->size(), vdf2.size() + std::string("WINEDLLOVERRIDES=\\\"dwmapi=n,b\\\" ").size());
    }

    // «Apps» с большой буквы и переводы строк Windows.
    const std::string vdf3 = Replace(Replace(vdf, "\"apps\"", "\"Apps\""), "\n", "\r\n");
    const auto upd3 = proton::WithOverride(vdf3);
    CHECK(upd3.has_value());
    if (upd3) {
        CHECK(proton::FindApp(*upd3).hasOptions);
        CHECK_EQ(Count(*upd3, "\n"), Count(*upd3, "\r\n"));
    }

    // Блока игры нет — ничего не пишем.
    CHECK(!proton::WithOverride("\"UserLocalConfigStore\"\n{\n\t\"Software\"\n\t{\n\t}\n}\n").has_value());
    CHECK(!proton::FindApp("").appFound);

    // Комментарии и условия KeyValues не сбивают разбор.
    const std::string vdf4 = Replace(vdf, "\t\t\t\t\"apps\"\n", "\t\t\t\t// comment \"x\"\n\t\t\t\t\"apps\" [$LINUX]\n");
    CHECK(proton::FindApp(vdf4).appFound);
}

static std::vector<char> Utf16(const std::string& ascii) {
    std::vector<char> out;
    for (char ch : ascii) { out.push_back(ch); out.push_back(0); }
    return out;
}

static void TestLangPatch() {
    std::vector<char> original = {'M', 'Z', 1, 2};
    for (const char* part : {"en-US", "xx", "en-US"}) {
        const auto u = Utf16(part);
        original.insert(original.end(), u.begin(), u.end());
    }
    original.push_back(3);

    std::vector<char> patched = original;
    CHECK_EQ(langpatch::Apply(patched), std::size_t(2));
    CHECK(langpatch::IsPatchedCopyOf(original, patched));
    CHECK(!langpatch::IsPatchedCopyOf(original, original));   // одинаковые — не «пропатченная копия»

    std::vector<char> updated = patched;                      // игру обновили: отличается не только языком
    updated[2] = 9;
    CHECK(!langpatch::IsPatchedCopyOf(original, updated));

    std::vector<char> shorter = patched;
    shorter.pop_back();
    CHECK(!langpatch::IsPatchedCopyOf(original, shorter));

    const std::vector<char> noLang = {'a', 0, 'b', 0};
    CHECK(!langpatch::IsPatchedCopyOf(noLang, noLang));
}

int main() {
    TestSha256();
    TestParseSums();
    TestOverride();
    TestVdf();
    TestLangPatch();
    if (g_failures) {
        std::cerr << g_failures << " of " << g_checks << " checks FAILED\n";
        return 1;
    }
    std::cout << "all " << g_checks << " checks passed\n";
    return 0;
}
