#ifndef _UNRAR_DLL_
#define _UNRAR_DLL_

#define ERAR_END_ARCHIVE     10
#define ERAR_NO_MEMORY       11
#define ERAR_BAD_DATA        12
#define ERAR_BAD_ARCHIVE     13
#define ERAR_UNKNOWN_FORMAT  14
#define ERAR_EOPEN           15
#define ERAR_ECREATE         16
#define ERAR_ECLOSE          17
#define ERAR_EREAD           18
#define ERAR_EWRITE          19
#define ERAR_SMALL_BUF       20
#define ERAR_UNKNOWN         21
#define ERAR_PASSWORD        22

#define RAR_OM_LIST           0
#define RAR_OM_EXTRACT        1

#define RAR_SKIP              0
#define RAR_TEST              1
#define RAR_EXTRACT           2

#define RAR_VOL_ASK           0
#define RAR_VOL_NOTIFY        1

#define RAR_DLL_VERSION       4

#ifdef _UNIX
#define CALLBACK
#define PASCAL
#define LONG long
#define HANDLE void *
#define UINT unsigned int
#endif

struct RARHeaderData
{
  char         ArcName[260];
  char         FileName[260];
  unsigned int Flags;
  unsigned int PackSize;
  unsigned int UnpSize;
  unsigned int HostOS;
  unsigned int FileCRC;
  unsigned int FileTime;
  unsigned int UnpVer;
  unsigned int Method;
  unsigned int FileAttr;
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
};


struct RARHeaderDataEx
{
  char         ArcName[1024];
  wchar_t      ArcNameW[1024];
  char         FileName[1024];
  wchar_t      FileNameW[1024];
  unsigned int Flags;
  unsigned int PackSize;
  unsigned int PackSizeHigh;
  unsigned int UnpSize;
  unsigned int UnpSizeHigh;
  unsigned int HostOS;
  unsigned int FileCRC;
  unsigned int FileTime;
  unsigned int UnpVer;
  unsigned int Method;
  unsigned int FileAttr;
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
  unsigned int Reserved[1024];
};


struct RAROpenArchiveData
{
  char         *ArcName;
  unsigned int OpenMode;
  unsigned int OpenResult;
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
};

struct RAROpenArchiveDataEx
{
  char         *ArcName;
  wchar_t      *ArcNameW;
  unsigned int OpenMode;
  unsigned int OpenResult;
  char         *CmtBuf;
  unsigned int CmtBufSize;
  unsigned int CmtSize;
  unsigned int CmtState;
  unsigned int Flags;
  unsigned int Reserved[32];
};

enum UNRARCALLBACK_MESSAGES {
  UCM_CHANGEVOLUME,UCM_PROCESSDATA,UCM_NEEDPASSWORD
};

typedef int (CALLBACK *UNRARCALLBACK)(UINT msg,LONG UserData,LONG P1,LONG P2);

typedef int (PASCAL *CHANGEVOLPROC)(char *ArcName,int Mode);
typedef int (PASCAL *PROCESSDATAPROC)(unsigned char *Addr,int Size);

#ifdef __cplusplus
extern "C" {
#endif

HANDLE PASCAL RAROpenArchive(vector<string> *pvRarFiles, string sParfileName, struct RAROpenArchiveData *ArchiveData);
HANDLE PASCAL RAROpenArchiveEx(vector<string> *pvRarFiles, string sParfileName, struct RAROpenArchiveDataEx *ArchiveData);
int    PASCAL RARCloseArchive(HANDLE hArcData);
int    PASCAL RARReadHeader(HANDLE hArcData,struct RARHeaderData *HeaderData, string sParfileName);
int    PASCAL RARReadHeaderEx(HANDLE hArcData,struct RARHeaderDataEx *HeaderData, string sParfileName);
int    PASCAL RARProcessFile(HANDLE hArcData,int Operation,char *DestPath,char *DestName, string sParfileName);
int    PASCAL RARProcessFileW(HANDLE hArcData,int Operation,wchar_t *DestPath,wchar_t *DestName);
void   PASCAL RARSetCallback(HANDLE hArcData,UNRARCALLBACK Callback,LONG UserData);
void   PASCAL RARSetChangeVolProc(HANDLE hArcData,CHANGEVOLPROC ChangeVolProc);
void   PASCAL RARSetProcessDataProc(HANDLE hArcData,PROCESSDATAPROC ProcessDataProc);
void   PASCAL RARSetPassword(HANDLE hArcData,char *Password);
int    PASCAL RARGetDllVersion();

#ifdef __cplusplus
}
#endif

#endif
