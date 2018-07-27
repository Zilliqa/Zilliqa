/**
* Copyright (c) 2018 Zilliqa
* This source code is being disclosed to you solely for the purpose of your participation in
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
* the protocols and algorithms that are programmed into, and intended by, the code. You may
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
* including modifying or publishing the code (or any part of it), and developing or forming
* another public or private blockchain network. This source code is provided ‘as is’ and no
* warranties are given as to title or non-infringement, merchantability or fitness for purpose
* and, to the extent permitted by law, all liability for your use of the code is disclaimed.
* Some programs in this code are governed by the GNU General Public License v3.0 (available at
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
* and which include a reference to GPLv3 in their program files.
**/

#ifndef __JSONUTILS_H__
#define __JSONUTILS_H__

#include <json/json.h>

class JSONUtils
{
public:
    //Convert a string to Json object
    static bool convertStrtoJson(const string& str, Json::Value& dstObj)
    {
        Json::CharReaderBuilder readBuilder;
        unique_ptr<Json::CharReader> reader(readBuilder.newCharReader());
        string errors;
        if (!reader->parse(str.c_str(), str.c_str() + str.size(), &dstObj,
                           &errors))
        {
            LOG_GENERAL(WARNING,
                        "The Json is corrupted, failed to parse: " << errors);
            return false;
        }
        return true;
    }
    //Convert a Json object to string
    static string convertJsontoStr(const Json::Value& _json)
    {
        Json::StreamWriterBuilder writeBuilder;
        unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
        ostringstream oss;
        writer->write(_json, &oss);
        return oss.str();
    }
    //Write a Json object to target file
    static void writeJsontoFile(const string& path, const Json::Value& _json)
    {
        Json::StreamWriterBuilder writeBuilder;
        unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
        std::ofstream os(path);
        writer->write(_json, &os);
    }
};

#endif // __JSONUTILS_H__
