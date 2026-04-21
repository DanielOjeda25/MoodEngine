#pragma once

#include "editor/panels/IPanel.h"

namespace Mood {

class AssetBrowserPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Asset Browser"; }
};

} // namespace Mood
