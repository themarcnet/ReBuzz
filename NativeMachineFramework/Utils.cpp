#include "Utils.h"

using namespace System;

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
    }
}