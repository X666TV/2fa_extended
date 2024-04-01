#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <sstream>
#include <map>
#include "include/auth.h"
#include "src/ProgressBar.h"
#include "include/cmdline.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define DEFALT_SAVED_PATH "./2fakey.key"

void copyToClipboard(const std::string &content)
{
#ifdef __APPLE__
    // macOS
    FILE *pbcopyProcess = popen("pbcopy", "w");
    if (pbcopyProcess == nullptr)
    {
        std::cerr << "Error opening pipe to pbcopy" << std::endl;
        return;
    }
    fprintf(pbcopyProcess, "%s", content.c_str());
    if (pclose(pbcopyProcess) == -1)
    {
        std::cerr << "Error closing pipe to pbcopy" << std::endl;
        return;
    }
#elif _WIN32
    // Windows
    if (OpenClipboard(nullptr))
    {
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, content.size() + 1);
        if (hg == nullptr)
        {
            CloseClipboard();
            return;
        }
        memcpy(GlobalLock(hg), content.c_str(), content.size() + 1);
        GlobalUnlock(hg);
        SetClipboardData(CF_TEXT, hg);
        CloseClipboard();
        GlobalFree(hg);
    }
#endif
}

int main(int argc, char *argv[])
{
    cmdline::parser parser;
    parser.add("ls",'l',"list the names of all keys in the keychain");
    parser.add("values",'v',"list all saved key token values");
    parser.add<std::string>("file",'f',"specify the file for saving the data",false,DEFALT_SAVED_PATH);
    parser.add<std::string>("auth",'a',"specify the name and save it with its authentication code",false,"");
    parser.add<std::string>("name",'n',"search the local key file for a given name and prints the two-factor authentication code corresponding to that name",false,"");
    parser.add("copy",'c',"copy the code to the system clipboard");
    parser.add("help",'h',"print this message");
    
    parser.parse_check(argc, argv);

    std::string fileName = parser.get<std::string>("file");
    std::string secret = "";

    if (parser.exist("help"))
    {
        std::cout << parser.usage() << std::endl;
        return 0;
    }

    if (parser.exist("ls"))
    {
        std::ifstream inputFile(fileName);
        if (!inputFile.is_open())
        {
            std::cerr << "Unable to open the file." << std::endl;
        }
        std::string line;
        std::string currentName, allNames;

        while (std::getline(inputFile, line))
        {
            size_t namePos = line.find("name:");
            if (namePos != std::string::npos)
            {
                currentName = line.substr(namePos + 5); // 5 是 "name:" 的长度
                allNames += currentName + "\n";
            }
        }
        inputFile.close();

        // 检查最后换行
        if (!allNames.empty() && allNames.back() == '\n')
        {
            allNames.pop_back();
        }

        if (!fileName.empty())
        {
            std::cout << "Name found: \n" << allNames << std::endl;
        }
        else
        {
            std::cout << "Name not found in the file." << std::endl;
        }
        return 0;
    }

    if (parser.exist("values"))
    {
        struct Credential
        {
            std::string name;
            std::string secret;
        };
        std::ifstream inputFile(fileName);
        std::vector<Credential> credentials;

        if (!inputFile.is_open())
        {
            std::cerr << "Unable to open the file." << std::endl;
        }
        
        std::string line;
        Credential currentCredential;

        while (std::getline(inputFile, line))
        {
            size_t namePos = line.find("name:");
            if (namePos != std::string::npos)
            {
                currentCredential.name = line.substr(namePos + 5);
            }
            size_t secretPos = line.find("secret:");
            if (secretPos != std::string::npos)
            {
                currentCredential.secret = line.substr(secretPos + 7);
                credentials.push_back(currentCredential);
            }
        }
        inputFile.close();

        if (parser.exist("copy"))
        {
            std::cout << "Copy is not supported here." << std::endl;
        }
        
        if(!credentials.empty())
        {
            for(const auto& credential : credentials)
            {
                std::cout << "Name: " << credential.name << ", Token: " << auth::generateToken(credential.secret) << "\n";
            }
        }
        return 0;
    }

    if (""!=parser.get<std::string>("auth"))
    {
        std::fstream cfile;
        cfile.open(fileName, std::ios::out | std::ios::app);
        cfile << "name:" << parser.get<std::string>("auth") << std::endl;
        std::cout << "Please enter your token:";
        std::cin >> secret;
        cfile << "secret:" << secret << std::endl;
        cfile.close();

        if (parser.exist("copy"))
        {
            std::ostringstream oss;
            oss << auth::generateToken(secret);
            std::string secretStr = oss.str();
            copyToClipboard(secretStr);
        }
        std::cout << "your dynamic token:" << auth::generateToken(secret) << std::endl;
        return 0;
    }

    if (""!=parser.get<std::string>("name"))
    {
        std::string targetName;
        std::string foundSecret; // 用于存储找到的secret值
        targetName = parser.get<std::string>("name");
        std::ifstream inputFile(fileName);
        if (!inputFile.is_open())
        {
            std::cerr << "Unable to open the file." << std::endl;
        }
        std::string line;
        std::string currentName;

        while (std::getline(inputFile, line))
        {
            size_t namePos = line.find("name:");
            if (namePos != std::string::npos)
            {
                std::string currentName = line.substr(namePos + 5); // 5 是 "name:" 的长度
                if (currentName == targetName)
                {
                    // 找到目标名字后，继续查找对应的secret值
                    while (std::getline(inputFile, line))
                    {
                        size_t secretPos = line.find("secret:");
                        if (secretPos != std::string::npos)
                        {
                            foundSecret = line.substr(secretPos + 7); // 7 是 "secret:" 的长度
                            break;                                    // 找到secret值后，结束循环
                        }
                    }
                    break; // 结束外层循环
                }
            }
        }
        inputFile.close();
        if (parser.exist("copy"))
        {
            std::ostringstream oss;
            oss << auth::generateToken(foundSecret);
            std::string secretStr = oss.str();
            copyToClipboard(secretStr);
        }
        std::cout << "your dynamic token:" << auth::generateToken(foundSecret) << std::endl;
        showCountdown();
        return 0;
    }

    std::cout << "Please enter your token:";
    std::cin >> secret;
    std::cout << "Whether to save(yes/no)?";
    std::string save;
    std::cin >> save;
    if (save == "yes" || save == "y")
    {
        std::fstream cfile;
        cfile.open(fileName, std::ios::out | std::ios::app);
        std::string Name;
        std::cout << "the name you wish to save:";
        std::cin >> Name;
        cfile << "name:" << Name << std::endl;
        cfile << "secret:" << secret << std::endl;
        cfile.close();
    }
    std::cout << "your dynamic token:" << auth::generateToken(secret) << std::endl;

    return 0;
}