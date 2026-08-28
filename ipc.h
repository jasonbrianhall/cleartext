#pragma once

#include <wx/arrstr.h>

// A second launch of ClearText hands its file arguments to the
// already-running instance (as new tabs) instead of opening a second
// window. See ipc.cpp for the transport details.

class ClearTextFrame; // OpenFilePath/CloseInitialBlankTabIfUnused are called from the IPC connection

// Removes a stale IPC socket/lock file left behind by a crashed instance,
// if any. Safe to call unconditionally before StartIpcServer.
void RemoveStaleIpcArtifacts();

// Starts the IPC server that receives hand-offs from later launches and
// routes them to `frame`. Returns false (non-fatal) if the server couldn't
// bind -- this instance simply won't receive hand-offs.
bool StartIpcServer(ClearTextFrame *frame);

// Stops the server started by StartIpcServer, if any. Safe to call even if
// StartIpcServer was never called or failed.
void StopIpcServer();

// Tries to hand `files` off to an already-running instance. Returns true
// if a running instance accepted the connection (whether or not `files`
// was empty -- an empty list just raises the existing window).
bool SendToRunningInstance(const wxArrayString &files);
