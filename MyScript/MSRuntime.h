#pragma once
#include "stdafx.h"

#include "MyScript.h"

struct MSHandleInternal
{
	int		refcount;
	void*	ptr;
};

struct MSStringInternal
{
	int size;
	wchar_t data[0];
};

void ms_rt_hdlinc(MSHandleInternal* hdl);
void ms_rt_hdldec(MSHandleInternal* hdl);

int ms_rt_strlen(MSHandleInternal* hdl);
MSHandleInternal* ms_rt_strcat(MSHandleInternal* s1, MSHandleInternal* s2);
int ms_rt_strcmp(MSHandleInternal* s1, MSHandleInternal* s2);
MSHandleInternal* ms_rt_substr(MSHandleInternal* s, int start, int len);
MSHandleInternal* ms_rt_stralloc(const wchar_t* s, int len);
const wchar_t* ms_rt_strgetptr(MSHandleInternal* s);