/*
    Copyright (C) 2017-2021  HolyWu

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <VapourSynth4.h>

#include <libbluray/bluray.h>

using namespace std::string_literals;

namespace {

// Same directories and separator that libbluray uses to look up a playlist
#ifdef _WIN32
constexpr std::array playlistDirs{"BDMV\\PLAYLIST\\", "BDMV\\BACKUP\\PLAYLIST\\"};
#else
constexpr std::array playlistDirs{"BDMV/PLAYLIST/", "BDMV/BACKUP/PLAYLIST/"};
#endif

// libbluray's BLURAY_CLIP_INFO::clip_id is always the first angle's clip, so the clips of other angles are parsed from the MPLS directly.
// A PlayItem without the given angle falls back to its first angle's clip, as libbluray does.
// Returns an empty vector if the MPLS can't be parsed or its PlayItems don't match the ones libbluray reported in titleInfo.
std::vector<std::string> readAngleClipIds(BLURAY* bd, const std::string& path, unsigned angle, const BLURAY_TITLE_INFO* titleInfo) {
    constexpr size_t playlistPosOffset = 8;
    constexpr size_t itemCountOffset = 6;
    constexpr size_t firstItemOffset = 10;
    constexpr size_t clipIdSize = 5;
    constexpr size_t multiAngleFlagPos = 10;
    constexpr uint8_t multiAngleFlag = 0x10;
    constexpr size_t angleCountPos = 32;
    constexpr size_t angleEntriesPos = 34;
    constexpr size_t angleEntrySize = 10;

    void* data = nullptr;
    int64_t size = 0;
    if (!bd_read_file(bd, path.c_str(), &data, &size))
        return {};

    struct FreeDeleter {
        void operator()(uint8_t* p) const noexcept { std::free(p); }
    };
    const std::unique_ptr<uint8_t, FreeDeleter> owner{static_cast<uint8_t*>(data)};
    const std::span<const uint8_t> mpls{owner.get(), static_cast<size_t>(size)};

    const auto readBigEndian = [&](size_t pos, size_t bytes) {
        uint32_t value = 0;
        for (size_t i = 0; i < bytes; i++)
            value = value << 8 | mpls[pos + i];
        return value;
    };

    if (mpls.size() < playlistPosOffset + 4)
        return {};

    const size_t playlistPos = readBigEndian(playlistPosOffset, 4);
    if (playlistPos > mpls.size() - firstItemOffset || readBigEndian(playlistPos + itemCountOffset, 2) != titleInfo->clip_count)
        return {};

    std::vector<std::string> clipIds;
    size_t itemPos = playlistPos + firstItemOffset;
    for (unsigned i = 0; i < titleInfo->clip_count; i++) {
        if (itemPos > mpls.size() - 2)
            return {};

        const size_t itemLength = readBigEndian(itemPos, 2);
        // Not limited to itemLength, since libbluray reads the angle entries even past a PlayItem length too short to hold them
        const auto item = mpls.subspan(itemPos + 2);
        if (itemLength > item.size() || item.size() < clipIdSize)
            return {};

        const auto clipIdAt = [&](size_t pos) {
            const auto clipId = item.subspan(pos, clipIdSize);
            return std::string(clipId.begin(), clipId.end());
        };
        if (clipIdAt(0) != titleInfo->clips[i].clip_id)
            return {};

        size_t clipIdPos = 0;
        if (item.size() > angleCountPos && (item[multiAngleFlagPos] & multiAngleFlag) && angle < item[angleCountPos])
            clipIdPos = angleEntriesPos + (angle - 1) * angleEntrySize;
        if (clipIdPos + clipIdSize > item.size())
            return {};

        clipIds.push_back(clipIdAt(clipIdPos));
        itemPos += 2 + itemLength;
    }
    return clipIds;
}

void VS_CC readmplsCreate(const VSMap* in, VSMap* out, [[maybe_unused]] void* userData, [[maybe_unused]] VSCore* core, const VSAPI* vsapi) noexcept {
    int err = 0;

    const char* bd_path = vsapi->mapGetData(in, "bd_path", 0, nullptr);
    const unsigned playlist = vsapi->mapGetIntSaturated(in, "playlist", 0, nullptr);
    const unsigned angle = vsapi->mapGetIntSaturated(in, "angle", 0, &err);

    auto bd = bd_open(bd_path, nullptr);
    if (!bd)
        return vsapi->mapSetError(out, ("ReadMpls: failed to open "s + bd_path).c_str());

    auto titleInfo = bd_get_playlist_info(bd, playlist, angle);
    if (!titleInfo) {
        vsapi->mapSetError(out, "ReadMpls: failed to get information of the specified playlist or angle");
        bd_close(bd);
        return;
    }

    if (angle > 0 && angle >= titleInfo->angle_count) {
        vsapi->mapSetError(out, "ReadMpls: the specified angle does not exist in the playlist");
        bd_free_title_info(titleInfo);
        bd_close(bd);
        return;
    }

    std::vector<std::string> clipIds;
    if (angle > 0) {
        for (const auto dir : playlistDirs) {
            clipIds = readAngleClipIds(bd, std::format("{}{:05}.mpls", dir, playlist), angle, titleInfo);
            if (!clipIds.empty())
                break;
        }
        if (clipIds.empty()) {
            vsapi->mapSetError(out, "ReadMpls: failed to read the clips of the specified angle from the playlist");
            bd_free_title_info(titleInfo);
            bd_close(bd);
            return;
        }
    } else {
        for (unsigned i = 0; i < titleInfo->clip_count; i++)
            clipIds.emplace_back(titleInfo->clips[i].clip_id);
    }

    vsapi->mapSetInt(out, "count", titleInfo->clip_count, maReplace);
    for (unsigned i = 0; i < titleInfo->clip_count; i++) {
        const std::string filename = clipIds[i] + ".m2ts"s;
        vsapi->mapSetData(out, "clip", (bd_path + "/BDMV/STREAM/"s + filename).c_str(), -1, dtUtf8, maAppend);
        vsapi->mapSetData(out, "filename", filename.c_str(), -1, dtUtf8, maAppend);
    }
    if (titleInfo->clip_count == 1) {
        vsapi->mapSetData(out, "clip", "", -1, dtUtf8, maAppend);
        vsapi->mapSetData(out, "filename", "", -1, dtUtf8, maAppend);
    }

    bd_free_title_info(titleInfo);
    bd_close(bd);
}

}  // namespace

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi) {
    vspapi->configPlugin("com.holywu.readmpls",
                         "mpls",
                         "Get m2ts clip id from a playlist and return a dict",
                         VS_MAKE_VERSION(6, 0),
                         VAPOURSYNTH_API_VERSION,
                         0,
                         plugin);

    vspapi->registerFunction("Read", "bd_path:data;playlist:int;angle:int:opt;", "count:int;clip:data[];filename:data[];", readmplsCreate, nullptr, plugin);
}
