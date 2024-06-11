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
*     命令处理
*/

#define WM_LOCKMACHINE		(WM_USER + 1)       // 锁机
#define WM_UNLOCKMACHINE	(WM_USER + 2)       // 解锁

class CCmdProcessor;
/* 函数指针，返回值为 void, 参数为 接收的数据包和需要发送的数据列表 */
typedef void (CCmdProcessor::*CMD_FUNC) (CPacket& recvPack, std::list<CPacket>& sendPacks);

class CCmdProcessor
{
private:
	//CServerSocket* pServer;
	HANDLE						m_hThreadLock;   // 锁机线程
	UINT						m_nThreadIdLock; // 锁机线程 id
	HANDLE						m_hEventLock;
	std::map<int, CMD_FUNC>		m_mapFuncs;
public:
	CCmdProcessor()
	{
		//pServer = CServerSocket::GetInstance();
		m_hThreadLock = INVALID_HANDLE_VALUE;       // 初始化线程句柄

		//建立消息映射机制
		struct
		{
			int			nCmd;
			CMD_FUNC	func;
		}
		func_map_table[]  // 函数映射表
		{
			{1	,&CCmdProcessor::GetDriveInfo	},     // 取得硬件驱动磁盘信息
			{2	,&CCmdProcessor::GetFileInfo	},     // 取得文件列表信息
			{3	,&CCmdProcessor::DownLoadFile	},     // 下载文件
			{4	,&CCmdProcessor::DelFile		},     // 删除文件
			{5	,&CCmdProcessor::ScreenWatch	},     // 屏幕监视
			{6	,&CCmdProcessor::ControlMouse	},     // 鼠标控制
			{7	,&CCmdProcessor::LockMachine	},     // 锁机
			{8	,&CCmdProcessor::UnLockMachine	},     // 解锁
			{-1	,NULL							},
		};

		for (int i = 0; func_map_table[i].func != NULL; i++)
		{
			m_mapFuncs.insert(std::pair<int, CMD_FUNC>
				(func_map_table[i].nCmd,
					func_map_table[i].func));
		}//将需要执行操作的函数和对应的命令保存到 map 中
	}
public:
	void Run()
	{
		
	}

	/* 根据对应的 id 号在 Map 中找到对应的函数执行，相当于任务的派发 */
	void DispatchCommand(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		std::map<int, CMD_FUNC>::iterator it = m_mapFuncs.find(recvPack.nCmd);
		if (it != m_mapFuncs.end())  // 找到了
		{
			(this->*(it->second))(recvPack, sendPacks);
		}
	}


private:
	/* 取得硬件磁盘信息 */
	void GetDriveInfo(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		DRIVEINFO driveInfo;
		for (int i = 1; i <= 26; i++)
		{
			if (_chdrive(i) == 0)  // 指定当前工作驱动器的 1 到 26 的整数
			{
				driveInfo.drive[driveInfo.drive_count++] = 'A' + i - 1;
			}
		}
		sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&driveInfo, sizeof(DRIVEINFO)));
	}

	/* 取得设备上的文件目录 */
	void GetFileInfo(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		std::string path = recvPack.sData; // 客户端传来的工作目录
		if (_chdir(path.c_str()) == 0)//_chdir 函数将当前工作目录更改为 dirname指定的目录。 dirname 参数必须引用现有目录。 此函数可更改任何驱动器上的当前工作目录。
		{
			FILEINFO fileInfo{};
			/* intptr_t first：intptr_t是一个整数类型，通常用于存储指针或句柄。
			 * 这里，它用于存储_findfirst函数的返回值，该返回值是一个唯一的搜索句柄，可以用于后续的搜索操作（如_findnext）
			 * _findfirst函数用于搜索与指定模式匹配的第一个文件。在这里，它尝试找到当前目录下与模式"*"匹配的所有文件。星号*是一个通配符，它匹配任何文件名。
			*/
			intptr_t first = _findfirst("*", &fileInfo.data);// &file.data 是一个传出参数
			if (first == -1)
			{
				//第一个就没找到：当前这个就是空的
				fileInfo.isNull = 1;
				sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO)));
				return;
			}
			else
			{
				do
				{
					//正在发送
					fileInfo.isNull = 0;
					sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO)));
					memset(&fileInfo, 0, sizeof(FILEINFO));

				} while (_findnext(first, &fileInfo.data) == 0);
				//发完了：当前这个就是空的
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

	/* 下载文件 */
	void DownLoadFile(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		static const int buffer_size = 1024 * 10;  // 10 KB
		static char buffer[buffer_size]{};
		std::string path = recvPack.sData;
		long long fileLen = 0;
		FILE* pFile = fopen(path.c_str(), "rb+");//rb+ 读写打开一个二进制文件，只允许读写数据。
		if (pFile == NULL)
		{
			//把长度发给控制端，0表示文件为空，或者没有权限
			sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileLen, 8));
			return;
		}
		//获得文件长度
		_fseeki64(pFile, 0, SEEK_END);//这行代码将文件指针（也称为文件位置指示器）移动到文件pFile的末尾。_fseeki64是fseek函数的一个变种，用于处理大于2^31 - 1（对于32位系统）字节的文件。
		fileLen = _ftelli64(pFile);//这行代码获取当前文件指针（在pFile中）的位置，并将其值赋给fileLen变量。由于文件指针已经被移动到文件的末尾，_ftelli64返回的值就是文件的总长度（以字节为单位）。同样，_ftelli64是ftell函数的一个变种，用于处理大文件。
		_fseeki64(pFile, 0, SEEK_SET);//这行代码将文件指针重新设置回文件的开头。SEEK_SET是一个常量，表示从文件的开头开始计算偏移量。偏移量被设置为0，所以文件指针回到文件的开始位置
		//把长度发给控制端
		sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)&fileLen, 8));
		//读取一点发一点
		int readLen = 0;
		while ((readLen = fread(buffer, 1, buffer_size, pFile)) > 0) // 1个字节1个字节的读
		{
			sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)buffer, readLen));
			memset(buffer, 0, buffer_size);
		}
		//发完关闭
		fclose(pFile);
	}

	/* 删除文件 */
	void DelFile(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		std::string path = recvPack.sData;
		WCHAR widePath[MAX_PATH]{};
		// 该函数的主要功能是将一个多字节字符字符串映射到一个宽字符（即Unicode）字符串。
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

	/* 屏幕监视 */
	void ScreenWatch(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{

		CImage screen;//CImage是MFC (Microsoft Foundation Classes) 库中的一个类，用于处理图像。它提供了一个封装了GDI+（图形设备接口+）功能的类，用于加载、保存和显示图像。
		HDC hScreen = ::GetDC(NULL);//用于获取整个屏幕的设备上下文（Device Context，简称DC）。设备上下文是一个数据结构，包含了与设备（如显示器、打印机等）相关的图形信息。在这里，NULL表示要获取的是整个屏幕的DC。
		int nBitperPixel = GetDeviceCaps(hScreen, BITSPIXEL);//使用 GetDeviceCaps 函数从屏幕DC中获取每个像素的位数。这可以帮助你确定图像的深度（即颜色质量）。
		int nWidth = GetDeviceCaps(hScreen, HORZRES);//获取屏幕的宽度（以像素为单位）。  
		int nHeight = GetDeviceCaps(hScreen, VERTRES);//获取屏幕的高度（以像素为单位）。
		screen.Create(nWidth, nHeight, nBitperPixel);//使用从屏幕DC中获取的信息，创建一个新的CImage对象screen。这实际上是在内存中为屏幕截图分配空间。

		/* 用于在设备上下文之间复制位图。这里，它将屏幕DC（hScreen）的内容复制到screen对象的DC中。复制从源DC的左上角（0,0）开始，到指定的宽度和高度。
		 * SRCCOPY是一个光栅操作码（ROP），表示源复制操作，即只复制源DC的内容。
		*/
		BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);

		ReleaseDC(NULL, hScreen);  // 释放 之前的屏幕截图

		/* 这行代码使用GlobalAlloc函数从全局堆（global heap）中分配内存。GMEM_MOVEABLE标志指示这块内存是可移动的
		* （这意味着它可能会被操作系统重新定位到堆中的其他位置，但系统会更新所有指向这块内存的句柄以确保它们仍然有效）。
		*/
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
		if (hMem == NULL) return;

		/* IStream是COM（Component Object Model）中的一个接口，用于表示可以读取、写入和/或搜索字节的流。*/
		IStream* pStream = NULL;
		/* 该函数尝试在先前分配的HGLOBAL内存块上创建一个IStream对象 */
		HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);

		if (ret == S_OK) // 分配内存成功
		{
			screen.Save(pStream, Gdiplus::ImageFormatPNG);//该方法将图像保存为PNG格式，并写入到之前通过CreateStreamOnHGlobal创建的IStream接口（pStream）指向的流中。
			LARGE_INTEGER li = { 0 };
			pStream->Seek(li, STREAM_SEEK_SET, NULL); //这样做是为了确保从流中读取或写入数据时从流的开始位置开始。
			LPVOID pData = GlobalLock(hMem);//调用GlobalLock函数来锁定之前通过GlobalAlloc分配的内存块（由hMem表示）。锁定内存块后，可以安全地访问和修改内存中的数据。
			sendPacks.push_back(CPacket(recvPack.nCmd, (BYTE*)pData, GlobalSize(hMem), false));

			GlobalUnlock(hMem);
		}
		GlobalFree(hMem);
		screen.ReleaseDC();
		screen.Destroy();
	}

	/* 鼠标控制 */
	void ControlMouse(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		//控制端发来的鼠标信息
		MOUSEINFO mouseInfo;
		memcpy(&mouseInfo, recvPack.sData.c_str(), sizeof(MOUSEINFO));
		//组成flags
		int mouseFlags = 0;
		mouseFlags |= mouseInfo.nButton;
		mouseFlags |= mouseInfo.nEvent;
		if (mouseInfo.nButton != MOUSEBTN::NOTHING)
		{
			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
		}
		//处理对应事件
		switch (mouseFlags)
		{
			//左键事件处理--------------------------------------------
		case MOUSEBTN::LEFT | MOUSEEVE::CLICK:		//左键单击
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::LEFT | MOUSEEVE::DBCLICK:	//左键双击，调两次左键单击
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::LEFT | MOUSEEVE::DOWN:		//左键按下
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::LEFT | MOUSEEVE::UP:			//左键弹起
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
			//中键事件处理--------------------------------------------
		case MOUSEBTN::MID | MOUSEEVE::CLICK:			//中键单击
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::MID | MOUSEEVE::DBCLICK:			//中键双击
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::MID | MOUSEEVE::DOWN:			//中键按下
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::MID | MOUSEEVE::UP:				//中键弹起
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
			//右键事件处理--------------------------------------------
		case MOUSEBTN::RIGHT | MOUSEEVE::CLICK:			//右键单击
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::RIGHT | MOUSEEVE::DBCLICK:		//右键双击
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::RIGHT | MOUSEEVE::DOWN:			//右键按下
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case MOUSEBTN::RIGHT | MOUSEEVE::UP:			//右键弹起
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
			//移动事件处理--------------------------------------------
		case MOUSEBTN::NOTHING | MOUSEEVE::MOVE:		//直接移动
			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
			break;
		}
		//给个回应
		sendPacks.push_back(CPacket(recvPack.nCmd));
	}

	/* 锁机 */
	void LockMachine(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		if (m_hThreadLock == INVALID_HANDLE_VALUE)
		{
			m_hThreadLock = (HANDLE)_beginthreadex(NULL, 0, &ThreadEntryLock, this, 0, &m_nThreadIdLock);
			m_hEventLock = CreateEvent(NULL, TRUE, FALSE, NULL);
			if (m_hEventLock)
			{
				/* WaitForSingleObject 函数检查指定对象的当前状态。 如果对象的状态为非信号，则调用线程将进入等待状态，直到对象收到信号或超时间隔已过。 */
				WaitForSingleObject(m_hEventLock, 100);
			}
		}
		/* 尝试向 ID 为 m_nThreadIdLock 的线程发送一个名为 WM_LOCKMACHINE 的消息。如果发送成功，postRet将包含一个非零值；如果失败，则为0。*/
		int postRet = PostThreadMessage(m_nThreadIdLock, WM_LOCKMACHINE, NULL, NULL);
		if (postRet == 0)
		{
			Sleep(10);// 发送失败，睡眠几秒后再次发送消息
			PostThreadMessage(m_nThreadIdLock, WM_LOCKMACHINE, NULL, NULL);
		}
		//给个回应
		sendPacks.push_back(CPacket(recvPack.nCmd));
	}

	/* 解锁 */
	void UnLockMachine(CPacket& recvPack, std::list<CPacket>& sendPacks)
	{
		PostThreadMessage(m_nThreadIdLock, WM_UNLOCKMACHINE, NULL, NULL);
		//给个回应
		sendPacks.push_back(CPacket(recvPack.nCmd));
	}

	static unsigned __stdcall ThreadEntryLock(void* arg)
	{
		CCmdProcessor* thiz = (CCmdProcessor*)arg;
		thiz->ThreadLock();
		_endthreadex(0);  // 结束线程
		return 0;
	}

	/* 线程执行函数 */
	void ThreadLock()
	{
		SetEvent(m_hEventLock);
		CLockMachineDlg lockDlg;  // 锁机对话框
		lockDlg.Create(IDD_LOCKMACHINEDLG);  // 创建对话框
		::SetWindowPos(lockDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // 设置对话框的位置
		int screenWidth = GetSystemMetrics(SM_CXSCREEN); //主显示器的屏幕宽度（以像素为单位）。
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);//主显示器的屏幕高度（以像素为单位）。 
		CRect screenRect(0, 0, screenWidth, (int)(screenHeight * 1.1));
		lockDlg.MoveWindow(screenRect);
		CRect txtInfoRectOld;
		lockDlg.m_txtInfo.GetWindowRect(&txtInfoRectOld);
		CRect txtInfoRectNew;
		// 设置文字区域永远显示在屏幕的中间
		txtInfoRectNew.left = (int)(screenRect.Width() / 2.0 - txtInfoRectOld.Width() / 2);
		txtInfoRectNew.top = (int)(screenRect.Height() / 2.0 - txtInfoRectOld.Height() / 2);
		txtInfoRectNew.right = txtInfoRectNew.left + txtInfoRectOld.Width();
		txtInfoRectNew.bottom = txtInfoRectNew.top + txtInfoRectOld.Height();
		lockDlg.m_txtInfo.MoveWindow(txtInfoRectNew);

		/* Windows 下的消息循环机制 */
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
//				//第一个就没找到：当前这个就是空的
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
//					//正在发送
//					fileInfo.isNull = 0;
//					CPacket pack(_nCmd, (BYTE*)&fileInfo, sizeof(FILEINFO));
//					int ret = pServer->Send(pack);
//					TRACE("-server-nCmd = %d name = %s\r\n", ret, fileInfo.data.name);
//					memset(&fileInfo, 0, sizeof(FILEINFO));
//
//				} while (_findnext(first, &fileInfo.data) == 0);
//				//发完了：当前这个就是空的
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
//			//把长度发给控制端，0表示文件为空，或者没有权限
//			CPacket pack(_nCmd, (BYTE*)&fileLen, 8);
//			pServer->Send(pack);
//			pServer->CloseSocket();
//			return;
//		}
//		//获得文件长度
//		_fseeki64(pFile, 0, SEEK_END);
//		 fileLen = _ftelli64(pFile);
//		_fseeki64(pFile, 0, SEEK_SET);
//		//把长度发给控制端
//		CPacket pack(_nCmd, (BYTE*)&fileLen, 8);
//		pServer->Send(pack);
//		//读取一点发一点
//		int readLen = 0;
//		while ((readLen = fread(buffer, 1, buffer_size, pFile)) > 0)
//		{
//			CPacket pack(_nCmd, (BYTE*)buffer, readLen);
//			pServer->Send(pack);
//			memset(buffer, 0, buffer_size);
//		}
//		//发完关闭
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
//		//屏幕截图
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
//		//控制端发来的鼠标信息
//		MOUSEINFO mouseInfo;
//		memcpy(&mouseInfo, pServer->GetPacket().sData.c_str(), sizeof(MOUSEINFO));
//		//组成flags
//		int mouseFlags = 0;
//		mouseFlags |= mouseInfo.nButton;
//		mouseFlags |= mouseInfo.nEvent;
//		if (mouseInfo.nButton != MOUSEBTN::NOTHING)
//		{
//			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
//		}
//		//处理对应事件
//		switch (mouseFlags)
//		{
//		//左键事件处理--------------------------------------------
//		case MOUSEBTN::LEFT		| MOUSEEVE::CLICK:		//左键单击
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::LEFT		| MOUSEEVE::DBCLICK:	//左键双击
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::LEFT		| MOUSEEVE::DOWN:		//左键按下
//			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::LEFT		| MOUSEEVE::UP:			//左键弹起
//			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		//中键事件处理--------------------------------------------
//		case MOUSEBTN::MID | MOUSEEVE::CLICK:			//中键单击
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::MID | MOUSEEVE::DBCLICK:			//中键双击
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::MID | MOUSEEVE::DOWN:			//中键按下
//			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::MID | MOUSEEVE::UP:				//中键弹起
//			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		//右键事件处理--------------------------------------------
//		case MOUSEBTN::RIGHT | MOUSEEVE::CLICK:			//右键单击
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::RIGHT | MOUSEEVE::DBCLICK:		//右键双击
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::RIGHT | MOUSEEVE::DOWN:			//右键按下
//			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		case MOUSEBTN::RIGHT | MOUSEEVE::UP:			//右键弹起
//			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
//			break;
//		//移动事件处理--------------------------------------------
//		case MOUSEBTN::NOTHING	| MOUSEEVE::MOVE:		//直接移动
//			SetCursorPos(mouseInfo.ptXY.x, mouseInfo.ptXY.y);
//			break;
//		}
//		//给个回应
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
//		//给个回应
//		CPacket pack(_nCmd);
//		pServer->Send(pack);
//	}
//	void UnLockMachine(WORD _nCmd)
//	{
//		PostThreadMessage(m_nThreadIdLock, WM_UNLOCKMACHINE, NULL, NULL);
//		//给个回应
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

