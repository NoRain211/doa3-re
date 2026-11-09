/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "netplay_profile.h"

#include "doa3_build.h"
#include "netplay.h"
#include "sha.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

static int failures;

static void check(const char *name, bool passed)
{
    if (!passed) {
        std::fprintf(stderr, "FAIL %s\n", name);
        ++failures;
    }
}

static void write(const fs::path &path, const std::vector<std::uint8_t> &bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

static std::vector<std::uint8_t> read(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

// A SaveMeta.xbx as the game writes it: UTF-16 with a byte order mark.
static std::vector<std::uint8_t> meta(const std::u16string &text)
{
    std::vector<std::uint8_t> bytes{0xffu, 0xfeu};
    for (const char16_t c : text) {
        bytes.push_back(static_cast<std::uint8_t>(c));
        bytes.push_back(static_cast<std::uint8_t>(c >> 8));
    }
    return bytes;
}

// A blob with a correct hash, so only its entries decide.
struct Entry {
    std::uint8_t kind;
    std::string path;
    std::vector<std::uint8_t> bytes;
};
static std::vector<std::uint8_t> blobOf(const std::vector<Entry> &entries)
{
    std::vector<std::uint8_t> body;
    const auto put = [&body](std::size_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) body.push_back(static_cast<std::uint8_t>(value >> (8u * i)));
    };
    for (const Entry &entry : entries) {
        body.push_back(entry.kind);
        put(entry.path.size(), 2u);
        body.insert(body.end(), entry.path.begin(), entry.path.end());
        if (entry.kind != 1u) continue;
        put(entry.bytes.size(), 4u);
        body.insert(body.end(), entry.bytes.begin(), entry.bytes.end());
    }
    const std::string hash = doaxbv::sha256Hex(body);
    std::vector<std::uint8_t> blob(hash.begin(), hash.end());
    blob.insert(blob.end(), body.begin(), body.end());
    return blob;
}

static bool unpacks(const std::vector<std::uint8_t> &blob, const fs::path &root, bool joiner = false)
{
    std::vector<std::string> names;
    return recomp_netplay_unpack_profile(blob.data(), blob.size(), root, joiner, names);
}

int main()
{
    const auto root = fs::temp_directory_path() / ("recomp-netplay-profile-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path mine = root / "mine", theirs = root / "theirs";

    // Names: the Name= line wherever it is, without the mark, zero-width spaces or controls, as UTF-8.
    const std::vector<std::uint8_t> kasumi = meta(u"Title=DOA2U\r\nName=Ka\u200bsumi \u00e9\u4e00\r\n");
    check("save name", recomp_netplay_profile_name(kasumi.data(), kasumi.size()) == "Kasumi \xc3\xa9\xe4\xb8\x80");
    check("no name line", recomp_netplay_profile_name(kasumi.data(), 8u).empty());
    // A surrogate pair is one 4-byte character; a lone surrogate becomes U+FFFD.
    std::u16string surrogates = u"Name=A\U0001F600";
    surrogates += static_cast<char16_t>(0xd800u);
    surrogates += u"B";
    const std::vector<std::uint8_t> emoji = meta(surrogates);
    check("surrogates", recomp_netplay_profile_name(emoji.data(), emoji.size()) == "A\xf0\x9f\x98\x80\xef\xbf\xbd" "B");

    // A profile, a second one, the game's save with a costume cache beside it and the title's files, as DOA3 writes them.
    const std::vector<std::uint8_t> data{1u, 2u, 3u}, other{6u}, image(10240u, 7u), save(DOA3(15376u, 15432u), 8u);
    const std::vector<std::uint8_t> beside = meta(u"TitleName=Dead or Alive 3\r\n");
    write(mine / "UDATA/54430001/0F515488395E/SaveMeta.xbx", kasumi);
    write(mine / "UDATA/54430001/0F515488395E/SaveImage.xbx", data);
    write(mine / "UDATA/54430001/AAAAAAAAAAAA/SaveMeta.xbx", meta(u"Name=Other\r\n"));
    write(mine / "UDATA/54430001/AAAAAAAAAAAA/SaveImage.xbx", other);
    write(mine / "UDATA/54430001/TitleMeta.xbx", beside);
    write(mine / "UDATA/54430001/TitleImage.xbx", image);
    write(mine / "TDATA/54430001/DOA3SAVE.DAT", save);
    write(mine / "TDATA/54430001/ayaA0.xpr", std::vector<std::uint8_t>(RECOMP_NETPLAY_MAX_PROFILE + 1u, 9u));
    fs::last_write_time(mine / "UDATA/54430001/AAAAAAAAAAAA/SaveImage.xbx",
        fs::last_write_time(mine / "UDATA/54430001/0F515488395E/SaveImage.xbx") - std::chrono::hours(1));
    fs::last_write_time(mine / "UDATA/54430001/AAAAAAAAAAAA/SaveMeta.xbx",
        fs::last_write_time(mine / "UDATA/54430001/0F515488395E/SaveImage.xbx") - std::chrono::hours(1));
    const std::string latest = recomp_netplay_latest_profile(mine);
    check("latest profile", latest == "UDATA/54430001/0F515488395E");
    check("own name", recomp_netplay_own_profile_name(mine) == "Kasumi \xc3\xa9\xe4\xb8\x80");

    // A host's blob carries its profile and the title saves; a joiner's only its profile.
    std::vector<std::uint8_t> host, joiner;
    check("pack host", recomp_netplay_pack_profile(mine, latest, true, host));
    check("pack joiner", recomp_netplay_pack_profile(mine, latest, false, joiner));
    std::vector<std::string> names;
    check("unpack host", recomp_netplay_unpack_profile(host.data(), host.size(), theirs / "host", false, names));
    check("host names", names == std::vector<std::string>{"Kasumi \xc3\xa9\xe4\xb8\x80"});
    check("host files", read(theirs / "host/UDATA/54430001/0F515488395E/SaveImage.xbx") == data &&
        read(theirs / "host/TDATA/54430001/DOA3SAVE.DAT") == save &&
        !fs::exists(theirs / "host/TDATA/54430001/ayaA0.xpr") &&
        read(theirs / "host/UDATA/54430001/TitleMeta.xbx") == beside &&
        read(theirs / "host/UDATA/54430001/TitleImage.xbx") == image &&
        !fs::exists(theirs / "host/UDATA/54430001/AAAAAAAAAAAA"));
    check("unpack joiner", unpacks(joiner, theirs / "joiner", true));
    check("joiner files", fs::exists(theirs / "joiner/UDATA/54430001/0F515488395E/SaveImage.xbx") &&
        !fs::exists(theirs / "joiner/TDATA") && !fs::exists(theirs / "joiner/UDATA/54430001/TitleMeta.xbx"));
    check("joiner's host blob refused", !unpacks(host, theirs / "joinerhost", true));
    std::vector<std::uint8_t> none;
    check("pack no profile", recomp_netplay_pack_profile(root / "missing", "", false, none) &&
        unpacks(none, theirs / "none", true));

    // Only the files DOA3 writes, in their real size and form: the game trusts their framing.
    const std::string folder = "UDATA/54430001/0F515488395E";
    const std::vector<std::pair<const char *, std::vector<Entry>>> refused = {
        {"save of another size", {{1u, "TDATA/54430001/DOA3SAVE.DAT", std::vector<std::uint8_t>(save.size() - 1u)}}},
        {"other title file", {{1u, "TDATA/54430001/ups.dat", data}}},
        {"file beside the title", {{1u, "UDATA/54430001/ups.dat", data}}},
        {"other save file", {{1u, folder + "/ups.dat", data}}},
        {"title image of another size", {{1u, "UDATA/54430001/TitleImage.xbx", data}}},
        {"folder in a save", {{0u, folder + "/inner", {}}}},
        {"file in a nested folder", {{1u, folder + "/inner/SaveMeta.xbx", meta(u"Name=A\r\n")}}},
        {"meta without a byte order mark", {{1u, folder + "/SaveMeta.xbx", std::vector<std::uint8_t>(kasumi.begin() + 2,
            kasumi.end())}}},
        {"meta with two names", {{1u, folder + "/SaveMeta.xbx", meta(u"Name=A\r\nTitle=x Name=B\r\n")}}},
        {"meta without a name", {{1u, folder + "/SaveMeta.xbx", meta(u"Title=x\r\n")}}},
        {"meta too long", {{1u, folder + "/SaveMeta.xbx", meta(u"Name=A\r\n" + std::u16string(0x800u, u' '))}}},
        {"joiner title file", {{1u, "UDATA/54430001/TitleMeta.xbx", beside}}},
        {"joiner two saves", {{1u, folder + "/SaveMeta.xbx", meta(u"Name=A\r\n")},
            {1u, "UDATA/54430001/BBBBBBBBBBBB/SaveMeta.xbx", meta(u"Name=B\r\n")}}},
    };
    for (const auto &[name, entries] : refused) {
        const bool joinerOnly = std::string(name).rfind("joiner", 0u) == 0u;
        check(name, !unpacks(blobOf(entries), theirs / "refused", joinerOnly));
    }
    check("save of this build's size", unpacks(blobOf({{1u, "TDATA/54430001/DOA3SAVE.DAT", save}}), theirs / "save"));
    names.clear();
    const std::vector<std::uint8_t> other_case = blobOf({{1u, folder + "/savemeta.XBX", kasumi}});
    check("any case of SaveMeta.xbx is named", recomp_netplay_unpack_profile(other_case.data(), other_case.size(),
        theirs / "case", true, names) && names.size() == 1u);

    // Damaged blobs.
    std::vector<std::uint8_t> flipped = host;
    flipped.back() ^= 1u;
    check("changed byte", !unpacks(flipped, theirs / "flipped"));
    check("short", !unpacks(std::vector<std::uint8_t>(host.begin(), host.begin() + 63), theirs / "short"));
    std::vector<std::uint8_t> cut = blobOf({{1u, "UDATA/a", data}});
    cut.erase(cut.end() - 1);
    const std::string body(cut.begin() + 64, cut.end());
    const std::string rehash = doaxbv::sha256Hex(std::vector<std::uint8_t>(body.begin(), body.end()));
    std::copy(rehash.begin(), rehash.end(), cut.begin());
    check("file cut short", !unpacks(cut, theirs / "cut"));
    check("unknown kind", !unpacks(blobOf({{2u, "UDATA/a", {}}}), theirs / "kind"));
    check("long path", !unpacks(blobOf({{0u, "UDATA/" + std::string(200u, 'a'), {}}}), theirs / "long"));
    std::vector<Entry> many(513u, Entry{0u, "UDATA/a", {}});
    check("too many entries", !unpacks(blobOf(many), theirs / "many"));

    // Paths that must never be written: outside UDATA and TDATA, not plain names, or Windows devices.
    const char *const unsafe[] = {"../escape", "UDATA/../../escape", "UDATA/./a", "UDATA//a", "/escape",
        "C:/escape", "UDATA\\..\\escape", "SYSTEM/a", "udata/a", "UDATA/a b", "UDATA/con", "UDATA/CON.txt",
        "UDATA/com1", "UDATA/LPT9.dat", "UDATA/a/", "", "UDATA/a:b", "UDATA/a.", "UDATA/a./b"};
    for (const char *path : unsafe) {
        const std::string name = std::string("unsafe \"") + path + '"';
        check(name.c_str(), !unpacks(blobOf({{1u, path, data}}), theirs / "unsafe" / "x"));
    }
    check("nothing escaped", !fs::exists(theirs / "unsafe" / "escape") && !fs::exists(theirs / "escape") &&
        !fs::exists(root / "escape"));
    check("device-like names are fine", unpacks(blobOf({{0u, "UDATA/CONSOLE", {}}, {0u, "UDATA/COM10", {}}}),
        theirs / "names"));

    std::error_code error;
    fs::remove_all(root, error);
    if (failures == 0) std::puts("netplay profile tests passed");
    return failures == 0 ? 0 : 1;
}
