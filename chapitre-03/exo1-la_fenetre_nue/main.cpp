#include <NKWindow/NkWindow.h>
#include <NKWindow/Core/NkMain.h>

int nkmain(const nkentseu::NkEntryState&)
{
    using namespace nkentseu;

    NkAppData app;
    app.appName = "Ma salle";
    app.preferredRenderer = NkRendererApi::NK_SOFTWARE;

    if (!NkInitialise(app)) {
        return 1;
    }

    NkWindowConfig config;
    config.title = "Ma salle";
    config.width = 1280;
    config.height = 720;

    nkentseu::Window fenetre(config);

    if (!fenetre.IsValid()) {
        NkClose();
        return 1;
    }

    auto& events = EventSystem::Instance();

    while (fenetre.IsOpen()) {
        events.PollEvents();
    }

    NkClose();
    return 0;
}
