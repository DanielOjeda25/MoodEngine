#pragma once

#include "editor/panels/IPanel.h"

namespace Mood {

class InspectorPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Inspector"; }
};

} // namespace Mood
