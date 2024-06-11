#pragma once

#include <afxdialogex.h>

// CLockMachineDlg 对话框

// 资源视图 ---> 新建 ---> 资源 ---> Dialog
// 右键对话框 ---> 添加类 ---> 继承 CDialog

class CLockMachineDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CLockMachineDlg)

public:
	CLockMachineDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CLockMachineDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LOCKMACHINEDLG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_txtInfo;   // 你已被锁定，请联系管理员解锁
};
