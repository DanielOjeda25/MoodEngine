#pragma once

#include "editor/panels/IPanel.h"

namespace Mood {

class ViewportPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Viewport"; }
};

} // namespace Mood
