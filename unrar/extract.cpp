//
//	11/1/05 gmilow - Modified 
#include "stdafx.h"
#include "rar.hpp"

CmdExtract::CmdExtract(vector<string> *pvRarFiles, string sParfileName)
{
	m_pvRarFiles = pvRarFiles;
	m_sParfileName = sParfileName;
	TotalFileCount=0;
	*Password=0;
	Unp=new Unpack(sParfileName,&DataIO);
	Unp->Init(NULL);
}


CmdExtract::~CmdExtract()
{
	delete Unp;
	memset(Password,0,sizeof(Password));
}


void CmdExtract::DoExtract(CommandData *Cmd)
{
	DataIO.SetCurrentCommand(*Cmd->Command);

	struct FindData FD;
	while (Cmd->GetArcName(ArcName,ArcNameW,sizeof(ArcName)))
		if (FindFile::FastFind(ArcName,ArcNameW,&FD))
			DataIO.TotalArcSize+=FD.Size;
	Cmd->ArcNames->Rewind();
	while (Cmd->GetArcName(ArcName,ArcNameW,sizeof(ArcName)))
	{
		while (ExtractArchive(Cmd)==EXTRACT_ARC_REPEAT)
			;
		if (FindFile::FastFind(ArcName,ArcNameW,&FD))
			DataIO.ProcessedArcSize+=FD.Size;
	}

	if (TotalFileCount==0 && *Cmd->Command!='I')
	{
		mprintf(m_sParfileName,St(MExtrNoFiles));
		ErrHandler.SetErrorCode(WARNING);
	}
#ifndef GUI
	else
		if (*Cmd->Command=='I')
			mprintf(m_sParfileName,St(MDone));
		else
			if (ErrHandler.GetErrorCount()==0)
				mprintf(m_sParfileName,St(MExtrAllOk));
			else
				mprintf(m_sParfileName,St(MExtrTotalErr),ErrHandler.GetErrorCount());
#endif
}


void CmdExtract::ExtractArchiveInit(CommandData *Cmd,Archive &Arc)
{
	DataIO.UnpArcSize=Arc.FileLength();

	FileCount=0;
	MatchedArgs=0;
#ifndef SFX_MODULE
	FirstFile=true;
#endif

	if (*Cmd->Password!=0)
		strcpy(Password,Cmd->Password);
	PasswordAll=(*Cmd->Password!=0);

	DataIO.UnpVolume=false;

	PrevExtracted=false;
	SignatureFound=false;
	AllMatchesExact=true;
	ReconstructDone=false;
}


EXTRACT_ARC_CODE CmdExtract::ExtractArchive(CommandData *Cmd)
{
	Archive Arc(m_pvRarFiles, m_sParfileName, Cmd);
	if (!Arc.WOpen(ArcName,ArcNameW))
	{
		ErrHandler.SetErrorCode(OPEN_ERROR);
		return(EXTRACT_ARC_NEXT);
	}

	if (!Arc.IsArchive(true))
	{
#ifndef GUI
		mprintf(m_sParfileName,St(MNotRAR),ArcName);
#endif
		if (CmpExt(ArcName,"rar"))
			ErrHandler.SetErrorCode(WARNING);
		return(EXTRACT_ARC_NEXT);
	}

	if (!Arc.IsOpened())
		return(EXTRACT_ARC_NEXT);

#ifndef SFX_MODULE
	if (Arc.Volume && Arc.NotFirstVolume)
	{
		char FirstVolName[NM];

		VolNameToFirstName(Arc.m_pvRarFiles, Arc.m_sParfileName,ArcName,FirstVolName,(Arc.NewMhd.Flags & MHD_NEWNUMBERING));
		if (stricomp(ArcName,FirstVolName)!=0 && FileExist(FirstVolName) &&
			Cmd->ArcNames->Search(FirstVolName,NULL,false))
			return(EXTRACT_ARC_NEXT);
	}
#endif
	ExtractArchiveInit(Cmd,Arc);

	if (*Cmd->Command=='T' || *Cmd->Command=='I')
		Cmd->Test=true;

#ifndef GUI
	if (*Cmd->Command=='I')
		Cmd->DisablePercentage=true;
	else
		if (Cmd->Test)
			mprintf(m_sParfileName,St(MExtrTest),ArcName);
		else
			mprintf(m_sParfileName,St(MExtracting),ArcName);
#endif

	Arc.ViewComment();

	while (1)
	{
		int Size=Arc.ReadHeader();
		bool Repeat=false;
		if (!ExtractCurrentFile(Cmd,Arc,Size,Repeat))
			if (Repeat)
			{
				return(EXTRACT_ARC_REPEAT);
			}
			else
				break;
	}
	return(EXTRACT_ARC_NEXT);
}


bool CmdExtract::ExtractCurrentFile(CommandData *Cmd,Archive &Arc,int HeaderSize,bool &Repeat)
{
	char Command=*Cmd->Command;
	if (HeaderSize<=0)
		if (DataIO.UnpVolume)
		{
#ifdef NOVOLUME
			return(false);
#else
			if (!MergeArchive(Arc,NULL,false,Command))
			{
				ErrHandler.SetErrorCode(WARNING);
				return(false);
			}
			SignatureFound=false;
#endif
		}
		else
			return(false);
	int HeadType=Arc.GetHeaderType();
	if (HeadType!=FILE_HEAD)
	{
		if (HeadType==AV_HEAD || HeadType==SIGN_HEAD)
			SignatureFound=true;
#if !defined(SFX_MODULE) && !defined(_WIN_CE)
		if (HeadType==SUB_HEAD && PrevExtracted)
			SetExtraInfo(Cmd,Arc,DestFileName,*DestFileNameW ? DestFileNameW:NULL);
#endif
		if (HeadType==NEWSUB_HEAD)
		{
			if (Arc.SubHead.CmpName(SUBHEAD_TYPE_AV))
				SignatureFound=true;
#if !defined(NOSUBBLOCKS) && !defined(_WIN_CE)
			if (PrevExtracted)
				SetExtraInfoNew(Cmd,Arc,DestFileName,*DestFileNameW ? DestFileNameW:NULL);
#endif
		}
		if (HeadType==ENDARC_HEAD)
			if (Arc.EndArcHead.Flags & EARC_NEXT_VOLUME)
			{
#ifndef NOVOLUME
				if (!MergeArchive(Arc,NULL,false,Command))
				{
					ErrHandler.SetErrorCode(WARNING);
					return(false);
				}
				SignatureFound=false;
#endif
				Arc.Seek(Arc.CurBlockPos,SEEK_SET);
				return(true);
			}
			else
				return(false);
		Arc.SeekToNext();
		return(true);
	}
	PrevExtracted=false;

	if (SignatureFound ||
		!Cmd->Recurse && MatchedArgs>=Cmd->FileArgs->ItemsCount() &&
		AllMatchesExact)
		return(false);

	char ArcFileName[NM];
	IntToExt(Arc.NewLhd.FileName,Arc.NewLhd.FileName);
	strcpy(Arc.ArcName,Cmd->ArcName);
	strcpy(ArcFileName,Cmd->ArcName);

	wchar ArcFileNameW[NM];
	*ArcFileNameW=0;

	bool EqualNames=false;
	int MatchNumber=Cmd->IsProcessFile(Arc.NewLhd,&EqualNames);
	bool ExactMatch=MatchNumber!=0;
#if !defined(SFX_MODULE) && !defined(_WIN_CE)
	if (Cmd->ExclPath==EXCL_BASEPATH)
	{
		*Cmd->ArcPath=0;
		if (ExactMatch)
		{
			Cmd->FileArgs->Rewind();
			if (Cmd->FileArgs->GetString(Cmd->ArcPath,NULL,sizeof(Cmd->ArcPath),MatchNumber-1))
				*PointToName(Cmd->ArcPath)=0;
		}
	}
#endif
	if (ExactMatch && !EqualNames)
		AllMatchesExact=false;

#ifdef UNICODE_SUPPORTED
	bool WideName=(Arc.NewLhd.Flags & LHD_UNICODE) && UnicodeEnabled();
#else
	bool WideName=false;
#endif

#ifdef _APPLE
	if (WideName)
	{
		WideToUtf(Arc.NewLhd.FileNameW,ArcFileName,sizeof(ArcFileName));
		WideName=false;
	}
#endif

	wchar *DestNameW=WideName ? DestFileNameW:NULL;

#ifdef UNICODE_SUPPORTED
	if (WideName)
	{
		ConvertPath(Arc.NewLhd.FileNameW,ArcFileNameW);
		char Name[NM];
		WideToChar(ArcFileNameW,Name);
		if (IsNameUsable(Name))
			strcpy(ArcFileName,Name);
	}
#endif

	ConvertPath(ArcFileName,ArcFileName);

	if (Arc.IsArcLabel())
		return(true);

	if (Arc.NewLhd.Flags & LHD_VERSION)
	{
		if (Cmd->VersionControl!=1 && !EqualNames)
		{
			if (Cmd->VersionControl==0)
				ExactMatch=false;
			int Version=ParseVersionFileName(ArcFileName,ArcFileNameW,false);
			if (Cmd->VersionControl-1==Version)
				ParseVersionFileName(ArcFileName,ArcFileNameW,true);
			else
				ExactMatch=false;
		}
	}
	else
		if (!Arc.IsArcDir() && Cmd->VersionControl>1)
			ExactMatch=false;

	Arc.ConvertAttributes();

	strcpy(ArcName,  Arc.FileName);
#ifndef SFX_MODULE
	if ((Arc.NewLhd.Flags & (LHD_SPLIT_BEFORE/*|LHD_SOLID*/)) && FirstFile)
	{
		char CurVolName[NM];
		strcpy(CurVolName,ArcName);

		VolNameToFirstName(Arc.m_pvRarFiles, Arc.m_sParfileName, ArcName,ArcName,(Arc.NewMhd.Flags & MHD_NEWNUMBERING));
		if (!FileExist(ArcName))
		{
			*ArcNameW=0;
			Repeat=true;
			return(false);
		}
#if !defined(RARDLL) && !defined(_WIN_CE)
		if (!ReconstructDone)
		{
			ReconstructDone=true;

			RecVolumes RecVol(m_pvRarFiles, m_sParfileName);
			if (RecVol.Restore(Cmd,Arc.FileName,Arc.FileNameW,true))
			{
				Repeat=true;
				return(false);
			}
		}
#endif
		strcpy(ArcName,CurVolName);
	}
#endif
	DataIO.UnpVolume=(Arc.NewLhd.Flags & LHD_SPLIT_AFTER);
	DataIO.NextVolumeMissing=false;

	Arc.Seek(Arc.NextBlockPos-Arc.NewLhd.FullPackSize,SEEK_SET);

	bool TestMode=false;
	bool ExtrFile=false;
	bool SkipSolid=false;

#ifndef SFX_MODULE
	if (FirstFile && (ExactMatch || Arc.Solid) && (Arc.NewLhd.Flags & (LHD_SPLIT_BEFORE/*|LHD_SOLID*/))!=0)
	{
		if (ExactMatch)
		{
			Log(m_sParfileName,Arc.FileName,St(MUnpCannotMerge),ArcFileName);
#ifdef RARDLL
			Cmd->DllError=ERAR_BAD_DATA;
#endif
		}
		ExactMatch=false;
	}

	FirstFile=false;
#endif

	if (ExactMatch || (SkipSolid=Arc.Solid)!=0)
	{
		if (Arc.NewLhd.Flags & LHD_PASSWORD)
#ifndef RARDLL
			if (*Password==0)
#endif
			{
#ifdef RARDLL
				if (*Cmd->Password==0)
					if (Cmd->Callback==NULL ||
						Cmd->Callback(UCM_NEEDPASSWORD,Cmd->UserData,(LONG)Cmd->Password,sizeof(Cmd->Password))==-1)
						return(false);
				strcpy(Password,Cmd->Password);

#else
				if (!GetPassword(m_sParfileName, PASSWORD_FILE,ArcFileName,Password,sizeof(Password)))
				{
					return(false);
				}
#endif
			}
#if !defined(GUI) && !defined(SILENT)
			else
				if (!PasswordAll && (!Arc.Solid || Arc.NewLhd.UnpVer>=20 && (Arc.NewLhd.Flags & LHD_SOLID)==0))
				{
					eprintf(m_sParfileName, St(MUseCurPsw),ArcFileName);
					switch(Cmd->AllYes ? 1:Ask(m_pvRarFiles, m_sParfileName,St(MYesNoAll)))
					{
					case -1:
						ErrHandler.Exit(USER_BREAK);
					case 2:
						if (!GetPassword(m_sParfileName, PASSWORD_FILE,ArcFileName,Password,sizeof(Password)))
						{
							return(false);
						}
						break;
					case 3:
						PasswordAll=true;
						break;
					}
				}
#endif

#ifndef SFX_MODULE
				if (*Cmd->ExtrPath==0 && *Cmd->ExtrPathW!=0)
					WideToChar(Cmd->ExtrPathW,DestFileName);
				else
#endif
					strcpy(DestFileName,Cmd->ExtrPath);


#ifndef SFX_MODULE
				if (Cmd->AppendArcNameToPath)
				{
					strcat(DestFileName,PointToName(Arc.FileName));
					SetExt(DestFileName,NULL);
					AddEndSlash(DestFileName);
				}
#endif

				char *ExtrName=ArcFileName;

#ifndef SFX_MODULE
				int Length=strlen(Cmd->ArcPath);
				if (Length>0 && strnicomp(Cmd->ArcPath,ArcFileName,Length)==0)
				{
					ExtrName+=Length;
					while (*ExtrName==CPATHDIVIDER)
						ExtrName++;
				}
#endif

				bool AbsPaths=Cmd->ExclPath==EXCL_ABSPATH && Command=='X' && IsDriveDiv(':');
				if (AbsPaths)
					*DestFileName=0;

				if (Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
					strcat(DestFileName,PointToName(ExtrName));
				else
					strcat(DestFileName,ExtrName);

				if (AbsPaths && DestFileName[1]=='_' && IsPathDiv(DestFileName[2]))
					DestFileName[1]=':';

#ifndef SFX_MODULE
				if (!WideName && *Cmd->ExtrPathW!=0)
				{
					DestNameW=DestFileNameW;
					WideName=true;
					CharToWide(ArcFileName,ArcFileNameW);
				}
#endif

				if (WideName)
				{
					if (*Cmd->ExtrPathW!=0)
						strcpyw(DestFileNameW,Cmd->ExtrPathW);
					else
						CharToWide(Cmd->ExtrPath,DestFileNameW);

#ifndef SFX_MODULE
					if (Cmd->AppendArcNameToPath)
					{
						wchar FileNameW[NM];
						if (*Arc.FileNameW!=0)
							strcpyw(FileNameW,Arc.FileNameW);
						else
							CharToWide(Arc.FileName,FileNameW);
						strcatw(DestFileNameW,PointToName(FileNameW));
						SetExt(DestFileNameW,NULL);
						AddEndSlash(DestFileNameW);
					}
#endif
					wchar *ExtrNameW=ArcFileNameW;
#ifndef SFX_MODULE
					if (Length>0)
					{
						wchar ArcPathW[NM];
						CharToWide(Cmd->ArcPath,ArcPathW);
						Length=strlenw(ArcPathW);
					}
					ExtrNameW+=Length;
					while (*ExtrNameW==CPATHDIVIDER)
						ExtrNameW++;
#endif

					if (AbsPaths)
						*DestFileNameW=0;

					if (Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
						strcatw(DestFileNameW,PointToName(ExtrNameW));
					else
						strcatw(DestFileNameW,ExtrNameW);

					if (AbsPaths && DestFileNameW[1]=='_' && IsPathDiv(DestFileNameW[2]))
						DestFileNameW[1]=':';
				}
				else
					*DestFileNameW=0;

				ExtrFile=!SkipSolid/* && *ExtrName*/;
				if ((Cmd->FreshFiles || Cmd->UpdateFiles) && (Command=='E' || Command=='X'))
				{
					struct FindData FD;
					if (FindFile::FastFind(DestFileName,DestNameW,&FD))
					{
						if (FD.mtime >= Arc.NewLhd.mtime)
							ExtrFile=false;
					}
					else
						if (Cmd->FreshFiles)
							ExtrFile=false;
				}

#ifdef RARDLL
				if (*Cmd->DllDestName)
				{
					strncpy(DestFileName,Cmd->DllDestName,sizeof(DestFileName));
					*DestFileNameW=0;
					if (Cmd->DllOpMode!=RAR_EXTRACT)
						ExtrFile=false;
				}
				if (*Cmd->DllDestNameW)
				{
					strncpyw(DestFileNameW,Cmd->DllDestNameW,sizeof(DestFileNameW)/sizeof(DestFileNameW[0]));
					DestNameW=DestFileNameW;
					if (Cmd->DllOpMode!=RAR_EXTRACT)
						ExtrFile=false;
				}
#endif

#ifdef SFX_MODULE
				if (Arc.NewLhd.UnpVer!=UNP_VER && Arc.NewLhd.Method!=0x30)
#else
				if (Arc.NewLhd.UnpVer<13 || Arc.NewLhd.UnpVer>UNP_VER)
#endif
				{
#ifndef SILENT
					Log(Arc.m_sParfileName, Arc.FileName,St(MUnknownMeth),ArcFileName);
#ifndef SFX_MODULE
					Log(Arc.m_sParfileName, Arc.FileName, St(MVerRequired), Arc.NewLhd.UnpVer/10, Arc.NewLhd.UnpVer%10);
#endif
#endif
					ExtrFile=false;
					ErrHandler.SetErrorCode(WARNING);
#ifdef RARDLL
					Cmd->DllError=ERAR_UNKNOWN_FORMAT;
#endif
				}

				File CurFile(m_pvRarFiles, m_sParfileName);

				if (!IsLink(Arc.NewLhd.FileAttr))
					if (Arc.IsArcDir())
					{
						if (!ExtrFile || Command=='P' || Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
							return(true);
						if (SkipSolid)
						{
#ifndef GUI
							mprintf(m_sParfileName,St(MExtrSkipFile),ArcFileName);
#endif
							return(true);
						}
						TotalFileCount++;
						if (Cmd->Test)
						{
#ifndef GUI
							mprintf(m_sParfileName,St(MExtrTestFile),ArcFileName);
							mprintf(m_sParfileName," %s",St(MOk));
#endif
							return(true);
						}
						MKDIR_CODE MDCode=MakeDir(DestFileName,DestNameW,Arc.NewLhd.FileAttr);
						bool DirExist=false;
						if (MDCode!=MKDIR_SUCCESS)
						{
							DirExist=FileExist(DestFileName,DestNameW);
							if (DirExist && !IsDir(GetFileAttr(DestFileName,DestNameW)))
							{
								bool UserReject;
								FileCreate(m_pvRarFiles, m_sParfileName,Cmd,NULL,DestFileName,DestNameW,Cmd->Overwrite,&UserReject,Arc.NewLhd.UnpSize,Arc.NewLhd.FileTime);
								DirExist=false;
							}
							CreatePath(m_sParfileName, DestFileName,DestNameW,true);
							MDCode=MakeDir(DestFileName,DestNameW,Arc.NewLhd.FileAttr);
						}
						if (MDCode==MKDIR_SUCCESS)
						{
#ifndef GUI
							mprintf(m_sParfileName,St(MCreatDir),DestFileName);
							mprintf(m_sParfileName," %s",St(MOk));
#endif
							PrevExtracted=true;
						}
						else
							if (DirExist)
							{
								SetFileAttr(DestFileName,DestNameW,Arc.NewLhd.FileAttr);
								PrevExtracted=true;
							}
							else
							{
								Log(m_sParfileName,Arc.FileName,St(MExtrErrMkDir),DestFileName);
								ErrHandler.SysErrMsg(Arc.m_sParfileName);
#ifdef RARDLL
								Cmd->DllError=ERAR_ECREATE;
#endif
								ErrHandler.SetErrorCode(CREATE_ERROR);
							}
							if (PrevExtracted)
							{
#if defined(_WIN_32) && !defined(_WIN_CE) && !defined(SFX_MODULE)
								if (Cmd->SetCompressedAttr &&
									(Arc.NewLhd.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0 && WinNT())
									SetFileCompression(DestFileName,DestFileNameW,true);
#endif
								SetDirTime(DestFileName,
									Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.NewLhd.mtime,
									Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.NewLhd.ctime,
									Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.NewLhd.atime);
							}
							return(true);
					}
					else
					{
						if (Cmd->Test && ExtrFile)
							TestMode=true;
#if !defined(GUI) && !defined(SFX_MODULE)
						if (Command=='P' && ExtrFile)
							CurFile.SetHandleType(FILE_HANDLESTD);
#endif
						if ((Command=='E' || Command=='X') && ExtrFile && !Cmd->Test)
						{
							bool UserReject;
							if (!FileCreate(m_pvRarFiles, m_sParfileName,Cmd,&CurFile,DestFileName,DestNameW,Cmd->Overwrite,&UserReject,Arc.NewLhd.UnpSize,Arc.NewLhd.FileTime))
							{
								ExtrFile=false;
								if (!UserReject)
								{
									ErrHandler.CreateErrorMsg(m_sParfileName, Arc.FileName,DestFileName);
									ErrHandler.SetErrorCode(CREATE_ERROR);
#ifdef RARDLL
									Cmd->DllError=ERAR_ECREATE;
#endif
									if (!IsNameUsable(DestFileName))
									{
										Log(Arc.m_sParfileName,Arc.FileName,St(MCorrectingName));
										MakeNameUsable(DestFileName,true);
										CreatePath(m_sParfileName, DestFileName,NULL,true);
										if (FileCreate(m_pvRarFiles, m_sParfileName,Cmd,&CurFile,DestFileName,NULL,Cmd->Overwrite,&UserReject,Arc.NewLhd.FullUnpSize,Arc.NewLhd.FileTime))
											ExtrFile=true;
										else
											ErrHandler.CreateErrorMsg(m_sParfileName, Arc.FileName,DestFileName);
									}
								}
							}
						}
					}

					if (!ExtrFile && Arc.Solid)
					{
						SkipSolid=true;
						TestMode=true;
						ExtrFile=true;
					}
					if (ExtrFile)
					{
						if (!SkipSolid)
						{
							if (!TestMode && Command!='P' && CurFile.IsDevice())
							{
								Log(m_sParfileName,Arc.FileName,St(MInvalidName),DestFileName);
								ErrHandler.WriteError(Arc.m_sParfileName,Arc.FileName,DestFileName);
							}
							TotalFileCount++;
						}
						FileCount++;
#ifndef GUI
						if (Command!='I')
							if (SkipSolid)
								mprintf(m_sParfileName,St(MExtrSkipFile),ArcFileName);
							else
								switch(Cmd->Test ? 'T':Command)
							{
								case 'T':
									mprintf(m_sParfileName,St(MExtrTestFile),ArcFileName);
									break;
#ifndef SFX_MODULE
								case 'P':
									mprintf(m_sParfileName,St(MExtrPrinting),ArcFileName);
									break;
#endif
								case 'X':
								case 'E':
									mprintf(m_sParfileName,St(MExtrFile),DestFileName);
									break;
							}
#endif
							DataIO.CurUnpRead=0;
							DataIO.CurUnpWrite=0;
							DataIO.UnpFileCRC=Arc.OldFormat ? 0 : 0xffffffff;
							DataIO.PackedCRC=0xffffffff;
							DataIO.SetEncryption(
								(Arc.NewLhd.Flags & LHD_PASSWORD) ? Arc.NewLhd.UnpVer:0,Password,
								(Arc.NewLhd.Flags & LHD_SALT) ? Arc.NewLhd.Salt:NULL,false);
							DataIO.SetPackedSizeToRead(Arc.NewLhd.FullPackSize);
							DataIO.SetFiles(&Arc,&CurFile);
							DataIO.SetTestMode(TestMode);
							DataIO.SetSkipUnpCRC(SkipSolid);
#ifndef _WIN_CE
							if (!TestMode && !Arc.BrokenFileHeader &&
								(Arc.NewLhd.FullPackSize<<11)>Arc.NewLhd.FullUnpSize &&
								(Arc.NewLhd.FullUnpSize<100000000 || Arc.FileLength()>Arc.NewLhd.FullPackSize))
								CurFile.Prealloc(Arc.NewLhd.FullUnpSize);
#endif

							CurFile.SetAllowDelete(!Cmd->KeepBroken);

							if (!ExtractLink(DataIO,Arc,DestFileName,DataIO.UnpFileCRC,Command=='X' || Command=='E') &&
								(Arc.NewLhd.Flags & LHD_SPLIT_BEFORE)==0)
								if (Arc.NewLhd.Method==0x30)
									UnstoreFile(DataIO,Arc.NewLhd.FullUnpSize);
								else
								{
									Unp->SetDestSize(Arc.NewLhd.FullUnpSize);
#ifndef SFX_MODULE
									if (Arc.NewLhd.UnpVer<=15)
										Unp->DoUnpack(15,FileCount>1 && Arc.Solid);
									else
#endif
										Unp->DoUnpack(Arc.NewLhd.UnpVer,Arc.NewLhd.Flags & LHD_SOLID);
								}

								if (Arc.IsOpened())
									Arc.SeekToNext();

								bool BrokenFile=false;
								if (!SkipSolid)
								{
									if (Arc.OldFormat && UINT32(DataIO.UnpFileCRC)==UINT32(Arc.NewLhd.FileCRC) ||
										!Arc.OldFormat && UINT32(DataIO.UnpFileCRC)==UINT32(Arc.NewLhd.FileCRC^0xffffffff))
									{
#ifndef GUI
										if (Command!='P' && Command!='I')
											mprintf(m_sParfileName,"%s",false ? " ": St(MOk));
#endif
									}
									else
									{
										char *BadArcName=(Arc.NewLhd.Flags & LHD_SPLIT_BEFORE) ? NULL:Arc.FileName;
										if (Arc.NewLhd.Flags & LHD_PASSWORD)
										{
											Log(m_sParfileName, BadArcName,St(MEncrBadCRC),ArcFileName);
										}
										else
										{
											Log(m_sParfileName, BadArcName,St(MCRCFailed),ArcFileName);
										}
										BrokenFile=true;										
										ErrHandler.SetErrorCode(CRC_ERROR);
#ifdef RARDLL
										Cmd->DllError=ERAR_BAD_DATA;
#endif
										Alarm();
									}
								}
#ifndef GUI
								else
									mprintf(m_sParfileName,"\b\b\b\b\b     ");
#endif

								if (!TestMode && (Command=='X' || Command=='E') &&
									!IsLink(Arc.NewLhd.FileAttr))
								{
#if defined(_WIN_32) || defined(_EMX)
									if (Cmd->ClearArc)
										Arc.NewLhd.FileAttr&=~FA_ARCH;
#endif
									if (!BrokenFile || Cmd->KeepBroken)
									{
										if (BrokenFile)
											CurFile.Truncate();
										CurFile.SetOpenFileStat(
											Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.NewLhd.mtime,
											Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.NewLhd.ctime,
											Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.NewLhd.atime);
										CurFile.Close();
#if defined(_WIN_32) && !defined(_WIN_CE) && !defined(SFX_MODULE)
										if (Cmd->SetCompressedAttr &&
											(Arc.NewLhd.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0 && WinNT())
											SetFileCompression(CurFile.FileName,CurFile.FileNameW,true);
#endif
										CurFile.SetCloseFileStat(
											Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.NewLhd.mtime,
											Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.NewLhd.atime,
											Arc.NewLhd.FileAttr);
										PrevExtracted=true;
									}
								}
					}
	}
	if (ErrHandler.GetErrorCount() > 0)
		ErrHandler.Throw(ErrHandler.GetErrorCode());

	if (ExactMatch)
		MatchedArgs++;
	if (DataIO.NextVolumeMissing || !Arc.IsOpened())
		return(false);
	if (!ExtrFile)
		if (!Arc.Solid)
			Arc.SeekToNext();
		else
			if (!SkipSolid)
				return(false);
	return(true);
}


void CmdExtract::UnstoreFile(ComprDataIO &DataIO,Int64 DestUnpSize)
{
	Array<byte> Buffer("", 0x10000);
	while (1)
	{
		unsigned int Code=DataIO.UnpRead(&Buffer[0],Buffer.Size());
		if (Code==0 || (int)Code==-1)
			break;
		Code=Code<DestUnpSize ? Code:int64to32(DestUnpSize);
		DataIO.UnpWrite(&Buffer[0],Code);
		if (DestUnpSize>=0)
			DestUnpSize-=Code;
	}
}
