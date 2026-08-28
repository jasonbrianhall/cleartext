#include <wx/wx.h>
#include <wx/snglinst.h>
#include "editor_frame.h"
#include "ipc.h"
#include "app_config.h"
#include "highlighting.h"
#include "themes.h"
#include "custom_themes.h"

class ClearTextApp : public wxApp
{
public:
    bool OnInit() override
    {
        m_instanceChecker = new wxSingleInstanceChecker("ClearText-" + wxGetUserId());

        wxArrayString filesToOpen;
        for (int i = 1; i < argc; i++)
            filesToOpen.Add(argv[i]);

        if (m_instanceChecker->IsAnotherRunning())
        {
            if (SendToRunningInstance(filesToOpen))
            {
                delete m_instanceChecker;
                m_instanceChecker = nullptr;
                return false; // hand-off succeeded, don't open a new window
            }
            // Fall through and open our own window if IPC didn't work
            // (e.g. the other instance is stuck/unresponsive).
        }

        // Installs the on-disk config as the process-wide default (so
        // wxConfigBase::Get() works anywhere, e.g. ClearTextFrame::SaveSession),
        // then reads back the saved theme, font size, and last-session files.
        AppConfig::Init();

        int themeIndex = AppConfig::GetThemeIndex(0);
        if (themeIndex < 0 || themeIndex >= (int)CustomThemes::All().size()) themeIndex = 0;
        SetThemeIndex(themeIndex);

        int fontSize = AppConfig::GetFontSize(kDefaultFontSize);
        if (fontSize < kMinFontSize || fontSize > kMaxFontSize) fontSize = kDefaultFontSize;
        SetFontSize(fontSize);

        wxArrayString lastSessionFiles = AppConfig::GetLastSessionFiles();

        ClearTextFrame *frame = new ClearTextFrame();

        RemoveStaleIpcArtifacts();
        StartIpcServer(frame); // non-fatal if it fails: this instance just won't receive hand-offs

        // Files explicitly passed on the command line always win. Only when
        // there are none do we fall back to whatever was open at last exit.
        if (filesToOpen.IsEmpty())
            filesToOpen = lastSessionFiles;

        for (const wxString &f : filesToOpen)
            frame->OpenFilePath(f);
        if (!filesToOpen.IsEmpty())
            frame->CloseInitialBlankTabIfUnused();

        frame->Show();
        return true;
    }

    int OnExit() override
    {
        StopIpcServer();
        delete m_instanceChecker;
        AppConfig::Shutdown();
        return wxApp::OnExit();
    }

private:
    wxSingleInstanceChecker *m_instanceChecker = nullptr;
};

wxIMPLEMENT_APP(ClearTextApp);
