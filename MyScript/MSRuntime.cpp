#include "stdafx.h"

#include "MSRuntime.h"

/*
Functions used internally by MyScript.
Not sure about that yet. Should that be global functions? not script local?
What about different runtime functions for each script, so we can optimize
stuff like string allocation for a given script... But then marshalling strings and stuff from C gets more complicated.
*/

//	Increment handle
void ms_rt_hdlinc(MSHandleInternal* hdl)
{
	if (hdl != nullptr)
	{
		hdl->refcount += 1;
	}
}

//	Decrement handle
void ms_rt_hdldec(MSHandleInternal* hdl)
{
	if (hdl != nullptr)
	{
		if (hdl->refcount == 1)
		{
			free(hdl);
		}
		else
			hdl->refcount -= 1;
	}
}
int ms_rt_strlen(MSHandleInternal* hdl)
{
	if (hdl != nullptr)
		return reinterpret_cast<MSStringInternal*>(hdl->ptr)->size;

	return 0;
}
MSHandleInternal* ms_rt_strcat(MSHandleInternal* s1, MSHandleInternal* s2)
{
	if (s1 == nullptr)
		return s2;
	if (s2 == nullptr)
		return s1;

	int s1_len = ms_rt_strlen(s1);
	int s2_len = ms_rt_strlen(s2);

	void* buffer = malloc(sizeof(MSHandleInternal) + sizeof(int) + (s1_len + s2_len + 1) * sizeof(wchar_t));

	MSHandleInternal* hdl = reinterpret_cast<MSHandleInternal*>(buffer);
	hdl->refcount = 1;

	MSStringInternal* new_s = reinterpret_cast<MSStringInternal*>(static_cast<char*>(buffer) + sizeof(MSHandleInternal));
	new_s->size = s1_len + s2_len;

	wcscpy(&new_s->data[0], &reinterpret_cast<MSStringInternal*>(s1->ptr)->data[0]);
	wcscat(&new_s->data[0], &reinterpret_cast<MSStringInternal*>(s2->ptr)->data[0]);

	hdl->ptr = new_s;

	return hdl;
}

int ms_rt_strcmp(MSHandleInternal* s1, MSHandleInternal* s2)
{
	//	same pointers
	if (s1 == s2)
		return 0;

	int s1_len = ms_rt_strlen(s1);
	int s2_len = ms_rt_strlen(s2);

	//	not same size
	if (s1_len != s2_len)
		return 1;
	else if (s1_len == 0)
		return 0;

	wchar_t* it1 = &reinterpret_cast<MSStringInternal*>(s1->ptr)->data[0];
	wchar_t* it2 = &reinterpret_cast<MSStringInternal*>(s2->ptr)->data[0];
	return wcscmp(it1, it2);
}
MSHandleInternal* ms_rt_substr(MSHandleInternal* s, int start, int len)
{
	if (s == nullptr || ms_rt_strlen(s) == 0)
		return s;

	if (start + len > ms_rt_strlen(s))
		len = ms_rt_strlen(s) - start;

	if (len <= 0)
		return nullptr;

	void* buffer = malloc(sizeof(MSHandleInternal) + sizeof(int) + (len + 1) * sizeof(wchar_t));
	MSHandleInternal* hdl = reinterpret_cast<MSHandleInternal*>(buffer);
	hdl->refcount = 1;

	MSStringInternal* new_s = reinterpret_cast<MSStringInternal*>(static_cast<char*>(buffer) + sizeof(MSHandleInternal));

	new_s->size = len;

	wcsncpy(&new_s->data[0], &reinterpret_cast<MSStringInternal*>(s->ptr)->data[0], len);
	new_s->data[len] = 0;

	hdl->ptr = new_s;
	return hdl;
}

MSHandleInternal* ms_rt_stralloc(const wchar_t* s, int len)
{
	void* buffer = malloc(sizeof(MSHandleInternal) + sizeof(int) + (len + 1) * sizeof(wchar_t));
	MSHandleInternal* hdl = reinterpret_cast<MSHandleInternal*>(buffer);
	hdl->refcount = 1;

	MSStringInternal* new_s = reinterpret_cast<MSStringInternal*>(static_cast<char*>(buffer) + sizeof(MSHandleInternal));
	new_s->size = len;

	wcscpy(&new_s->data[0], s);

	hdl->ptr = new_s;
	return hdl;
}

const wchar_t* ms_rt_strgetptr(MSHandleInternal* s)
{
	if (s == nullptr)
		return nullptr;
	if (s->ptr == nullptr)
		return nullptr;
	return &static_cast<MSStringInternal*>(s->ptr)->data[0];
}