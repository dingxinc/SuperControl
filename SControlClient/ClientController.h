#pragma once
#include <vector>
#include <string>
#include "Common.h"

class CClientController
{
public:
	// 用来记录用户信息，不同的用户有不同的 ip 地址和 端口
	static std::vector<USERINFO> m_vecUserInfos;
};

