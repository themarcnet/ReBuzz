#include "Utils.h"

using namespace System;
using  System::Runtime::InteropServices::Marshal;

namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        void Utils::CLRStringToStdString(String^ str, std::string& out)
        {
            using namespace Runtime::InteropServices;
            const char* chars = (const char*)(Marshal::StringToHGlobalAnsi(str)).ToPointer();
            out = chars;
            Marshal::FreeHGlobal(IntPtr((void*)chars));
        }

        String^ Utils::stdStringToCLRString(const std::string& str)
        {
            return gcnew String(str.c_str());
        }

        int64_t Utils::ObjectToInt64(Object^ obj)
        {
            System::IntPtr val = Marshal::GetIUnknownForObject(obj); //GCHandle::ToIntPtr(handle);
            int64_t ret = val.ToInt64();
            Marshal::Release(val);
            return ret;
        }
    }
}