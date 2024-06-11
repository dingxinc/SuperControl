// DownLoadFileDlg.cpp: 实现文件
//

#include "pch.h"
#include "SControlClient.h"
#include "DownLoadFileDlg.h"
#include "afxdialogex.h"


// CDownLoadFileDlg 对话框

IMPLEMENT_DYNAMIC(CDownLoadFileDlg, CDialogEx)

CDownLoadFileDlg::CDownLoadFileDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLFILE_STATUS, pParent)
	
{

}

CDownLoadFileDlg::~CDownLoadFileDlg()
{
}

void CDownLoadFileDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_DL_INFO, m_Info);
	DDX_Control(pDX, IDC_PROGRESS_DL, m_Pro);
	DDX_Control(pDX, IDC_TXT_DL_PRO, m_TxtPro);
	DDX_Control(pDX, IDC_BTN_DL_OK, m_BtnOk);
}

void CDownLoadFileDlg::SetFileLength(long long _fileLen)
{
	fileLen = _fileLen;                  // 下载的文件长度
	m_Pro.SetRange(0, 100);  // 进度条范围
}

void CDownLoadFileDlg::SetDownLoadedLen(long long _len)   // 根据下载文件的进度 动态显示进度条的进度
{
	int _bfb = (int)(1.0 * _len / fileLen * 100);
	if (_bfb != bfb)
	{
		CString str;
		str.Format(L"%d", _bfb);
		str.Append(L"%");
		m_TxtPro.SetWindowText(str);
		m_Pro.SetPos(_bfb);
	}	
	if (_len == fileLen)
	{
		m_BtnOk.ShowWindow(SW_SHOW);
	}
	bfb = _bfb;
}

void CDownLoadFileDlg::SetInfo(CString& info)  // 设置显示信息
{
	m_Info.SetWindowText(info);
}


BEGIN_MESSAGE_MAP(CDownLoadFileDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_DL_OK, &CDownLoadFileDlg::OnBnClickedBtnDlOk)
END_MESSAGE_MAP()


// CDownLoadFileDlg 消息处理程序


void CDownLoadFileDlg::OnBnClickedBtnDlOk()
{
	DestroyWindow();
}


void CDownLoadFileDlg::OnCancel()
{
	// TODO: 在此添加专用代码和/或调用基类
	DestroyWindow();
}
