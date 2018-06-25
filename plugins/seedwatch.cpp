// This does not work with Linux Dwarf Fortress
// With thanks to peterix for DFHack and Quietust for information http://www.bay12forums.com/smf/index.php?topic=91166.msg2605147#msg2605147

#include <map>
#include <string>
#include <vector>
#include "Console.h"
#include "Core.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/World.h"
#include "modules/Kitchen.h"
#include "VersionInfo.h"
#include "df/world.h"
#include "df/plant_raw.h"
#include "df/item_flags.h"
#include "df/items_other_id.h"

using namespace std;
using namespace DFHack;
using namespace df::enums;

DFHACK_PLUGIN("seedwatch");
DFHACK_PLUGIN_IS_ENABLED(running); // whether seedwatch is counting the seeds or not

REQUIRE_GLOBAL(world);

const int buffer = 20; // seed number buffer - 20 is reasonable

// vectors for all farmable plant types
const vector<string> plants_standard =
{
    "MUSHROOM_HELMET_PLUMP",
    "GRASS_TAIL_PIG",
    "GRASS_WHEAT_CAVE",
    "POD_SWEET",
    "BUSH_QUARRY",
    "ROOT_MUCK",
    "TUBER_BLOATED",
    "BULB_KOBOLD",
    "BERRIES_PRICKLE",
    "BERRIES_STRAW",
    "GRASS_LONGLAND",
    "HERB_VALLEY",
    "WEED_RAT",
    "BERRIES_FISHER",
    "REED_ROPE",
    "MUSHROOM_CUP_DIMPLE",
    "WEED_BLADE",
    "ROOT_HIDE",
    "SLIVER_BARB",
    "BERRY_SUN",
    "VINE_WHIP"
};

const vector<string> plants_crops =
{
    "SINGLE-GRAIN_WHEAT",
    "TWO-GRAIN_WHEAT",
    "SOFT_WHEAT",
    "HARD_WHEAT",
    "SPELT",
    "BARLEY",
    "BUCKWHEAT",
    "OATS",
    "ALFALFA",
    "RYE",
    "SORGHUM",
    "RICE",
    "MAIZE",
    "QUINOA",
    "KANIWA",
    "BITTER_VETCH",
    "PENDANT_AMARANTH",
    "BLOOD_AMARANTH",
    "PURPLE_AMARANTH",
    "RED_SPINACH",
    "ELEPHANT-HEAD_AMARANTH",
    "PEARL_MILLET",
    "WHITE_MILLET",
    "FINGER_MILLET",
    "FOXTAIL_MILLET",
    "FONIO",
    "TEFF",
    "FLAX",
    "JUTE",
    "HEMP",
    "COTTON",
    "RAMIE",
    "KENAF",
    "PAPYRUS_SEDGE"
};

const vector<string> plants_garden =
{
    "ARTICHOKE",
    "ASPARAGUS",
    "BAMBARA_GROUNDNUT",
    "STRING_BEAN",
    "BROAD_BEAN",
    "BEET",
    "BITTER_MELON",
    "CABBAGE",
    "CAPER",
    "WILD_CARROT",
    "CASSAVA",
    "CELERY",
    "CHICKPEA",
    "CHICORY",
    "COWPEA",
    "CUCUMBER",
    "EGGPLANT",
    "GARDEN_CRESS",
    "GARLIC",
    "HORNED_MELON",
    "LEEK",
    "LENTIL",
    "LETTUCE",
    "MUNG_BEAN",
    "MUSKMELON",
    "ONION",
    "PARSNIP",
    "PEA",
    "PEANUT",
    "PEPPER",
    "POTATO",
    "RADISH",
    "RED_BEAN",
    "RHUBARB",
    "SOYBEAN",
    "SPINACH",
    "SQUASH",
    "SWEET_POTATO",
    "TARO",
    "TOMATO",
    "TOMATILLO",
    "TURNIP",
    "URAD_BEAN",
    "WATERMELON",
    "WINTER_MELON",
    "LESSER_YAM",
    "LONG_YAM",
    "PURPLE_YAM",
    "WHITE_YAM",
    "PASSION_FRUIT",
    "GRAPE",
    "CRANBERRY",
    "BILBERRY",
    "BLUEBERRY",
    "BLACKBERRY",
    "RASPBERRY",
    "PINEAPPLE"
};


bool ignoreSeeds(df::item_flags& f) // seeds with the following flags should not be counted
{
    return
        f.bits.dump ||
        f.bits.forbid ||
        f.bits.garbage_collect ||
        f.bits.hidden ||
        f.bits.hostile ||
        f.bits.on_fire ||
        f.bits.rotten ||
        f.bits.trader ||
        f.bits.in_building ||
        f.bits.in_job;
};

void printHelp(color_ostream &out) // prints help
{
    out.print(
        "Watches the numbers of seeds available and enables/disables seed and plant cooking.\n"
        "Each plant type can be assigned a limit. If their number falls below,\n"
        "the plants and seeds of that type will be excluded from cookery.\n"
        "If the number rises above the limit + %i, then cooking will be allowed.\n", buffer
        );
    out.printerr(
        "The plugin needs a fortress to be loaded and will deactivate automatically otherwise.\n"
        "You have to reactivate with 'seedwatch start' after you load the game.\n"
        );
    out.print(
        "Options:\n"
        "seedwatch all N      - Adds all plants with limit N to the watch list.\n"
        "seedwatch standard N - Adds standard plants with limit N to the watch list.\n"
        "seedwatch crops N    - Adds crop plants with limit N to the watch list.\n"
        "seedwatch garden N   - Adds garden plants with limit N to the watch list.\n"
        "seedwatch start      - Start watching.\n"
        "seedwatch stop       - Stop watching.\n"
        "seedwatch info       - Display whether seedwatch is watching, and the watch list.\n"
        "seedwatch clear      - Clears the watch list.\n\n"
        );
    out.print(
        "Examples:\n"
        "seedwatch MUSHROOM_HELMET_PLUMP 30\n"
        "  add MUSHROOM_HELMET_PLUMP to the watch list, limit = 30\n"
        "seedwatch MUSHROOM_HELMET_PLUMP\n"
        "  removes MUSHROOM_HELMET_PLUMP from the watch list.\n"
        "seedwatch all 30\n"
        "  adds all plants from the abbreviation list to the watch list, the limit being 30.\n"
        );
};

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    running = enable;
    return CR_OK;
}

command_result df_seedwatch(color_ostream &out, vector<string>& parameters)
{
    CoreSuspender suspend;

    map<string, t_materialIndex> materialsReverser;
    for(size_t i = 0; i < world->raws.plants.all.size(); ++i)
    {
        materialsReverser[world->raws.plants.all[i]->id] = i;
    }

    t_gamemodes gm;
    World::ReadGameMode(gm);// FIXME: check return value

    // if game mode isn't fortress mode
    if(gm.g_mode != game_mode::DWARF || !World::isFortressMode(gm.g_type))
    {
        // just print the help
        printHelp(out);
        return CR_OK;
    }

    string par;
    int limit;
    auto setLimit = [&materialsReverser, &limit](vector<string> in)
    {
        for (vector<string>::const_iterator i = in.begin(); i != in.end(); ++i)
        {
            if (materialsReverser.count(*i) > 0) Kitchen::setLimit(materialsReverser[*i], limit);
        }
    };
    switch(parameters.size())
    {
    case 0:
        printHelp(out);
        return CR_WRONG_USAGE;

    case 1:
        par = parameters[0];
        if ((par == "help") || (par == "?"))
        {
            printHelp(out);
            return CR_WRONG_USAGE;
        }
        else if(par == "start")
        {
            running = true;
            out.print("seedwatch supervision started.\n");
        }
        else if(par == "stop")
        {
            running = false;
            out.print("seedwatch supervision stopped.\n");
        }
        else if(par == "clear")
        {
            Kitchen::clearLimits();
            out.print("seedwatch watchlist cleared\n");
        }
        else if(par == "info")
        {
            out.print("seedwatch Info:\n");
            if(running)
            {
                out.print("seedwatch is supervising.  Use 'seedwatch stop' to stop supervision.\n");
            }
            else
            {
                out.print("seedwatch is not supervising.  Use 'seedwatch start' to start supervision.\n");
            }
            map<t_materialIndex, unsigned int> watchMap;
            Kitchen::fillWatchMap(watchMap);
            if(watchMap.empty())
            {
                out.print("The watch list is empty.\n");
            }
            else
            {
                out.print("The watch list is:\n");
                for(map<t_materialIndex, unsigned int>::const_iterator i = watchMap.begin(); i != watchMap.end(); ++i)
                {
                    out.print("%s : %u\n", world->raws.plants.all[i->first]->id.c_str(), i->second);
                }
            }
        }
        else if(par == "debug")
        {
            map<t_materialIndex, unsigned int> watchMap;
            Kitchen::fillWatchMap(watchMap);
            Kitchen::debug_print(out);
        }
        /*
        else if(par == "dumpmaps")
        {
            out.print("Plants:\n");
            for(auto i = plantMaterialTypes.begin(); i != plantMaterialTypes.end(); i++)
            {
                auto t = materialsModule.df_organic->at(i->first);
                out.print("%s : %u %u\n", organics[i->first].id.c_str(), i->second, t->material_basic_mat);
            }
            out.print("Seeds:\n");
            for(auto i = seedMaterialTypes.begin(); i != seedMaterialTypes.end(); i++)
            {
                auto t = materialsModule.df_organic->at(i->first);
                out.print("%s : %u %u\n", organics[i->first].id.c_str(), i->second, t->material_seed);
            }
        }
        */
        else
        {
            string token = par;
            if(materialsReverser.count(token) > 0)
            {
                Kitchen::removeLimit(materialsReverser[token]);
                out.print("%s is not being watched\n", token.c_str());
            }
            else
            {
                out.print("%s has not been found as a material.\n", token.c_str());
            }
        }
        break;
    case 2:
        limit = atoi(parameters[1].c_str());
        if(limit < 0) limit = 0;
        if (parameters[0] == "standard" || parameters[0] == "all")
        {
            setLimit(plants_standard);
        }
        else if (parameters[0] == "crops" || parameters[0] == "all")
        {
            setLimit(plants_crops);
        }
        else if (parameters[0] == "garden" || parameters[0] == "all")
        {
            setLimit(plants_garden);
        }
        else
        {
            string token = parameters[0];
            if(materialsReverser.count(token) > 0)
            {
                Kitchen::setLimit(materialsReverser[token], limit);
                out.print("%s is being watched.\n", token.c_str());
            }
            else
            {
                out.print("%s has not been found as a material.\n", token.c_str());
            }
        }
        break;
    default:
        printHelp(out);
        return CR_WRONG_USAGE;
        break;
    }

    return CR_OK;
}

DFhackCExport command_result plugin_init(color_ostream &out, vector<PluginCommand>& commands)
{
    commands.push_back(PluginCommand("seedwatch", "Toggles seed cooking based on quantity available", df_seedwatch));
    return CR_OK;
}

DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    switch (event) {
    case SC_MAP_LOADED:
    case SC_MAP_UNLOADED:
        if (running)
            out.printerr("seedwatch deactivated due to game load/unload\n");
        running = false;
        break;
    default:
        break;
    }

    return CR_OK;
}

DFhackCExport command_result plugin_onupdate(color_ostream &out)
{
    if (running)
    {
        // reduce processing rate
        static int counter = 0;
        if (++counter < 500)
            return CR_OK;
        counter = 0;

        t_gamemodes gm;
        World::ReadGameMode(gm);// FIXME: check return value
        // if game mode isn't fortress mode
        if(gm.g_mode != game_mode::DWARF || !World::isFortressMode(gm.g_type))
        {
            // stop running.
            running = false;
            out.printerr("seedwatch deactivated due to game mode switch\n");
            return CR_OK;
        }
        // this is dwarf mode, continue
        map<t_materialIndex, unsigned int> seedCount; // the number of seeds

        // count all seeds and plants by RAW material
        for(size_t i = 0; i < world->items.other[items_other_id::SEEDS].size(); ++i)
        {
            df::item * item = world->items.other[items_other_id::SEEDS][i];
            t_materialIndex materialIndex = item->getMaterialIndex();
            if(!ignoreSeeds(item->flags)) ++seedCount[materialIndex];
        }

        map<t_materialIndex, unsigned int> watchMap;
        Kitchen::fillWatchMap(watchMap);
        for(auto i = watchMap.begin(); i != watchMap.end(); ++i)
        {
            if(seedCount[i->first] <= i->second)
            {
                Kitchen::denyPlantSeedCookery(i->first);
            }
            else if(i->second + buffer < seedCount[i->first])
            {
                Kitchen::allowPlantSeedCookery(i->first);
            }
        }
    }
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown(Core* pCore)
{
    return CR_OK;
}
