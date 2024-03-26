#pragma once
#include <xm/PlatformDefine.hpp>

#include <string>
#include <fstream>

#if defined(XM_OS_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <DbgHelp.h>
    #pragma comment(lib, "DbgHelp.lib")
#elif defined(XM_OS_LINUX)
    #include <execinfo.h>
    #include <dlfcn.h>
    #include <stdio.h>
#elif defined(XM_OS_MAC)
    #include <execinfo.h>
#endif

namespace xm {
/** unclassified utils. */
struct OtherTool {
    XM_FORCE_INLINE static uint32_t Backtrace(void* buffer[], uint32_t bufferSize) {
#if defined(XM_OS_WINDOWS)
        return (uint32_t)CaptureStackBackTrace(0, bufferSize, buffer, 0);
#elif defined(XM_OS_LINUX) || defined(XM_OS_MAC)
        return (uint32_t)backtrace(buffer, (int)bufferSize);
#else
        #error "not impl"
#endif
    }

    static std::string CallstackToString(void* frame) {
#if defined(XM_OS_WINDOWS)
        HANDLE hProcess = GetCurrentProcess();
        DWORD64 addr = (DWORD64)frame;
        constexpr int MAX_NAME_LEN = 128;
        char symbol[sizeof(SYMBOL_INFO) + MAX_NAME_LEN] = {};
        SYMBOL_INFO* pSymbol = (SYMBOL_INFO*)symbol;
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = MAX_NAME_LEN;
        if (!SymFromAddr(hProcess, addr, NULL, pSymbol)) {
            return "";
        }
        DWORD displacement = {};
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        if (!SymGetLineFromAddr64(hProcess, addr, &displacement, &line)) {
            return "";
        }
        std::string str;
        str.append(pSymbol->Name, pSymbol->NameLen);
        str += " at ";
        str += line.FileName;
        str += ":";
        str += std::to_string(line.LineNumber);
        return str;
#elif defined(XM_OS_LINUX)
        Dl_info dlinfo;
        if (dladdr(frame, &dlinfo) == 0) {
            return "";
        }
        if (dlinfo.dli_fname == nullptr) {
            return "";
        }
        if (frame < dlinfo.dli_fbase) {
            return "";
        }

        size_t offset = (size_t)frame - (size_t)dlinfo.dli_fbase;
        if (offset > 0) {
            offset -= 1;
        }

        char hex_str[32] = {};
        snprintf(hex_str, 32, "\" 0x%zx", offset);
        std::string cmd_str = "addr2line -C -f -e \"";
        cmd_str += dlinfo.dli_fname;
        cmd_str += hex_str;

        FILE* f = popen(cmd_str.c_str(), "r");
        if (f == nullptr) {
            return "";
        }
        std::string str;
        const char* at = " at: ";
        int c;
        while ((c = fgetc(f)) != EOF) {
            if (c == '\r' || c == '\n') {
                str.append(at);
                at = " ";
            } else {
                str.push_back((char)c);
            }
        }
        if (pclose(f) != 0) {
            return "";
        }
        return str;
#elif defined(XM_OS_MAC)
        return "TODO";
#else
        #error "not impl"
#endif
    }

    static std::string ReadFileBinary(const char* fileName) {
        std::string s;
        std::fstream ifs;
        ifs.exceptions(std::ios::badbit | std::ios::failbit);
        ifs.open(fileName, std::ios::in | std::ios::binary);
        ifs.seekg(0, std::ios::end);
        size_t size = (size_t)ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        if (size) {
            s.assign(size, 0);
            ifs.read(&s[0], size);
        }
        return s;
    }

#if defined(XM_OS_WINDOWS)
    static std::string LastErrorToString(DWORD errId) {
        if (errId == 0) {
            return "";
        }

        struct Tmp {
            ~Tmp() {
                if (p) {
                    LocalFree(p);
                }
            }
            LPSTR p = nullptr;
        } tmp;
        DWORD len = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            errId,
            MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
            (LPSTR)&tmp.p,
            0,
            nullptr);
        std::string str;
        if (len > 0) {
            // remove tail line breaks.
            for (int i = len; i > 0; --i) {
                char& c = tmp.p[i - 1];
                if (c == '\r' || c == '\n') {
                    c = ' ';
                } else {
                    break;
                }
            }
            str.append(tmp.p, len);
        } else {
            str = std::string("unknown(") + std::to_string(errId) + ")";
        }
        return str;
    }
#endif
};
}  // namespace xm
