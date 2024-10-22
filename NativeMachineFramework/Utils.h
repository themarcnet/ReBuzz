#pragma once

#include <string>

using System::String;


namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        class Utils
        {
        public:
            
            static void CLRStringToStdString(String^ str, std::string& out);

            static String^ stdStringToCLRString(const std::string& str);

        };
    }
}
