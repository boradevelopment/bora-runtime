// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#ifdef WRAPPER
#include "TAZA.h"
#include <stdlib.h>
#include <string.h>
#include "tools/AppParam.h"
#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include "SysImageMgr.h"
#include "../software/win32/rcscle.h"


bool CopyFileWin32(const std::wstring& sourcePath, const std::wstring& destPath, bool overwrite = true)
{
    // BOOL CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists);
    if (CopyFileW(sourcePath.c_str(), destPath.c_str(), !overwrite)) {
        return true;
    }
    else {
        DWORD error = GetLastError();
        std::cerr << "CopyFile failed with error: " << error << std::endl;
        return false;
    }
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cout << "Hello Bora";
        return 0;
    }

    AppParam::registerParam("compatability", {"-comp", "-co"});
    AppParam::initialize(argc, argv);

    std::wstring compatabilty = AppParam::get("compatability");
    if (!compatabilty.empty()) {
        HMODULE dll = LoadLibraryW(compatabilty.c_str());
        if (!dll) {
            MessageBoxW(NULL, L"Failed to load DLL", L"Error", MB_OK);
            return 1;
        }

        using MainFn = int(*)(int, char**);
        MainFn dllMain = (MainFn)GetProcAddress(dll, "main");

        if (!dllMain) {
            MessageBoxW(NULL, L"`main` function not found", L"Error", MB_OK);
            FreeLibrary(dll);
            return 1;
        }

        int result = dllMain(argc, argv);
        FreeLibrary(dll);
        return result;
    }

    char *wasm_file = argv[1];
    V2Archive boraApp;
    boraApp.output = wasm_file;

    int resArchive = boraApp.getArchive();
    if(resArchive != 0){
      //  wasm_runtime_destroy();
        printf("This is not a BORA application\n");
        return 1;
    }

    if(std::get<std::string>(boraApp.header.customVariables[L"id"]) != "BORA"){
        printf("This is not a BORA application\n");
        return 1;
    }

    auto entryFile = std::get<std::wstring>(boraApp.header.customVariables[L"entry"]);
    if(entryFile.empty()){
      //  wasm_runtime_destroy();
        printf("This is not a BORA application as it does not have a entry point.\n");
        return 1;
    }


    auto entryV2File = boraApp.header.files.find(entryFile);
    if(entryV2File == boraApp.header.files.end()){
   //     wasm_runtime_destroy();
        printf("This is not a BORA application as it has an invalid entry point.\n");
        return 1;
    }

    auto logoV2File = boraApp.header.files.find(L"logo");

    std::ifstream bla(wasm_file, std::ios::in | std::ios::binary);

    auto logoData = boraApp.header.getV2File(bla,logoV2File->second, boraApp.iv, boraApp.key);

    auto vData = boraApp.header.getV2File(bla,entryV2File->second, boraApp.iv, boraApp.key);
    if(vData.empty()){
      //  wasm_runtime_destroy();
        printf("This is not a BORA application as it's corrupted\n");
        return 1;
    }

    if(logoData.empty()) return 1;

    if (!std::filesystem::exists("./.cache/processes")) std::filesystem::create_directories("./.cache/processes");

    std::wstring iPath = L"./" + entryV2File->second.file + L".ico";

    SysImageMgr ico;
    auto ICON = SysImageMgr::CreateIcon(logoData, {0, 0});
    SysImageMgr::SaveIcon(ICON, iPath.c_str());

    std::wstring src = L"./BORASUBPROCESS.exe";
    std::wstring dst = L"./.cache/processes/" + entryV2File->first + L".exe";

    CopyFileWin32(src, dst);

    rescle::ResourceUpdater updater;
    if(!updater.Load(dst.c_str())) return 1;
    updater.SetFileVersion(0, 2, 4, 0);
    updater.SetVersionString(L"FileDescription", std::get<std::wstring>(boraApp.header.customVariables[L"displayName"]).c_str());
    updater.SetVersionString(L"ProductName", L"BORA APP");
    updater.SetVersionString(L"CompanyName", L"Team Azury BORA Development");
    updater.SetVersionString(L"ProductVersion", L"Al[1.0.0.0]");
    updater.SetIcon(iPath.c_str());
    updater.Commit();

    return 0;
    }
#endif