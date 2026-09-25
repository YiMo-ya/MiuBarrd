#pragma once

#include "Xs.Main.h"

namespace xs
{
    //字符串
    namespace XString 
    {
    
        //转换
        namespace Convert
        {
        
            // UTF-8 std::string <-> std::wstring
            std::wstring utf8_to_wstring(const std::string& utf8);
            std::string wstring_to_utf8(const std::wstring& wstr);

            // GBK std::string <-> std::wstring
            std::wstring gbk_to_wstring(const std::string& gbk);
            std::string wstring_to_gbk(const std::wstring& wstr);

            // UTF-8 <-> GBK
            std::string utf8_to_gbk(const std::string& utf8);
            std::string gbk_to_utf8(const std::string& gbk);
        };
       //查找
        namespace Find
        {
        
            size_t find(const std::string& str, const std::string& sub, size_t offset = 0);
            size_t find(const std::wstring& str, const std::wstring& sub, size_t offset = 0);

            bool contains(const std::string& str, const std::string& sub);
            bool contains(const std::wstring& str, const std::wstring& sub);
        };
       //替换
        namespace Replace
        {
        
            std::string replace(const std::string& str,
                const std::string& from,
                const std::string& to);

            std::wstring replace(const std::wstring& str,
                const std::wstring& from,
                const std::wstring& to);
        };
        //删除
        namespace Remove
        {
        
            std::string remove(const std::string& str, const std::string& sub);
            std::wstring remove(const std::wstring& str, const std::wstring& sub);
        };
        //大小写
        namespace Case
        {
        
            std::string ToLower(std::string str);
            std::string ToUpper(std::string str);

            std::wstring ToLower(std::wstring str);
            std::wstring ToUpper(std::wstring str);
        };
        //分割
        namespace Split
        {
        
            std::vector<std::string> split(const std::string& str, char delimiter);
            std::vector<std::wstring> split(const std::wstring& str, wchar_t delimiter);
        };
    };
}