#pragma once
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#include "resource.h"
#include "ServerSocket.h"
#include "LockMachineDlg.h"
#include <io.h>
#include <atlimage.h>
#include <list>
#include <map>

/**
*     �����
*/

#define WM_LOCKMACHINE		(WM_USER + 1)       // ����
#define WM_UNLOCKMACHINE	(WM_USER + 2)       // ����

class CCmdProcessor;
/* ����ָ�룬����ֵΪ void, ����Ϊ ���յ����ݰ�����Ҫ���͵������б� */
typedef void (CCmdProcessor::*CMD_FUNC) (CPacket& recvPack, std::list<CPacket>& sendPacks);

class CCmdProcessor
{
private:
	//CServerSocket* pServer;
	HANDLE						m_hThreadLock;   // �����߳�
	UINT						m_nThreadIdLock; // �����߳� id
	HANDLE						m_hEventLock;
	std::map<int, CMD_FUNC>		m_mapFuncs;
public:
	CCmdProcessor()
	{
		//pServer = CServerSocket::GetInstance();
		m_hThreadLock = INVALID_HANDLE_VALUE;       // ��ʼ���߳̾��

		//������Ϣӳ�����
		struct
		{
			int			nCmd;
			CMD_FUNC	func;
		}
		func_map_table[]  // ����ӳ���
		{
			{1	,&CCmdProcessor::GetDriveInfo	},     // ȡ��Ӳ������������Ϣ
			{2	,&CCmdProcessor::GetFileInfo	},     // ȡ���ļ��б���Ϣ
			{3	,&CCmdProcessor::DownLoadFile	},     // �����ļ�
			{4	,&CCmdProcessor::DelFile		},     // ɾ���ļ�
			{5	,&CCmdProcessor::ScreenWatch	},     // ��Ļ����
			{6	,&CCmdProcessor::ControlMouse	},     // ������
			{7	,&CCmdProcessor::LockMachine	},     // ����
			{8	,&CCmdProcessor::UnLockMachine	},     // ����
			{-1	,NULL							},
		};

		for (int i = 0; func_map_table[i].func != NULL; i++)
		{
			m_mapFuncs.insert(std::pair<int, CMD_FUNC>
				(func_map_table[i].nCmd,
					func_map_table[i].func));
		}//����Ҫִ�в����ĺ����Ͷ�Ӧ������浽 map ��
	}
public:
	void Run()
	{
		
	}

	/* ���ݶ�Ӧ�� id ���� Map ���ҵ���Ӧ�ĺ���ִ�У��൱��������ɷ� */
	void DispatchCommand(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		std::map<int, CMD_FUNC>::iterator it = m_mapFuncs.find(recvPack.nCmd);
		if (it != m_mapFuncs.end())  // �ҵ���
		{
			(this->*(it->second))(recvPack, sendPacks);
		}
	}


private:
	/* ȡ��Ӳ��������Ϣ */
	void GetDriveInfo(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		DRIVEINFO driveInfo;
		for (int i = 1; i <= 26; i++)
		{
			if (_chdrive(i) == 0)  // ָ����ǰ������������ 1 �� 26 ������
			{
				driveInfo.drive[driveInfo.drive_count++] = 'A' + i - 1;
			}
		}
		sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&driveInfo, sizeof(DRIVEINFO)));
	}

	/* ȡ���豸�ϵ��ļ�Ŀ¼ */
	void GetFileInfo(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		std::string path = recvPack.sData; // �ͻ��˴����Ĺ���Ŀ¼
		if (_chdir(path.c_str()) == 0)//_chdir ��������ǰ����Ŀ¼����Ϊ dirnameָ����Ŀ¼�� dirname ����������������Ŀ¼�� �˺����ɸ����κ��������ϵĵ�ǰ����Ŀ¼��
		{
			FILEINFO fileInfo{};
			/* intptr_t first��intptr_t��һ���������ͣ�ͨ�����ڴ洢ָ�������
			 * ��������ڴ洢_findfirst�����ķ���ֵ���÷���ֵ��һ��Ψһ������������������ں�����������������_findnext��
			 * _findfirst��������������ָ��ģʽƥ��ĵ�һ���ļ���������������ҵ���ǰĿ¼����ģʽ"*"ƥ��������ļ����Ǻ�*��һ��ͨ�������ƥ���κ��ļ�����
			*/
			intptr_t first = _findfirst("*", &fileInfo.data);// &file.data ��һ����������
			if (first == -1)
			{
				//��һ����û�ҵ�����ǰ������ǿյ�
				fileInfo.isNull = 1;
				sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO)));
				return;
			}
			else
			{
				do
				{
					//���ڷ���
					fileInfo.isNull = 0;
					sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO)));
					memset(&fileInfo, 0, sizeof(FILEINFO));

				} while (_findnext(first, &fileInfo.data) == 0);
				//�����ˣ���ǰ������ǿյ�
				fileInfo.isNull = 1;
				sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO)));
			}
		}
		else
		{
			FILEINFO fileInfo{};
			fileInfo.isNull = 1;
			sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO)));
		}
	}

	/* �����ļ� */
	void DownLoadFile(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		static const int buffer_size = 1024 * 10;  // 10 KB
		static char buffer[buffer_size]{};
		std::string path = recvPack.sData;
		long long fileLen = 0;
		FILE* pFile = fopen(path.c_str(), "rb+");//rb+ ��д��һ���������ļ���ֻ�����д���ݡ�
		if (pFile == NULL)
		{
			//�ѳ��ȷ������ƶˣ�0��ʾ�ļ�Ϊ�գ�����û��Ȩ��
			sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileLen, 8));
			return;
		}
		//����ļ�����
		_fseeki64(pFile, 0, SEEK_END);//���д��뽫�ļ�ָ�루Ҳ��Ϊ�ļ�λ��ָʾ�����ƶ����ļ�pFile��ĩβ��_fseeki64��fseek������һ�����֣����ڴ������2^31 - 1������32λϵͳ���ֽڵ��ļ���
		fileLen = _ftelli64(pFile);//���д����ȡ��ǰ�ļ�ָ�루��pFile�У���λ�ã�������ֵ����fileLen�����������ļ�ָ���Ѿ����ƶ����ļ���ĩβ��_ftelli64���ص�ֵ�����ļ����ܳ��ȣ����ֽ�Ϊ��λ����ͬ����_ftelli64��ftell������һ�����֣����ڴ�����ļ���
		_fseeki64(pFile, 0, SEEK_SET);//���д��뽫�ļ�ָ���������û��ļ��Ŀ�ͷ��SEEK_SET��һ����������ʾ���ļ��Ŀ�ͷ��ʼ����ƫ������ƫ����������Ϊ0�������ļ�ָ��ص��ļ��Ŀ�ʼλ��
		//�ѳ��ȷ������ƶ�
		sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileLen, 8));
		//��ȡһ�㷢һ��
		int readLen = 0;
		while ((readLen = fread(buffer, 1, buffer_size, pFile)) > 0) // 1���ֽ�1���ֽڵĶ�
		{
			sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)buffer, readLen));
			memset(buffer, 0, buffer_size);
		}
		//����ر�
		fclose(pFile);
	}

	/* ɾ���ļ� */
	void DelFile(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		std::string path = recvPack.sData;
		WCHAR widePath[MAX_PATH]{};
		// �ú�������Ҫ�����ǽ�һ�����ֽ��ַ��ַ���ӳ�䵽һ�����ַ�����Unicode���ַ�����
		int transRet = MultiByteToWideChar(CP_ACP, 0, path.c_str(), path.size(), widePath, MAX_PATH);
		int success = 0;
		if (DeleteFileW(widePath))
		{
			success = 1;
		}
		else
		{
			success = 0;
		}
		sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&success, sizeof(int)));
	}

	/* ��Ļ���� */
	void ScreenWatch(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{

		CImage screen;//CImage��MFC (Microsoft Foundation Classes) ���е�һ���࣬���ڴ���ͼ�����ṩ��һ����װ��GDI+��ͼ���豸�ӿ�+�����ܵ��࣬���ڼ��ء��������ʾͼ��
		HDC hScreen = ::GetDC(NULL);//���ڻ�ȡ������Ļ���豸�����ģ�Device Context�����DC�����豸��������һ�����ݽṹ�����������豸������ʾ������ӡ���ȣ���ص�ͼ����Ϣ�������NULL��ʾҪ��ȡ����������Ļ��DC��
		int nBitperPixel = GetDeviceCaps(hScreen, BITSPIXEL);//ʹ�� GetDeviceCaps ��������ĻDC�л�ȡÿ�����ص�λ��������԰�����ȷ��ͼ�����ȣ�����ɫ��������
		int nWidth = GetDeviceCaps(hScreen, HORZRES);//��ȡ��Ļ�Ŀ�ȣ�������Ϊ��λ����  
		int nHeight = GetDeviceCaps(hScreen, VERTRES);//��ȡ��Ļ�ĸ߶ȣ�������Ϊ��λ����
		screen.Create(nWidth, nHeight, nBitperPixel);//ʹ�ô���ĻDC�л�ȡ����Ϣ������һ���µ�CImage����screen����ʵ���������ڴ���Ϊ��Ļ��ͼ����ռ䡣

		/* �������豸������֮�临��λͼ�����������ĻDC��hScreen�������ݸ��Ƶ�screen�����DC�С����ƴ�ԴDC�����Ͻǣ�0,0����ʼ����ָ���Ŀ�Ⱥ͸߶ȡ�
		 * SRCCOPY��һ����դ�����루ROP������ʾԴ���Ʋ�������ֻ����ԴDC�����ݡ�
		*/
		BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);

		ReleaseDC(NULL, hScreen);  // �ͷ� ֮ǰ����Ļ��ͼ

		/* ���д���ʹ��GlobalAlloc������ȫ�ֶѣ�global heap���з����ڴ档GMEM_MOVEABLE��־ָʾ����ڴ��ǿ��ƶ���
		* ������ζ�������ܻᱻ����ϵͳ���¶�λ�����е�����λ�ã���ϵͳ���������ָ������ڴ�ľ����ȷ��������Ȼ��Ч����
		*/
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
		if (hMem == NULL) return;

		/* IStream��COM��Component Object Model���е�һ���ӿڣ����ڱ�ʾ���Զ�ȡ��д���/�������ֽڵ�����*/
		IStream* pStream = NULL;
		/* �ú�����������ǰ�����HGLOBAL�ڴ���ϴ���һ��IStream���� */
		HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);

		if (ret == S_OK) // �����ڴ�ɹ�
		{
			screen.Save(pStream, Gdiplus::ImageFormatPNG);//�÷�����ͼ�񱣴�ΪPNG��ʽ����д�뵽֮ǰͨ��CreateStreamOnHGlobal������IStream�ӿڣ�pStream��ָ������С�
			LARGE_INTEGER li = { 0 };
			pStream->Seek(li, STREAM_SEEK_SET, NULL); //��������Ϊ��ȷ�������ж�ȡ��д������ʱ�����Ŀ�ʼλ�ÿ�ʼ��
			LPVOID pData = GlobalLock(hMem);//����GlobalLock����������֮ǰͨ��GlobalAlloc������ڴ�飨��hMem��ʾ���������ڴ��󣬿��԰�ȫ�ط��ʺ��޸��ڴ��е����ݡ�
			sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)pData, GlobalSize(hMem), false));

			GlobalUnlock(hMem);
		}
		GlobalFree(hMem);
		screen.ReleaseDC();
		screen.Destroy();
	}

	/* ������ */
	void ControlMouse(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		//���ƶ˷����������Ϣ
		MOUSEINFO mouseInfo;
		memcpy(&mouseInfo, recvPack.sData.c_str(), sizeof(MOUSEINFO));
		//���flags
		int mouseFlags = 0;
		mouseFlags |= mouseInfo.nButton;
		mouseFlags |= mouseInfo.nEvent;
		if (mouseInfo.nButton != MOUSEBTN::NOTHING)
		{
			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
		}
		//�����Ӧ�¼�
		switch (mouseFlags)
		{
			//����¼�����--------------------------------------------
		case MOUSEBTN::LEFT | MOUSEEVE::CLICK:		//�������
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::LEFT | MOUSEEVE::DBCLICK:	//���˫�����������������
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::LEFT | MOUSEEVE::DOWN:		//�������
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::LEFT | MOUSEEVE::UP:			//�������
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
			//�м��¼�����--------------------------------------------
		case MOUSEBTN::MID | MOUSEEVE::CLICK:			//�м�����
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::MID | MOUSEEVE::DBCLICK:			//�м�˫��
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::MID | MOUSEEVE::DOWN:			//�м�����
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::MID | MOUSEEVE::UP:				//�м�����
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
			//�Ҽ��¼�����--------------------------------------------
		case MOUSEBTN::RIGHT | MOUSEEVE::CLICK:			//�Ҽ�����
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::RIGHT | MOUSEEVE::DBCLICK:		//�Ҽ�˫��
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::RIGHT | MOUSEEVE::DOWN:			//�Ҽ�����
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::RIGHT | MOUSEEVE::UP:			//�Ҽ�����
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
			//�ƶ��¼�����--------------------------------------------
		case MOUSEBTN::NOTHING | MOUSEEVE::MOVE:		//ֱ���ƶ�
			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
			break;
		}
		//������Ӧ
		sendPacks.push_back(CPacket(recvPack.nCmd));
	}

	/* ���� */
	void LockMachine(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		if (m_hThreadLock == INVALID_HANDLE_VALUE)
		{
			m_hThreadLock = (HANDLE)_beginthreadex(NULL, 0, &ThreadEntryLock, this, 0, &m_nThreadIdLock);
			m_hEventLock = CreateEvent(NULL, TRUE, FALSE, NULL);
			if (m_hEventLock)
			{
				/* WaitForSingleObject �������ָ������ĵ�ǰ״̬�� ��������״̬Ϊ���źţ�������߳̽�����ȴ�״̬��ֱ�������յ��źŻ�ʱ����ѹ��� */
				WaitForSingleObject(m_hEventLock, 100);
			}
		}
		/* ������ ID Ϊ m_nThreadIdLock ���̷߳���һ����Ϊ WM_LOCKMACHINE ����Ϣ��������ͳɹ���postRet������һ������ֵ�����ʧ�ܣ���Ϊ0��*/
		int postRet = PostThreadMessage(m_nThreadIdLock, WM_LOCKMACHINE, NULL, NULL);
		if (postRet == 0)
		{
			Sleep(10);// ����ʧ�ܣ�˯�߼�����ٴη�����Ϣ
			PostThreadMessage(m_nThreadIdLock, WM_LOCKMACHINE, NULL, NULL);
		}
		//������Ӧ
		sendPacks.push_back(CPacket(recvPack.nCmd));
	}

	/* ���� */
	void UnLockMachine(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		PostThreadMessage(m_nThreadIdLock, WM_UNLOCKMACHINE, NULL, NULL);
		//������Ӧ
		sendPacks.push_back(CPacket(recvPack.nCmd));
	}

	static unsigned __stdcall ThreadEntryLock(void* arg)
	{
		CCmdProcessor* thiz = (CCmdProcessor*)arg;
		thiz->ThreadLock();
		_endthreadex(0);  // �����߳�
		return 0;
	}

	/* �߳�ִ�к��� */
	void ThreadLock()
	{
		SetEvent(m_hEventLock);
		CLockMachineDlg lockDlg;  // �����Ի���
		lockDlg.Create(IDD_LOCKMACHINEDLG);  // �����Ի���
		::SetWindowPos(lockDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // ���öԻ����λ��
		int screenWidth = GetSystemMetrics(SM_CXSCREEN); //����ʾ������Ļ��ȣ�������Ϊ��λ����
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);//����ʾ������Ļ�߶ȣ�������Ϊ��λ���� 
		CRect screenRect(0, 0, screenWidth, (int)(screenHeight * 1.1));
		lockDlg.MoveWindow(screenRect);
		CRect txtInfoRectOld;
		lockDlg.m_txtInfo.GetWindowRect(&txtInfoRectOld);
		CRect txtInfoRectNew;
		// ��������������Զ��ʾ����Ļ���м�
		txtInfoRectNew.left = (int)(screenRect.Width() / 2.0 - txtInfoRectOld.Width() / 2);
		txtInfoRectNew.top = (int)(screenRect.Height() / 2.0 - txtInfoRectOld.Height() / 2);
		txtInfoRectNew.right = txtInfoRectNew.left + txtInfoRectOld.Width();
		txtInfoRectNew.bottom = txtInfoRectNew.top + txtInfoRectOld.Height();
		lockDlg.m_txtInfo.MoveWindow(txtInfoRectNew);

		/* Windows �µ���Ϣѭ������ */
		MSG msg;
		while (::GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_LOCKMACHINE)
			{
				lockDlg.ShowWindow(SW_SHOW);
				lockDlg.CenterWindow();
			}

			if (msg.message == WM_UNLOCKMACHINE)
			{
				lockDlg.ShowWindow(SW_HIDE);
			}
		}
	}
};
//
//	void GetDriveInfo(WORD _nCmd)
//	{
//		DRIVEINFO driveInfo;
//		for (int i = 1; i <= 26; i++)
//		{
//			if (_chdrive(i) == 0)
//			{
//				driveInfo.drive[driveInfo.drive_count++] = 'A' + i - 1;
//			}
//		}
//		CPacket pack(_nCmd, (BYTE*)&driveInfo, sizeof(DRIVEINFO));
//		pServer->Send(pack);
//		pServer->CloseSocket();
//	}
//	void GetFileInfo(WORD _nCmd)
//	{
//		std::string path;
//		pServer->GetPath(path);
//		if (_chdir(path.c_str()) == 0)
//		{
//			FILEINFO fileInfo{};
//			intptr_t first = _findfirst("*", &fileInfo.data);
//			if (first == -1)
//			{
//				//��һ����û�ҵ�����ǰ������ǿյ�
//				fileInfo.isNull = 1;
//				CPacket pack(_nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO));
//				pServer->Send(pack);
//				pServer->CloseSocket();
//
//				return;
//			}
//			else
//			{
//				do
//				{
//					//���ڷ���
//					fileInfo.isNull = 0;
//					CPacket pack(_nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO));
//					int ret = pServer->Send(pack);
//					TRACE("-server-nCmd = %d name = %s\r\n", ret, fileInfo.data.name);
//					memset(&fileInfo, 0, sizeof(FILEINFO));
//
//				} while (_findnext(first, &fileInfo.data) == 0);
//				//�����ˣ���ǰ������ǿյ�
//				fileInfo.isNull = 1;
//				CPacket pack(_nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO));
//				pServer->Send(pack);
//				pServer->CloseSocket();
//
//			}
//		}
//		else
//		{
//			FILEINFO fileInfo{};
//			fileInfo.isNull = 1;
//			CPacket pack(_nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO));
//			pServer->Send(pack);
//			pServer->CloseSocket();
//		}
//	}
//	void DownLoadFile(WORD _nCmd)
//	{
//		static const int buffer_size = 1024 * 10;
//		static char buffer[buffer_size]{};
//		std::string path;
//		pServer->GetPath(path);
//		long long fileLen = 0;
//		FILE* pFile = fopen(path.c_str(), "rb+");
//		if (pFile == NULL)
//		{
//			//�ѳ��ȷ������ƶˣ�0��ʾ�ļ�Ϊ�գ�����û��Ȩ��
//			CPacket pack(_nCmd, (BYTE*)&fileLen, 8);
//			pServer->Send(pack);
//			pServer->CloseSocket();
//			return;
//		}
//		//����ļ�����
//		_fseeki64(pFile, 0, SEEK_END);
//		 fileLen = _ftelli64(pFile);
//		_fseeki64(pFile, 0, SEEK_SET);
//		//�ѳ��ȷ������ƶ�
//		CPacket pack(_nCmd, (BYTE*)&fileLen, 8);
//		pServer->Send(pack);
//		//��ȡһ�㷢һ��
//		int readLen = 0;
//		while ((readLen = fread(buffer, 1, buffer_size, pFile)) > 0)
//		{
//			CPacket pack(_nCmd, (BYTE*)buffer, readLen);
//			pServer->Send(pack);
//			memset(buffer, 0, buffer_size);
//		}
//		//����ر�
//		pServer->CloseSocket();
//		fclose(pFile);
//	}
//	void DelFile(WORD _nCmd)
//	{
//		std::string path;
//		pServer->GetPath(path);
//		WCHAR widePath[MAX_PATH]{};
//		int transRet = MultiByteToWideChar(CP_ACP, 0, path.c_str(), path.size(), widePath, MAX_PATH);
//		int success = 0;
//		if (DeleteFileW(widePath))
//		{
//			success = 1;		
//		}
//		else
//		{
//			success = 0;	
//		}
//		CPacket pack(_nCmd, (BYTE*)&success, sizeof(int));
//		pServer->Send(pack);
//		pServer->CloseSocket();
//	}
//	void ScreenWatch(WORD _nCmd)
//	{
//		//��Ļ��ͼ
//		/*CImage screen;
//		HDC hScreen = ::GetDC(NULL);
//		int nBitperPixel = GetDeviceCaps(hScreen, BITSPIXEL);
//		int nWidth = GetDeviceCaps(hScreen, HORZRES);
//		int nHeight = GetDeviceCaps(hScreen, VERTRES);
//		screen.Create(nWidth, nHeight, nBitperPixel);
//		BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
//		ReleaseDC(NULL, hScreen);
//
//		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
//		if (hMem == NULL) return -1;
//		IStream* pStream = NULL;
//		HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
//
//		if (ret == S_OK)
//		{
//			screen.Save(pStream, Gdiplus::ImageFormatPNG);
//		}*/
//
//		CImage screen;
//		HDC hScreen = ::GetDC(NULL);
//		int nBitperPixel = GetDeviceCaps(hScreen, BITSPIXEL);
//		int nWidth = GetDeviceCaps(hScreen, HORZRES);
//		int nHeight = GetDeviceCaps(hScreen, VERTRES);
//		screen.Create(nWidth, nHeight, nBitperPixel);
//		BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
//		ReleaseDC(NULL, hScreen);
//
//		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
//		if (hMem == NULL) return;
//		IStream* pStream = NULL;
//		HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
//
//		if (ret == S_OK)
//		{
//			screen.Save(pStream, Gdiplus::ImageFormatPNG);
//			LARGE_INTEGER li = { 0 };
//			pStream->Seek(li, STREAM_SEEK_SET, NULL);
//			LPVOID pData = GlobalLock(hMem);
//			CPacket pack(_nCmd, (BYTE*)pData, GlobalSize(hMem));
//			pServer->Send(pack);
//			GlobalUnlock(hMem);
//		}
//		GlobalFree(hMem);
//		screen.ReleaseDC();
//		screen.Destroy();
//	}
//	void ControlMouse(WORD _nCmd)
//	{
//		//���ƶ˷����������Ϣ
//		MOUSEINFO mouseInfo;
//		memcpy(&mouseInfo, pServer->GetPacket().sData.c_str(), sizeof(MOUSEINFO));
//		//���flags
//		int mouseFlags = 0;
//		mouseFlags |= mouseInfo.nButton;
//		mouseFlags |= mouseInfo.nEvent;
//		if (mouseInfo.nButton != MOUSEBTN::NOTHING)
//		{
//			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
//		}
//		//�����Ӧ�¼�
//		switch (mouseFlags)
//		{
//		//����¼�����--------------------------------------------
//		case MOUSEBTN::LEFT		| MOUSEEVE::CLICK:		//�������
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::LEFT		| MOUSEEVE::DBCLICK:	//���˫��
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::LEFT		| MOUSEEVE::DOWN:		//�������
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::LEFT		| MOUSEEVE::UP:			//�������
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		//�м��¼�����--------------------------------------------
//		case MOUSEBTN::MID | MOUSEEVE::CLICK:			//�м�����
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::MID | MOUSEEVE::DBCLICK:			//�м�˫��
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::MID | MOUSEEVE::DOWN:			//�м�����
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::MID | MOUSEEVE::UP:				//�м�����
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		//�Ҽ��¼�����--------------------------------------------
//		case MOUSEBTN::RIGHT | MOUSEEVE::CLICK:			//�Ҽ�����
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::RIGHT | MOUSEEVE::DBCLICK:		//�Ҽ�˫��
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::RIGHT | MOUSEEVE::DOWN:			//�Ҽ�����
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::RIGHT | MOUSEEVE::UP:			//�Ҽ�����
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		//�ƶ��¼�����--------------------------------------------
//		case MOUSEBTN::NOTHING	| MOUSEEVE::MOVE:		//ֱ���ƶ�
//			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
//			break;
//		}
//		//������Ӧ
//		CPacket pack(_nCmd);
//		pServer->Send(pack);
//	}
//	void LockMachine(WORD _nCmd)
//	{
//		if (m_hThreadLock == INVALID_HANDLE_VALUE)
//		{
//			m_hThreadLock = (HANDLE)_beginthreadex(NULL,0,&ThreadEntryLock,this, 0, &m_nThreadIdLock);
//			m_hEventLock = CreateEvent(NULL, TRUE, FALSE, NULL);
//			if (m_hEventLock)
//			{
//				WaitForSingleObject(m_hEventLock, 100);
//			}
//		}
//		int postRet = PostThreadMessage(m_nThreadIdLock, WM_LOCKMACHINE,NULL,NULL);
//		if (postRet == 0)
//		{
//			Sleep(10);
//			PostThreadMessage(m_nThreadIdLock, WM_LOCKMACHINE, NULL, NULL);
//		}
//		//������Ӧ
//		CPacket pack(_nCmd);
//		pServer->Send(pack);
//	}
//	void UnLockMachine(WORD _nCmd)
//	{
//		PostThreadMessage(m_nThreadIdLock, WM_UNLOCKMACHINE, NULL, NULL);
//		//������Ӧ
//		CPacket pack(_nCmd);
//		pServer->Send(pack);
//	}
//	static unsigned __stdcall ThreadEntryLock(void* arg)
//	{
//		CCmdProcessor* thiz = (CCmdProcessor*)arg;
//		thiz->ThreadLock();
//		_endthreadex(0);
//		return 0;
//	}
//	void ThreadLock()
//	{
//		SetEvent(m_hEventLock);	
//		CLockMachineDlg lockDlg;
//		lockDlg.Create(IDD_LOCKMACHINEDLG);
//		::SetWindowPos(lockDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
//		int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
//		int screenHeight = GetSystemMetrics(SM_CYSCREEN);
//		CRect screenRect(0,0,screenWidth,(int)(screenHeight * 1.1));
//		lockDlg.MoveWindow(screenRect);
//		CRect txtInfoRectOld;
//		lockDlg.m_txtInfo.GetWindowRect(&txtInfoRectOld);
//		CRect txtInfoRectNew;
//		txtInfoRectNew.left   = (int)(screenRect.Width()  / 2.0 - txtInfoRectOld.Width()  / 2);
//		txtInfoRectNew.top    = (int)(screenRect.Height() / 2.0 - txtInfoRectOld.Height() / 2);
//		txtInfoRectNew.right  = txtInfoRectNew .left + txtInfoRectOld.Width();
//		txtInfoRectNew.bottom = txtInfoRectNew.top  + txtInfoRectOld.Height();
//		lockDlg.m_txtInfo.MoveWindow(txtInfoRectNew);
//
//		MSG msg;
//		while (::GetMessage(&msg,NULL,0,0))
//		{
//			TranslateMessage(&msg);
//			DispatchMessage (&msg);
//
//			if (msg.message == WM_LOCKMACHINE)
//			{
//				lockDlg.ShowWindow(SW_SHOW);
//				lockDlg.CenterWindow();
//			}
//
//			if (msg.message == WM_UNLOCKMACHINE)
//			{
//				lockDlg.ShowWindow(SW_HIDE);
//			}
//		}
//
//	}
//};

