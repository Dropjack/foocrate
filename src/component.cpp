#include "component_identity.h"
#include "playback_panel.h"

#include <windows.h>
#include <mmsystem.h>

#include <columns_ui-sdk-8.1.0/ui_extension.h>
#include <foobar2000/SDK/foobar2000.h>
#include <libPPUI/commandline_parser.h>

static_assert(FOOBAR2000_SDK_VERSION == 20250307);
static_assert(std::string_view{UI_EXTENSION_VERSION} == "8.1.0");
static_assert(sizeof(commandline_parser) > 0);
static_assert(refrain::kComponentGuid.Data1 == 0x5aeac28d);

DECLARE_COMPONENT_VERSION(
    "Refrain",
    "0.1.0",
    "Native integrated Columns UI component for foobar2000.\n"
    "Native playback controls, Front Cover summary, Playback Statistics rating, "
    "and versioned foobar2000 Preferences integration.");

VALIDATE_COMPONENT_FILENAME("foo_refrain.dll");
