#include <windows.h>
#include <Commctrl.h>
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <tchar.h>
#include <richedit.h>

#include "resource.h"

#define WINTITLE _TEXT("Mail.ru History Reader")

// ���� ���������
#define TYPE_SMS 0x11
#define TYPE_BIRTHDAY 0x0D // ����������� � ��� ��������

HMENU hMenu;
HWND hwndDlg;
HINSTANCE hInst;
HANDLE hDBSFile=INVALID_HANDLE_VALUE;
HANDLE hDBSMap=NULL;

unsigned char * mra_base = NULL;
unsigned int * offset_table;


#pragma pack(push,1) 
typedef struct _ids{
	unsigned int id1;
	unsigned int id2;
}_ids;

struct _message{
	unsigned int size;
	unsigned int prev_id;
	unsigned int next_id;
	unsigned int xz1;
	FILETIME time;
	unsigned int type_message;
	char flag_incoming;
	char lol[3]; 
	unsigned int count_nick;
	unsigned int magic_num; // 0x38
	unsigned int count_message; // ������ ����������, �� ������ � ������
	unsigned int xz2; // 
	unsigned int size_lps_rtf; // ���� 
	unsigned int xz3; // 
};



typedef struct _email{
	wchar_t *history;
	wchar_t *email;
	//unsigned int size;
	HANDLE hTmpFile;
	_ids *id;
}_email;

typedef struct _emails{
	struct _email *emails;
	unsigned int count_messages;
}_emails;

struct _emails emails = {NULL,0};

/* ��������� ������ "mrahistory_" � unicode */
unsigned char mrahistory[22] = {
	0x6D, 0x00, 0x72, 0x00, 0x61, 0x00, 0x68, 0x00, 0x69, 0x00, 0x73, 0x00, 0x74, 0x00, 0x6F, 0x00, 
	0x72, 0x00, 0x79, 0x00, 0x5F, 0x00
};

#pragma pack(pop)

// ������� ������� ������ �� ������, �� ���������� GNU libc 
void * memmem(const void *buf, const void *pattern, size_t buflen, size_t len)
{
	size_t i, j;
	char *bf = (char *)buf, *pt = (char *)pattern;

	if (len > buflen)
		return (void *)NULL;

	for (i = 0; i <= (buflen - len); ++i)
	{
		for (j = 0; j < len; ++j)
		{
			if (pt[j] != bf[i + j])
				break;
		}
		if (j == len)
			return (bf + i);
	}
	return NULL;
}

void get_history()
{
	unsigned int end_id_mail=*(unsigned int*)(mra_base+44+offset_table[1]);
	unsigned int count_emails=*(unsigned int*)(mra_base+32+offset_table[1]);


	emails.emails=(struct _email *)VirtualAlloc(NULL,count_emails*sizeof(struct _emails),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
	emails.count_messages=0;

	for(unsigned int i=0;i<count_emails;i++) {
		_ids *mail_data=(struct _ids*)(mra_base+offset_table[end_id_mail]+4);
		if(memmem(((unsigned char*)mail_data+0x190), mrahistory,sizeof(mrahistory),sizeof(mrahistory))) {
			emails.emails[emails.count_messages].hTmpFile = NULL;
			emails.emails[emails.count_messages].id=(_ids*)((unsigned char*)mail_data+0x24);
			emails.emails[emails.count_messages].history=emails.emails[emails.count_messages].email=(wchar_t*)mail_data+0xC8+11; //�������� ��������� ����� ����� "mrahistory_"

			while(*emails.emails[emails.count_messages].email++!=0x75);
			emails.emails[emails.count_messages].email[0]=0x00;
			++emails.emails[emails.count_messages].email;

			//emails->emails[emails->count_messages].size=emails->emails[emails->count_messages].email - emails->emails[emails->count_messages].history;

			//while(*emails[k].email++!=0); //����� ������� ��������� email ���������� �� + 2 ����� (��� wchar_t) � ��� �������� ������������ � �����
			emails.count_messages++;
		}
		end_id_mail=mail_data->id2;
	}
}

BOOL CALLBACK MainDialogProc(HWND s_hwndDlg,UINT Message, UINT wParam, LONG lParam);

// ������� �������  
int WINAPI WinMain (HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nShowCmd)   
{  
	UNREFERENCED_PARAMETER(hPrevInstance);  
	UNREFERENCED_PARAMETER(lpCmdLine);

	HMODULE hRe=LoadLibrary(_TEXT("Riched20.dll"));
	InitCommonControls();
	hInst=hInstance;
	DialogBox(hInstance,MAKEINTRESOURCE(IDD_DIALOG1),NULL,MainDialogProc);
	FreeLibrary(hRe);

	return 0;
}

DWORD CALLBACK EditStreamCallback(DWORD_PTR dwCookie, LPBYTE lpBuff,LONG cb, PLONG pcb)
{
	HANDLE hFile = (HANDLE)dwCookie;

	if (ReadFile(hFile, lpBuff, cb, (DWORD *)pcb, NULL)) 
	{
		return 0;
	}

	return -1;
}

BOOL SetListEmails()
{
	LVITEM lvI;
	wchar_t search[512];
	HWND hList=GetDlgItem(hwndDlg,IDC_LIST1);
	unsigned int search_len=GetWindowTextW(GetDlgItem(hwndDlg,IDC_COMBO1),search,sizeof(search))*2;

	memset(&lvI,0,sizeof(LVITEM));

	lvI.mask = LVIF_TEXT | LVIF_PARAM;  

	SendMessage(hList,LVM_DELETEALLITEMS,0,0);

	for(int index  = 0; index < emails.count_messages;index++)
	{
		if(memmem(emails.emails[index].history,search,sizeof(search),search_len))
		{
			lvI.lParam=index;
			lvI.pszText=emails.emails[index].email;
			lvI.iItem=SendMessageW(hList,LVM_INSERTITEMW,0,(LPARAM)&lvI);
		}
	}

	
	lvI.lParam=MF_GRAYED;
	if(lvI.iItem > 0)
	{
		lvI.lParam=MF_ENABLED;
	}
	EnableMenuItem(hMenu,IDM_SAVE_LIST,lvI.lParam);


	return TRUE;

}

BOOL SetComboEmail()
{
	HWND hCombo=GetDlgItem(hwndDlg,IDC_COMBO1);
	SendMessage(hCombo,CB_RESETCONTENT, 0, 0);
	EnableWindow(hCombo,FALSE);

	if(emails.count_messages == 0) return FALSE;

	for(int index  = 0; index < emails.count_messages;index++)
	{
		if(SendMessageW(hCombo,CB_FINDSTRING,-1,(LPARAM)emails.emails[index].history)==CB_ERR)
			SendMessageW(hCombo,CB_ADDSTRING,0,(LPARAM)emails.emails[index].history);
	}

	EnableWindow(hCombo,TRUE);
	SendMessage(hCombo,CB_SETCURSEL,0,0);

	return TRUE;
}

BOOL PrintMessage(HWND hwndDlg,struct _emails * emails,int email_index)
{

	BOOL fSuccess = FALSE;

	if(0 == emails->emails[email_index].id->id1)
	{
		MessageBox(hwndDlg,_TEXT("��� ���������"),NULL,MB_OK|MB_ICONINFORMATION);
		//char text[]="{\\rtf1\\ansi\\pard\\fs100\\par\\par\\qc{\\b TOP SECRET}}";

		//SendMessage(GetDlgItem(hwndDlg,IDC_RICHEDIT21),WM_SETTEXT,0,(LPARAM)text);
		return fSuccess;
	}

	if(NULL == emails->emails[email_index].hTmpFile)
	{
		HANDLE hTmpFile=CreateFile(emails->emails[email_index].email,GENERIC_WRITE|GENERIC_READ,FILE_SHARE_WRITE|FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
		if(INVALID_HANDLE_VALUE != hTmpFile )
		{
			int id_message=emails->emails[email_index].id->id1;
			SYSTEMTIME st; // ��������� ���������� ��� ��������������� FILETIME
			wchar_t buf[128]; // ������ ��� ���� ������� ���� ��� ����������� ������������.
			wchar_t enter[2]={0x0d,0x0a};
			DWORD len;
			do
			{
				_message *mes=(_message *)(mra_base+offset_table[id_message]);

				FileTimeToSystemTime(&mes->time,&st);
				len=wsprintfW(buf,L"%02d.%02d.%04d %02d:%02d (0x%X) > ",st.wDay,st.wMonth,st.wYear,st.wHour,st.wMinute, mes->type_message);
				wprintf(_TEXT("%d\t%s\n"),id_message,buf);


				WriteFile(hTmpFile,buf,len*sizeof(wchar_t),(LPDWORD)&st,NULL); // ��� ��� ��� ������ �� ����� ��������� st �� ����� �������

				wchar_t *str=(wchar_t *)((unsigned char *)mes+sizeof(_message));

				WriteFile(hTmpFile,str,(mes->count_nick - 1)*sizeof(wchar_t),&len,NULL); //����� ���  -1 ��� �� ������ ����������� ����� �� ������
				str+=mes->count_nick; // ���������� ���������, ������ �� ��������� ��������� � unicode

				if(0 == *str && mes->type_message == TYPE_SMS)
				{
					mes->count_message=((*(str+1))/sizeof(wchar_t))+1;
					// ������� ��� ������� �� �������� 2 �����
					str+=3;
				}

				WriteFile(hTmpFile,enter,sizeof(enter),(LPDWORD)&st,NULL);

				WriteFile(hTmpFile,str,(mes->count_message - 1)*sizeof(wchar_t),&len,NULL); //����� ���������
				// str+=mes->count_message; // ������ ��������� ���������� �� LSP RTF, �� ��� ��� �� ���� :)

				WriteFile(hTmpFile,enter,sizeof(enter),(LPDWORD)&st,NULL);
				WriteFile(hTmpFile,enter,sizeof(enter),(LPDWORD)&st,NULL);

				id_message=mes->prev_id;
			} while(id_message);
		}

		emails->emails[email_index].hTmpFile=hTmpFile;
	}

	if(INVALID_HANDLE_VALUE != emails->emails[email_index].hTmpFile )
	{
		SetFilePointer( emails->emails[email_index].hTmpFile, 0, NULL, FILE_BEGIN );

		EDITSTREAM es = { 0 };

		es.pfnCallback = EditStreamCallback;
		es.dwCookie    = (DWORD_PTR)emails->emails[email_index].hTmpFile;


		if (SendDlgItemMessageW(hwndDlg, IDC_RICHEDIT21, EM_STREAMIN, SF_TEXT | SF_UNICODE, (LPARAM)&es) && es.dwError == 0) 
		{
			fSuccess = TRUE;
		}
	}
	else
	{
		// ����������� ������...
		emails->emails[email_index].hTmpFile = NULL;
	}

	return fSuccess;
}

LRESULT SetDefaultText(HWND hRich)
{
	//char text[]="{\\rtf1\\ansi\\ansicpg1251\\deff0\\deflang1033{\\fonttbl{\\f0\\fswiss\\fprq2\\fcharset0 Tahoma;}}\
				\\viewkind4\\uc1\\pard\\par\\qc{\\b\\f0\\fs52 mail.ru}\\fs40  History Reader 3.1\\par\
\\fs20 2009-2013 (c)oded by Gar|k}";

	char text[]="{\\rtf1\\ansi\\pard\\par\\qc{\\b\\fs52 mail.ru}\\fs40  History Reader 3.1\\par\\fs20 2009-2013 (c)oded by Gar|k}";
		return SendMessage(hRich,WM_SETTEXT,0,(LPARAM)text); 

}

BOOL CALLBACK MainDialogProc(HWND s_hwndDlg,UINT Message, UINT wParam, LONG lParam) 
{




	switch (Message)   
	{   
	case WM_INITDIALOG:
		hwndDlg=s_hwndDlg;
		SendMessage(hwndDlg,WM_SETICON,ICON_SMALL,(LPARAM)LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON1)));
		hMenu=LoadMenu(hInst,MAKEINTRESOURCE(IDR_MENU1));
		SetMenu(hwndDlg,hMenu);

		//for(int i=0;i<20;i++)
		//	EnableMenuItem(hMenu, i, MF_BYPOSITION | MF_ENABLED);
		

		ListView_SetExtendedListViewStyleEx(GetDlgItem(hwndDlg,IDC_LIST1),LVS_EX_FULLROWSELECT,LVS_EX_FULLROWSELECT);

		LV_COLUMN lc;
		lc.mask=LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
		lc.fmt=LVCFMT_LEFT;
		lc.pszText=_TEXT("�����"); 
		lc.cx=189;

		SendDlgItemMessage(hwndDlg,IDC_LIST1,LVM_INSERTCOLUMN,0,(LPARAM)&lc);
		SendDlgItemMessage(hwndDlg,IDC_RICHEDIT21,EM_SETEVENTMASK,0,ENM_MOUSEEVENTS|ENM_KEYEVENTS);
		SetDefaultText(GetDlgItem(hwndDlg,IDC_RICHEDIT21));
		break;   

	case WM_COMMAND:

		if(LOWORD(wParam)==IDC_COMBO1 && HIWORD(wParam)==CBN_SELCHANGE) {
			SetListEmails();
			break;
		}

		switch(LOWORD(wParam))
		{
			// ��������� ���� -----------------------------------------------
		case IDM_OPEN:
			{
				OPENFILENAME ofn;
				TCHAR szFile[MAX_PATH];
				memset(&ofn,0,sizeof(ofn));
				ofn.lStructSize=sizeof(ofn);
				ofn.hwndOwner=hwndDlg;
				ofn.hInstance=hInst;
				ofn.lpstrFile = szFile;
				ofn.lpstrFile[0] = '\0';
				ofn.nMaxFile= sizeof(szFile)/sizeof(*szFile);
				ofn.lpstrFilter=_TEXT("���� ������� Mail.ru ������\0*.dbs\0��� �����\0*.*\0");
				ofn.nFilterIndex=1;
				ofn.Flags=OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_READONLY | OFN_HIDEREADONLY;

				if(GetOpenFileName(&ofn) == TRUE)
				{
					hDBSFile=CreateFile(ofn.lpstrFile,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
					if(INVALID_HANDLE_VALUE != hDBSFile ) 
					{
						hDBSMap=CreateFileMapping(hDBSFile,NULL,PAGE_READONLY,0,0,NULL);
						if(NULL == hDBSMap) 
						{
							CloseHandle(hDBSFile);			
						}
						else
						{
							mra_base=(unsigned char *)MapViewOfFile(hDBSMap,FILE_MAP_COPY,0,0,0);
							if(NULL == mra_base)
							{
								CloseHandle(hDBSMap);
								CloseHandle(hDBSFile);
							}
							else
							{
								TCHAR text[512];
								wsprintf(text,_TEXT("%s - %s"),&ofn.lpstrFile[ofn.nFileOffset],WINTITLE);
								SetWindowText(hwndDlg,text);

								offset_table=(unsigned int *)(mra_base + *(unsigned int*)(mra_base + 16));
								get_history();

								SetComboEmail();
								SetListEmails();

								EnableMenuItem(hMenu,IDM_OPEN,MF_GRAYED);
								EnableMenuItem(hMenu,IDM_CLOSE,MF_ENABLED);
							}
						}
					}
				}

			} //IDM_OPEN
			break;
		case IDM_CLOSE:
			{
				if(NULL != mra_base)
				{
					if(NULL != emails.emails)
					{
						for(int index  = 0; index < emails.count_messages;index++)
						{
							if(INVALID_HANDLE_VALUE != emails.emails[index].hTmpFile)
								CloseHandle(emails.emails[index].hTmpFile);
						}
						VirtualFree(emails.emails,0,MEM_RELEASE);
						emails.count_messages=0;

						HWND hCombo=GetDlgItem(hwndDlg,IDC_COMBO1);
						SendMessage(hCombo,CB_RESETCONTENT,0,0);
						EnableWindow(hCombo,FALSE);

						EnableMenuItem(hMenu,IDM_SAVE_LIST,MF_GRAYED);
						SendDlgItemMessage(hwndDlg,IDC_LIST1,LVM_DELETEALLITEMS,0,0);
						
					}
					UnmapViewOfFile(mra_base);
					mra_base=NULL;

					CloseHandle(hDBSMap);
					CloseHandle(hDBSFile);

					SetWindowText(hwndDlg,WINTITLE);
					EnableMenuItem(hMenu,IDM_OPEN,MF_ENABLED);
					EnableMenuItem(hMenu,IDM_CLOSE,MF_GRAYED);
					

					SetDefaultText(GetDlgItem(hwndDlg,IDC_RICHEDIT21));
				}
			} //IDM_CLOSE
			break;
		case IDM_EXIT:
			SendMessage(hwndDlg,WM_CLOSE,0,0);
			break; // IDM_EXIT
			// / ��������� ���� -----------------------------------------------//
		}
		break;



	case WM_CLOSE:
		SendMessage(hwndDlg,WM_COMMAND,IDM_CLOSE,0);
		EndDialog(hwndDlg,wParam);
		return TRUE;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) 
		{
		case NM_DBLCLK:
			if(((LPNMITEMACTIVATE)lParam)->iItem!=-1)
			{
				LVITEM lvI = {0};
				lvI.mask=LVIF_PARAM;
				lvI.iItem=((LPNMITEMACTIVATE)lParam)->iItem;

				SendDlgItemMessage(hwndDlg,IDC_LIST1,LVM_GETITEM,0,(LPARAM)&lvI);
				return PrintMessage(hwndDlg,&emails,lvI.lParam);
			}
			break;
		}
	}   
	return FALSE;
}