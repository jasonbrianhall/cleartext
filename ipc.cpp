#include "ipc.h"
#include "editor_frame.h"
#include <wx/ipc.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/utils.h>

namespace
{
    // A local TCP port is used on Windows (no AF_UNIX support in older
    // toolchains); on Linux/macOS a per-user Unix domain socket is used
    // instead, which -- like gedit's D-Bus based activation -- is scoped to
    // this session and user rather than a guessable, potentially-colliding
    // network port.
#ifdef WIN32
    const wxString kIpcService = "47230";
#else
    const wxString kIpcService = wxString::Format("/tmp/cleartext-ipc-%s", wxGetUserId());
#endif
    const wxString kIpcTopic = "cleartext";

    class ClearTextConnection : public wxConnection
    {
    public:
        explicit ClearTextConnection(ClearTextFrame *frame) : m_frame(frame) {}

        bool OnExec(const wxString &topic, const wxString &data) override
        {
            if (topic != kIpcTopic) return false;

            if (!data.IsEmpty())
            {
                m_frame->OpenFilePath(data);
                m_frame->CloseInitialBlankTabIfUnused();
            }

            if (m_frame->IsIconized()) m_frame->Iconize(false);
            m_frame->Raise();
            m_frame->RequestUserAttention();
            return true;
        }

    private:
        ClearTextFrame *m_frame;
    };

    class ClearTextServer : public wxServer
    {
    public:
        explicit ClearTextServer(ClearTextFrame *frame) : m_frame(frame) {}

        wxConnectionBase *OnAcceptConnection(const wxString &topic) override
        {
            if (topic != kIpcTopic) return nullptr;
            return new ClearTextConnection(m_frame);
        }

    private:
        ClearTextFrame *m_frame;
    };

    ClearTextServer *g_server = nullptr;
}

void RemoveStaleIpcArtifacts()
{
#ifndef WIN32
    if (wxFileExists(kIpcService))
    {
        wxLogNull noLog; // best-effort, not worth a popup
        wxRemoveFile(kIpcService);
    }
#endif
}

bool StartIpcServer(ClearTextFrame *frame)
{
    g_server = new ClearTextServer(frame);
    if (!g_server->Create(kIpcService))
    {
        delete g_server;
        g_server = nullptr;
        return false;
    }
    return true;
}

void StopIpcServer()
{
    delete g_server;
    g_server = nullptr;
}

bool SendToRunningInstance(const wxArrayString &files)
{
    wxClient client;
    wxConnectionBase *conn = client.MakeConnection("localhost", kIpcService, kIpcTopic);
    if (!conn) return false;

    if (files.IsEmpty())
    {
        conn->Execute("");
    }
    else
    {
        for (const wxString &f : files)
        {
            wxFileName fn(f);
            fn.MakeAbsolute(); // resolve against our cwd, not the running instance's
            conn->Execute(fn.GetFullPath());
        }
    }

    conn->Disconnect();
    delete conn;
    return true;
}
