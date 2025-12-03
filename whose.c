#include <windows.h>
#include <stdio.h>
#include <assert.h>

#define HOSE_ICON 0

#define PREVIEW_SIZE 32
#define FILENAME_SIZE 14
#define CONFIG_LEAST_SIZE 1

typedef struct Note {
  HWND handle;
  int id;
  int opened;
  int changes;
  char filename[FILENAME_SIZE + 1];
  char preview[PREVIEW_SIZE + 1];
} Note;

struct NoteArray {
  size_t size, capacity;
  Note* data;  
};

typedef struct {
  HWND handle;
  HWND textHandle;
  HWND deleteButtonHandle;
  char filepath[MAX_PATH + 1];
  int id;
} WindowData;

static char NOTESPATH[MAX_PATH + 1] = {0};

static const char NOTESCLASSNAME[] = "Hose_NoteWindow";
static const char MAINCLASSNAME[]  = "Hose_MainWindow";
static char EMPTYNOTE_STRING[] = "Empty Note\0";

static int STD_MAIN_WINDOWWIDTH = 500, STD_MAIN_WINDOWHEIGHT = 500;
static int STD_NOTE_WINDOWWIDTH = 300, STD_NOTE_WINDOWHEIGHT = 300;
static int STD_BUTTONWIDTH = 100, STD_BUTTONHEIGHT = 32;

static int FISSURE = 16;

static HWND MAIN_CREATEBUTTON_HANDLE, MAIN_OPENBUTTON_HANDLE, MAIN_DELETEBUTTON_HANDLE, MAIN_NOTELIST_HANDLE;

static int MAIN_CREATEBUTTON_ID = 1, MAIN_NOTELIST_ID = 2, NOTE_DELETEBUTTON_ID = 3, MAIN_OPENBUTTON_ID = 4, MAIN_DELETEBUTTON_ID = 5, NOTE_EDIT_ID = 6;

static int NOTEID = 0;
static struct NoteArray noteArray = {.size = 0, .capacity = 0, .data = NULL};

LRESULT CALLBACK DEBUGPROC(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

  switch(uMsg) {
    case WM_CLOSE: {
      DestroyWindow(hwnd);
    } return 0;

    case WM_DESTROY: return 0;
  }

  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

char *strfmt(const char *fmt, ...) {
  va_list args;
  va_list args_copy;
  char *buffer;
  int needed;

  va_start(args, fmt);
  va_copy(args_copy, args);

  needed = vsnprintf(NULL, 0, fmt, args);
  if (needed < 0) {
      va_end(args);
      va_end(args_copy);
      return NULL; // Encoding error
  }

  buffer = malloc((size_t)needed + 1);
  if (!buffer) {
      va_end(args);
      va_end(args_copy);
      return NULL;
  }
  
  vsnprintf(buffer, (size_t)needed + 1, fmt, args_copy);

  va_end(args);
  va_end(args_copy);
  return buffer;
}

char *vstrfmt(const char *fmt, va_list args) {
  va_list args_copy;
  char *buffer;
  int needed;

  va_copy(args_copy, args);

  needed = vsnprintf(NULL, 0, fmt, args_copy);
  va_end(args_copy);

  if (needed < 0)
      return NULL;

  buffer = malloc((size_t)needed + 1);
  if (!buffer)
      return NULL;

  vsnprintf(buffer, (size_t)needed + 1, fmt, args);
  return buffer;
}

static int debug_registered = 0;

void DEBUGMSG(const char *fmt, ...) {
  va_list args;
  
  va_start(args, fmt);
  char* buffer = vstrfmt(fmt,args);
  va_end(args);

  if(!debug_registered) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DEBUGPROC;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "custom_popup";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    debug_registered = 1;
  }

  HWND hwnd = CreateWindowExA(
      WS_EX_TOPMOST,
      "custom_popup",
      buffer,
      WS_OVERLAPPEDWINDOW,
      100, 100,
      300, 200,
      NULL, NULL, GetModuleHandle(NULL), NULL
    );

  ShowWindow(hwnd,SW_SHOWNORMAL);
  
  free(buffer);
}

void DEBUGPRINTARRAY(const char* prefix) {
  if(!debug_registered) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DEBUGPROC;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "custom_popup";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    debug_registered = 1;
  }

  char buffer[256];
  DEBUGMSG("%s: ARRAY HAS %d ELEMENTS", prefix, noteArray.size);
  for(int i = 0; i < noteArray.size; ++i) {
    Note* note = &noteArray.data[i];
    DEBUGMSG("%s: NOTE %i in %s", prefix, note->id, note->filename);
  }
}

Note* Push(Note n) {
  if(noteArray.size >= noteArray.capacity) {
    if(!noteArray.capacity) noteArray.capacity = 6;
    else noteArray.capacity *= 2;

    Note* nArr = malloc(sizeof(Note) * noteArray.capacity);
    if(noteArray.data){
      memcpy(nArr,noteArray.data,noteArray.size * sizeof(Note));
      free(noteArray.data);
    }
    noteArray.data = nArr;
  }
  noteArray.data[noteArray.size++] = n;
  return &noteArray.data[noteArray.size-1];
}

void SwapRemove(size_t i) {
  if(i >= noteArray.size) return;

  noteArray.data[i] = noteArray.data[--noteArray.size];
}

size_t IndexOf(int id) {
  for(int i = 0; i < noteArray.size; ++i) {
    if(noteArray.data[i].id == id) return i;
  }
  return SIZE_MAX;
}

Note* Find(int id) {
  for(int i = 0; i < noteArray.size; ++i) {
    if(noteArray.data[i].id == id) return &noteArray.data[i];
  }
  return NULL;
}

long GetSizeOfFile(FILE* f) {
  fseek(f,0,SEEK_END);
  long size = ftell(f);
  fseek(f,0,SEEK_SET);
  return size;
}

static inline long ConfigLength(FILE* f) {
  int c;

  fseek(f,0,SEEK_SET);

  while((c = fgetc(f)) != EOF && c != '\n');
  long len = fseek(f,0,SEEK_CUR);

  fseek(f,0,SEEK_SET);
  return len;
}

void PrepareNotesPath() {
  char userPath[MAX_PATH];
  DWORD len = GetEnvironmentVariable("USERPROFILE",userPath,MAX_PATH);

  if(len == 0 || len >= MAX_PATH) {    // Fall back to executable's directory
    userPath[0] = '.';
    userPath[1] = '\0';
  }

  // Default notes path: %USERPROFILE%/hose/
  sprintf(NOTESPATH,"%s\\hose",userPath); // Appends null terminator

  CreateDirectory(NOTESPATH,NULL); // Create dir if not exists
}

Note* InitNoteWithConfigFromDisk(FILE* f) {
  long filelen = GetSizeOfFile(f);
  long configLength = ConfigLength(f);
  if(ConfigLength < CONFIG_LEAST_SIZE || configLength == filelen) assert(false && "false config");

  int opened;
  fread(&opened,1,1,f);       // Read was open?

  Note* note = Push((Note){
    .opened = opened, 
    .changes = 0, 
    .id = NOTEID++
  });
  return note;
}

void OpenNoteFromList(const char* filepath, HINSTANCE hInstance, HWND listHandle, int index);

void FindNotesFromDisk(const char* path) {
  char searchBuffer[MAX_PATH] = {0};
  int written = snprintf(searchBuffer,MAX_PATH,"%s\\*.hnote",path); // Appends null terminator
  if(written >= MAX_PATH) {
    assert(false && "search path is longer than MAX_PATH");
  }

  WIN32_FIND_DATAA fd;

  HANDLE hfind = FindFirstFile(searchBuffer,&fd);
  if(hfind == INVALID_HANDLE_VALUE) {
    // Could not find valid files in path on disk
    return;
  }

  char fullpath[MAX_PATH + 1];
  do { 

    sprintf(fullpath,"%s\\%s",NOTESPATH,fd.cFileName);

    size_t len = strlen(fd.cFileName);
    if(len > FILENAME_SIZE) continue;

    FILE* file = fopen(fullpath,"r");
    if(!file) continue;

    Note* n = InitNoteWithConfigFromDisk(file);
    memcpy(n->filename,fd.cFileName,len);
    n->filename[FILENAME_SIZE] = '\0';

    long fileLength = GetSizeOfFile(file);
    long configLength = ConfigLength(file);

    fileLength = fileLength - configLength;

    // Read and set preview of note
    if(fileLength == 0) memcpy(n->preview,EMPTYNOTE_STRING,sizeof(EMPTYNOTE_STRING));
    else {
      if(fileLength > PREVIEW_SIZE) fileLength = PREVIEW_SIZE;
      fseek(file,configLength + 1,SEEK_SET);
      fread(n->preview,1,fileLength,file);
      n->preview[PREVIEW_SIZE] = '\0';
    }

     // Add preview to list and assign the custom id
    int index = SendMessage(MAIN_NOTELIST_HANDLE,LB_ADDSTRING,0,(LPARAM)n->preview);
    SendMessage(MAIN_NOTELIST_HANDLE,LB_SETITEMDATA,index,(LPARAM)n->id);

    fclose(file);

    // If they were open, open them now
    if(n->opened)
      OpenNoteFromList(fullpath,GetModuleHandle(NULL),MAIN_NOTELIST_HANDLE,index);

    
  } while(FindNextFile(hfind,&fd));

  FindClose(hfind);
}

void DeleteNoteFromDisk(const char* path, const char* filename) {
  static char buffer[MAX_PATH + 1];
  sprintf(buffer,"%s\\%s",path,filename);
  DeleteFileA(buffer);
}

void RemoveNoteFromListById(Note* note, HWND listHandle) {
  if(!note) return;

  int count = SendMessage(listHandle,LB_GETCOUNT,0,0);
  for(int i = 0; i < count; ++i) {
    int id_ = SendMessage(listHandle,LB_GETITEMDATA,i,0);
    if(id_ == note->id) {
      SendMessage(listHandle,LB_DELETESTRING,i,0);
      break;
    }
  }
}

void DeleteNote(Note* note) {
  if(!note) return;

  if(note->opened) DestroyWindow(note->handle);

  RemoveNoteFromListById(note, MAIN_NOTELIST_HANDLE);

  size_t index = IndexOf(note->id);
  if(index != SIZE_MAX) SwapRemove(index);
}

void UpdateNotePreview(Note* note, HWND textHandle, HWND listHandle) {
  if(!note) return;
  
  int len = GetWindowTextLength(textHandle);
  if(len == 0) memcpy(note->preview,EMPTYNOTE_STRING,sizeof(EMPTYNOTE_STRING));
  else {
    if(len > PREVIEW_SIZE) len = PREVIEW_SIZE;
    char text[PREVIEW_SIZE + 1] = {0};
    GetWindowText(textHandle,text,len + 1);
    memcpy(note->preview,text,len + 1);
  }

  int count = SendMessage(listHandle,LB_GETCOUNT,0,0);
  for(int i = 0; i < count; ++i) {
    int id_ = SendMessage(listHandle,LB_GETITEMDATA,i,0);
    if(id_ == note->id) {
      SendMessage(listHandle,LB_DELETESTRING,i,0);
      int ni = SendMessage(listHandle,LB_INSERTSTRING,i,(LPARAM)note->preview);
      SendMessage(listHandle,LB_SETITEMDATA,ni,(LPARAM)note->id);
      break;
    }
  }
}

void WriteNoteToDisk(const char* filepath, HWND textHandle, Note* note) {
  if(!filepath || !textHandle || !note) return;

  int len = GetWindowTextLength(textHandle);
  if(len <= 1) return;  // Empty note, do not write to disk

  FILE* f = fopen(filepath,"w");
  if(!f) return;

  char* buffer = malloc(len + 1); // + NT
  GetWindowText(textHandle,buffer,len + 1);

  // 
  // Write config 
  // 
  fputc(note->opened,f);         // Opened during exit?
  // End config with new line
  fputc('\n',f);

  // 
  // Write note text
  // 
  fwrite(buffer,1,len,f);

  fclose(f);
  free(buffer);
}

void ReadNoteTextFromDisk(FILE* f, HWND textHandle, ssize_t offset) {
  if(!f || !textHandle) return;

  long size = GetSizeOfFile(f) - offset;
  fsetpos(f,(fpos_t*)&offset);

  char* buffer = malloc(size + 1);
  fread(buffer,1,size,f);
  buffer[size] = '\0';
  fclose(f);

  SetWindowText(textHandle,buffer);
  free(buffer);
}

void CloseNote(const char* filepath, HWND textHandle, Note* note) {
  if(!note || !filepath || !textHandle) return;

  if(note->changes) WriteNoteToDisk(filepath,textHandle,note);

  note->changes = 0;
  note->opened = 0;
}

LRESULT CALLBACK NoteWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  WindowData* wd = (WindowData*)GetWindowLongPtr(hwnd,GWLP_USERDATA);

  switch(uMsg) {
    case WM_CLOSE: {
      Note* note = Find(wd->id);
      CloseNote(wd->filepath,wd->textHandle,note);
      DestroyWindow(hwnd);
    } return 0;

    case WM_DESTROY: {
      free(wd);
      SetWindowLongPtr(hwnd,GWLP_USERDATA,0);
    } return 0;

    case WM_SIZE: {
      int width = LOWORD(lParam);
      int height = HIWORD(lParam);

      // Reposition text box
      SetWindowPos(
        wd->textHandle,NULL,
        0,0,width,height-STD_BUTTONHEIGHT,
        SWP_NOZORDER
      );

      // Reposition button
      if(wd->deleteButtonHandle) {
        SetWindowPos(
          wd->deleteButtonHandle,NULL,
          10,height-STD_BUTTONHEIGHT,STD_BUTTONWIDTH,STD_BUTTONHEIGHT,
          SWP_NOZORDER
        );
      }
    } return 0;

    case WM_COMMAND : {
      int wmId = LOWORD(wParam);
      int msg = HIWORD(wParam);

      if(wmId == NOTE_DELETEBUTTON_ID) {
        Note* note = Find(wd->id);
        if(!note) assert(false && "this should not have happened");

        DeleteNoteFromDisk(NOTESPATH,note->filename);
        DeleteNote(note);
      }
      else if(wmId == NOTE_EDIT_ID && msg == EN_CHANGE) {
        Note* note = Find(wd->id);
        UpdateNotePreview(note,wd->textHandle,MAIN_NOTELIST_HANDLE);
        note->changes = 1;
      }
    } break;
  }

  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

void CreateStandardNoteComponents(WindowData* wd, HINSTANCE hInstance) {
  // Create note window itself
  wd->handle = CreateWindowEx(
    0,NOTESCLASSNAME,"Hose_Note", 
    WS_OVERLAPPEDWINDOW, 
    CW_USEDEFAULT,CW_USEDEFAULT,STD_NOTE_WINDOWWIDTH,STD_NOTE_WINDOWHEIGHT, 
    NULL,NULL,hInstance,wd
  );

  if(wd->handle == NULL)
    return;

  // Create text are of the window
  wd->textHandle = CreateWindowEx(
    0,"EDIT","",
    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
    0,0,STD_NOTE_WINDOWWIDTH,STD_NOTE_WINDOWHEIGHT - STD_BUTTONHEIGHT,
    wd->handle,(HMENU)NOTE_EDIT_ID,hInstance,NULL
  );

  // Create delete button of the note
  wd->deleteButtonHandle = CreateWindowEx(
    0,"BUTTON","Delete",
    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    10,STD_NOTE_WINDOWHEIGHT-STD_BUTTONHEIGHT,STD_BUTTONWIDTH,STD_BUTTONHEIGHT,
    wd->handle,(HMENU)NOTE_DELETEBUTTON_ID,hInstance,NULL
  );
}

void OpenNoteFromList(const char* filepath, HINSTANCE hInstance, HWND listHandle, int index) {
  WindowData* wd = malloc(sizeof(WindowData));
  if(!wd) return;

  int id = SendMessage(listHandle,LB_GETITEMDATA,index,0);
  Note* note = Find(id);
  if(!note) return;

  strcpy(wd->filepath,filepath);

  CreateStandardNoteComponents(wd,hInstance);
  if(wd->handle == NULL) {
    free(wd);
    return;
  }
  if(wd->textHandle == NULL) {
    // TODO
    // Delete window for now
    DestroyWindow(wd->handle);
    return;
  }

  FILE* f = fopen(filepath,"r");
  if(!f) return;

  long configlen = ConfigLength(f);
  ReadNoteTextFromDisk(f,wd->textHandle,configlen + 1);

  note->changes = 1;
  note->handle = wd->handle;
  note->opened = 1;
  wd->id = id;

  SetWindowLongPtr(wd->handle,GWLP_USERDATA,(LONG_PTR)wd);
  ShowWindow(wd->handle,SW_SHOWNORMAL);
  SetForegroundWindow(wd->handle);
}

void CreateNewNoteAndAddToList(const char* notePath, HINSTANCE hInstance, HWND listHandle) {
  WindowData* wd = malloc(sizeof(WindowData));
  if(!wd) return;

  CreateStandardNoteComponents(wd, hInstance);
  if(wd->handle == NULL) {
    free(wd);
    return;
  }
  if(wd->textHandle == NULL) {
    // TODO
    // Delete window for now
    DestroyWindow(wd->handle);
    return;
  }

  char filenameBuffer[FILENAME_SIZE + 1];
  sprintf(filenameBuffer,"%lld.hnote",(long long)GetTickCount64());
  filenameBuffer[FILENAME_SIZE] = '\0';

  // Set path and file name
  sprintf(wd->filepath,"%s\\%s",notePath,filenameBuffer);

  // Register user data pointer
  SetWindowLongPtr(wd->handle,GWLP_USERDATA,(LONG_PTR)wd);

  // Show and focus on new note
  ShowWindow(wd->handle,SW_SHOWNORMAL);
  SetForegroundWindow(wd->handle);
  SetFocus(wd->textHandle);

  Note* n = Push((Note){.opened = 1, .changes = 1, .id = NOTEID++});
  memcpy(n->filename,filenameBuffer,FILENAME_SIZE + 1);
  n->handle = wd->handle;
  memcpy(n->preview,EMPTYNOTE_STRING,sizeof(EMPTYNOTE_STRING));

  wd->id = n->id;

  int index = SendMessage(listHandle,LB_ADDSTRING,0,(LPARAM)n->preview);
  SendMessage(listHandle,LB_SETITEMDATA,index,(LPARAM)n->id);
}

void OpenBySelection(HWND listHandle) {
  int selectedIndex = (int)SendMessage(listHandle,LB_GETCURSEL,0,0);
  if(selectedIndex == LB_ERR) return; // non selected

  int id = SendMessage(listHandle,LB_GETITEMDATA,selectedIndex,0);
  Note* note = Find(id);
  if(!note) return;
  
  char fullPath[MAX_PATH];
  sprintf(fullPath,"%s\\%s\0",NOTESPATH,note->filename);

  OpenNoteFromList(fullPath,GetModuleHandle(NULL),listHandle,selectedIndex);
}

/** Windows Event Handler  */
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch(uMsg) {
    case WM_CLOSE: {
      // Write all open notes with changes to disk
      for(int i = 0; i < noteArray.size; ++i) {
        Note* note = &noteArray.data[i];
        if(note->changes) {
          WindowData* wd = (WindowData*)GetWindowLongPtr(note->handle,GWLP_USERDATA);
          WriteNoteToDisk(wd->filepath,wd->textHandle,note);
          DestroyWindow(note->handle);
        }
      }
      DestroyWindow(hwnd);
    } return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;

    case WM_SIZE: { // Window resize event
      // Get new window dimensions
      int width = LOWORD(lParam);
      int height = HIWORD(lParam);

      // Reposition main components
      if(MAIN_NOTELIST_HANDLE) {
        int newWidth = width - STD_BUTTONWIDTH - FISSURE*2;
        SetWindowPos(
          MAIN_NOTELIST_HANDLE,NULL,
          0,0,newWidth,STD_MAIN_WINDOWHEIGHT,
          SWP_NOZORDER
        );
      }

      int newStart = width - STD_BUTTONWIDTH - FISSURE;

      if(MAIN_CREATEBUTTON_HANDLE) {
        SetWindowPos(
          MAIN_CREATEBUTTON_HANDLE,NULL,
          newStart,0,STD_BUTTONWIDTH,STD_BUTTONHEIGHT,
          SWP_NOZORDER
        );
      }
      if(MAIN_OPENBUTTON_HANDLE) {
        SetWindowPos(
          MAIN_OPENBUTTON_HANDLE,NULL,
          newStart,STD_BUTTONHEIGHT + (FISSURE / 2),STD_BUTTONWIDTH,STD_BUTTONHEIGHT,
          SWP_NOZORDER
        );
      }
      if(MAIN_DELETEBUTTON_HANDLE) {
        SetWindowPos(
          MAIN_DELETEBUTTON_HANDLE,NULL,
          newStart,(STD_BUTTONHEIGHT * 2) + FISSURE,STD_BUTTONWIDTH,STD_BUTTONHEIGHT,
          SWP_NOZORDER
        );
      }

    } return 0;

    case WM_COMMAND: 
    {
      int wmId = LOWORD(wParam);
      if(wmId == MAIN_CREATEBUTTON_ID) {
        CreateNewNoteAndAddToList(NOTESPATH,GetModuleHandle(NULL),MAIN_NOTELIST_HANDLE);
        return 0;
      }
      else if(wmId == MAIN_OPENBUTTON_ID) {
        OpenBySelection(MAIN_NOTELIST_HANDLE);
      }
      else if(wmId == MAIN_DELETEBUTTON_ID) {
        int selectedIndex = (int)SendMessage(MAIN_NOTELIST_HANDLE,LB_GETCURSEL,0,0);
        if(selectedIndex == LB_ERR) break;
        
        int id = SendMessage(MAIN_NOTELIST_HANDLE,LB_GETITEMDATA,selectedIndex,0);
        Note* note = Find(id);
        if(!note) assert(false && "this should not have happened");

        DeleteNoteFromDisk(NOTESPATH,note->filename);
        DeleteNote(note);
      }

      if(wmId == MAIN_NOTELIST_ID && HIWORD(wParam) == LBN_DBLCLK) {
        // Open on double click
        OpenBySelection(MAIN_NOTELIST_HANDLE);
      }
    } break;
  }

  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

void RegisterClasses(HINSTANCE hInstance) {
  // Register main window class
  WNDCLASS wcMain = {};
  wcMain.lpfnWndProc = MainWindowProc;
  wcMain.hInstance = hInstance;
  wcMain.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(HOSE_ICON));
  wcMain.lpszClassName = MAINCLASSNAME;
  wcMain.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcMain.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClass(&wcMain);

  // Register note window class
  WNDCLASS wcNote = {0};
  wcNote.lpfnWndProc = NoteWindowProc;
  wcNote.hInstance = hInstance;
  wcNote.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(HOSE_ICON));
  wcNote.lpszClassName = NOTESCLASSNAME;
  wcNote.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcNote.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClass(&wcNote);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  PrepareNotesPath();
  RegisterClasses(hInstance);

  // Create the main window
  HWND mainHandle = CreateWindowEx(
    0,                                            // Extra Styles
    MAINCLASSNAME,                                // Window class name
    "Hose",                                       // Title
    WS_OVERLAPPEDWINDOW,                          // Style flags (behavior & appearance)
    CW_USEDEFAULT, CW_USEDEFAULT,                 // Coords of window (depends on type of window)
    STD_MAIN_WINDOWWIDTH, STD_MAIN_WINDOWHEIGHT,  // width and height of window
    NULL,                                         // Parent window handle
    NULL,                                         // Menu handle OR child id
    hInstance,                                    // Application instance handle
    NULL                                          // Pointer to window user data
  );

  if(mainHandle == NULL) return 1;

  int xStart = (STD_MAIN_WINDOWWIDTH-STD_BUTTONWIDTH-FISSURE);

  // New note button
  MAIN_CREATEBUTTON_HANDLE = CreateWindowEx(
    0,"BUTTON","Create New",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
    xStart,0,STD_BUTTONWIDTH,STD_BUTTONHEIGHT, 
    mainHandle,(HMENU)MAIN_CREATEBUTTON_ID,hInstance,NULL
  );
  
  // Open note button
  MAIN_OPENBUTTON_HANDLE = CreateWindowEx(
    0,"BUTTON","Open Selected",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
    xStart,STD_BUTTONHEIGHT + (FISSURE / 2),STD_BUTTONWIDTH,STD_BUTTONHEIGHT,
    mainHandle,(HMENU)MAIN_OPENBUTTON_ID,hInstance,NULL
  );

  // Delete note button
  MAIN_DELETEBUTTON_HANDLE = CreateWindowEx(
    0,"BUTTON","Del Selected",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
    xStart,(STD_BUTTONHEIGHT * 2) + FISSURE,STD_BUTTONWIDTH,STD_BUTTONHEIGHT,
    mainHandle,(HMENU)MAIN_DELETEBUTTON_ID,hInstance,NULL
  );

  // List of notes
  MAIN_NOTELIST_HANDLE = CreateWindowEx(
    0,"LISTBOX",NULL,
    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
    0,0,(STD_MAIN_WINDOWWIDTH-STD_BUTTONWIDTH-FISSURE*2),STD_MAIN_WINDOWHEIGHT,
    mainHandle,(HMENU)MAIN_NOTELIST_ID,hInstance,NULL
  );

  FindNotesFromDisk(NOTESPATH);

  ShowWindow(mainHandle,nCmdShow);

  // Program loop
  MSG msg = {0};
  while(GetMessage(&msg,NULL,0,0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg); // Deliver message to event handler
  }

  return 0;
}