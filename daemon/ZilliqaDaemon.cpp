#include <algorithm>
#include <array>
#include <cstdio>
#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace std;

const vector<string> programName = {"zilliqa", "lzilliqa", "latezilliqa"};

const string restart_zilliqa = "python tests/Zilliqa/daemon_restart_local.py";

unordered_map<int, string> PrivKey;
unordered_map<int, string> PubKey;
unordered_map<int, string> Port;
unordered_map<int, string> Path;
unordered_map<int, bool> isDS;

const string logName = "epochinfo-00001-log.txt";

string ReadLastLine(string filePath, ofstream& log)
{
    ifstream logFile;

    logFile.open(filePath + "/" + logName);

    if (logFile.is_open())
    {
        logFile.seekg(-1, ios_base::end);
    }
    else
    {
        return "";
    }

    bool keepLooping = true;

    while (keepLooping)
    {
        char ch;
        logFile.get(ch);

        if ((int)logFile.tellg() <= 1)
        {
            logFile.seekg(0);
            keepLooping = false;
        }
        else if (ch == ']')
        {
            keepLooping = false;
        }
        else
        {
            logFile.seekg(-2, ios_base::cur);
        }
    }

    string lastLine = "";
    getline(logFile, lastLine);

    // log<<"Line :"<<lastLine<<endl;

    logFile.close();

    return lastLine;
}

void setIsDs(const pid_t& pid, ofstream& log)
{
    string s = ReadLastLine(Path[pid], log);

    if (s == " PROMOTED TO DS")
    {
        isDS[pid] = true;
    }
    else
    {
        isDS[pid] = false;
    }
}

vector<pid_t> getProcIdByName(string procName, ofstream& log)
{
    vector<pid_t> result;
    result.clear();

    // Open the /proc directory
    DIR* dp = opendir("/proc");

    if (dp != NULL)
    {
        // Enumerate all entries in directory until process found
        struct dirent* dirp;
        while ((dirp = readdir(dp)))
        {
            // Skip non-numeric entries
            int id = atoi(dirp->d_name);
            if (id > 0)
            {
                // Read contents of virtual /proc/{pid}/cmdline file
                string cmdPath = string("/proc/") + dirp->d_name + "/cmdline";
                ifstream cmdFile(cmdPath.c_str());
                string cmdLine;
                string fullLine;
                getline(cmdFile, fullLine);
                if (!fullLine.empty())
                {
                    // Keep first cmdline item which contains the program path
                    size_t pos = fullLine.find('\0');
                    if (pos != string::npos)
                        cmdLine = fullLine.substr(0, pos);
                    // Keep program name only, removing the path
                    pos = cmdLine.rfind('/');
                    string path = "";
                    if (pos != string::npos)
                    {
                        path = fullLine.substr(0, pos);
                        cmdLine = cmdLine.substr(pos + 1);
                        fullLine = fullLine.substr(pos + 1);
                    }
                    // Compare against requested process name
                    if (procName == cmdLine)
                    {
                        result.push_back(id);
                        size_t pubkey_pos = fullLine.find('\0');
                        fullLine = fullLine.substr(pubkey_pos + 1);
                        size_t privkey_pos = fullLine.find('\0');
                        string publicKey = fullLine.substr(0, privkey_pos);
                        PubKey[id] = publicKey;

                        log << publicKey << " ";

                        fullLine = fullLine.substr(privkey_pos + 1);
                        size_t privkey_pos_end = fullLine.find('\0');
                        string privKey = fullLine.substr(0, privkey_pos_end);

                        PrivKey[id] = privKey;

                        log << privKey << " ";

                        fullLine = fullLine.substr(privkey_pos_end + 1);
                        size_t ip_end = fullLine.find('\0');
                        string ip = fullLine.substr(0, ip_end);

                        fullLine = fullLine.substr(ip_end + 1);
                        size_t port_end = fullLine.find('\0');
                        string port = fullLine.substr(0, port_end);
                        Port[id] = port;

                        log << port << " ";

                        Path[id] = path;

                        log << " id: " << id << " Path: " << path << endl;

                        setIsDs(id, log);
                    }
                }
            }
        }
    }

    closedir(dp);

    return result;
}
string execute(string cmd)
{

    array<char, 128> buffer;
    string result;
    shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get()))
    {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

int getRestartValue(pid_t pid, const string& prgname)
{
    if (prgname == "latezilliqa" || prgname == "zilliqa")
    {
        if (isDS[pid])
        {
            return 3;
        }
        else
        {
            return 2;
        }
    }
    else if (prgname == "lzilliqa")
    {
        return 4;
    }

    return -1;
}

void initialize(unordered_map<string, vector<pid_t>>& pids,
                unordered_map<pid_t, bool>& died, ofstream& log)
{

    bool isProcesstoTrack = false;
    for (auto v : programName)
    {
        vector<pid_t> tmp = getProcIdByName(v, log);
        if (tmp.size() > 0)
        {
            isProcesstoTrack = true;
            pids[v] = tmp;
            log << "Process " << v << " exists in " << pids[v].size()
                << " instances" << endl;
            log << "Pids: ";
            for (auto i : pids[v])
            {
                log << i << " ";
                died[i] = false;
            }
            log << endl;
        }
        else
        {
            log << "Process " << v << " does not exist" << endl;
            //What to do??
        }
    }
    if (!isProcesstoTrack)
    {
        log << "No Process to Track\n"
            << " Exiting ..." << endl;
        exit(EXIT_SUCCESS);
    }
}

void MonitorProcess(unordered_map<string, vector<pid_t>>& pids,
                    unordered_map<pid_t, bool>& died, ofstream& log)
{

    for (string name : programName)
    {
        for (pid_t pid : pids[name])
        {

            setIsDs(pid, log);
            int w = kill(pid, 0);

            if (w < 0)
            {
                if (errno == EPERM)
                {
                    log << "Daemon does not have permission "
                        << "Name: " << name << " Id: " << pid << endl;
                }
                else if (errno == ESRCH)
                {
                    log << "Process died "
                        << "Name: " << name << " Id: " << pid << endl;
                    died[pid] = true;
                }
                else
                {
                    log << "Kill failed due to " << errno << " Name: " << name
                        << "Id: " << pid << endl;
                }
            }

            if (died[pid])
            {
                int v = getRestartValue(pid, name);

                died.erase(pid);

                auto it = find(pids[name].begin(), pids[name].end(), pid);
                if (it != pids[name].end())
                {
                    log << "Not monitoring " << pid << " of " << name << endl;
                    pids[name].erase(it);
                }
                if (v == -1)
                {
                    log << "Error " << pid << " does not give proper value "
                        << endl;
                    return;
                }

                try
                {
                    log << "Trying to restart .." << endl;
                    log << "Params: "
                        << PubKey[pid] + " " + PrivKey[pid] + " " + Port[pid]
                            + " " + to_string(v) + " " + Path[pid] + " " + name
                            + " 2>&1"
                        << endl;
                    log << "Output from shell...(if any)" << endl << endl;
                    log << "\" "
                        << execute(restart_zilliqa + " " + PubKey[pid] + " "
                                   + PrivKey[pid] + " " + Port[pid] + " "
                                   + to_string(v) + " " + Path[pid] + " " + name
                                   + " 2>&1")
                        << " \"" << endl;
                    log << endl;
                }
                catch (runtime_error& e)
                {
                    log << "ERROR!! " << e.what() << endl;
                }
                vector<pid_t> tmp = getProcIdByName(name, log);

                for (pid_t i : tmp)
                {
                    auto it = find(pids[name].begin(), pids[name].end(), i);
                    if (it == pids[name].end())
                    {
                        died[i] = false;
                        pids[name].push_back(i);
                        log << "Started new process " << name
                            << " with PiD: " << i << endl;
                    }
                }
                PrivKey.erase(pid);
                PubKey.erase(pid);
                Port.erase(pid);
                Path.erase(pid);
                isDS.erase(pid);
            }
        }
    }
}

int main()
{

    pid_t pid_parent, sid;
    ofstream log;
    log.open("daemon-log-local.txt", fstream::out | fstream::trunc);

    pid_parent = fork();

    if (pid_parent < 0)
    {
        log << "Failed to fork " << endl;
        exit(EXIT_FAILURE);
    }

    if (pid_parent > 0)
    {
        log << "Started daemon successfully" << endl;
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();

    if (sid < 0)
    {
        log << "Unable to set sid" << endl;
        exit(EXIT_FAILURE);
    }

    if ((chdir("..")) < 0)
    {
        log << "Failed to chdir" << endl;
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    unordered_map<string, vector<pid_t>> pids;
    unordered_map<pid_t, bool> died;

    initialize(pids, died, log);
    while (1)
    {
        MonitorProcess(pids, died, log);
        sleep(5);
    }

    exit(EXIT_SUCCESS);
}
