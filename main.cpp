#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <cpr/cpr.h>
#include <json.hpp>
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <yaml-cpp/yaml.h>

using namespace std;
using json = nlohmann::json;
using namespace sql;

atomic_bool run = true;
atomic_bool sqlop = false;

struct Config {
    string apiHost;
    string apiPass;
    string dbHost;
    string dbUser;
    string dbPass;
    string dbName;
    int refreshDelay;
    int blockConfirmerInterval;
    bool servicemode;
    bool logging;
} config;

struct roundContrib {
    string addr;
    int score;
};

void output(string out) {
    if(!config.servicemode) {
        cout << endl<< out << endl;
        cout << "command>";
    }
    else {
        cout << out << endl;
    }
    cout.flush();
}

void log(string str) {
    if(config.logging) {
        ofstream Log("extapi.log", std::ios_base::app);
        time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        string timestring = ctime(&now);
        Log << "[" << timestring.substr(0, timestring.size()-1) << "]  " << str << "\n";
        Log.close();
    }
}

void writeSQL(vector<roundContrib>* vect, int* height, long* timestamp, int* reward, string* finder, int* confirmHeight, string* hash, bool* solo, sql::Driver* driver, sql::Connection* con) {
    while(sqlop) usleep(1000);
    sqlop = true;
    //Calculate total score, if solo don't
    unsigned long totalScore = 0;
    if(!*solo) {
        for(auto i = (*vect).begin(); i != (*vect).end(); ++i) {
            totalScore += (*i).score;
        }
    }
    //Write block data
    sql::PreparedStatement* prep_stmt;
    prep_stmt = con->prepareStatement("INSERT INTO BlockData (height, totalScore, timestamp, reward, finder, confirmHeight, hash) VALUES (?, ?, ?, ?, ?, ?, ?)");
    prep_stmt->setInt(1, *height);
    prep_stmt->setUInt64(2, totalScore);
    prep_stmt->setUInt64(3, *timestamp);
    prep_stmt->setInt(4, *reward);
    prep_stmt->setString(5, *finder);
    prep_stmt->setInt(6, *confirmHeight);
    prep_stmt->setString(7, *hash);
    prep_stmt->execute();
    if(!*solo) {
        //Get block ID
        int blockID = 0;
        sql::Statement* stmt;
        sql::ResultSet* res;
        stmt = con->createStatement();
        res = stmt->executeQuery("SELECT LAST_INSERT_ID() as id");
        while(res->next()) {
            blockID = stoi(res->getString("id"));
        }

        //Write miners
        for(auto i = (*vect).begin(); i != (*vect).end(); ++i) {
            if((*i).score != 0) {
                prep_stmt = con->prepareStatement("INSERT INTO BlockContrib (address, score, blockID) VALUES (?, ?, ?)");
                prep_stmt->setString(1, (*i).addr);
                prep_stmt->setInt(2, (*i).score);
                prep_stmt->setInt(3, blockID);
                prep_stmt->execute();
            }
        }
        delete stmt;
        delete res;
    }
    delete prep_stmt;
    sqlop = false;
}

void CommandHandler() {
    while(run && !config.servicemode) {
        string command;
        getline(cin, command);

        if (command == "q" || command == "quit" || command == "exit") { //EXIT
            run = false;
            break;
        }
        else if(command == "help") {
            cout << "q|quit|exit       -- Quit extAPI" << endl;
            cout << "help              -- Show help" << endl;
        }

        cout << "command>";
        cout.flush();
    }
}

void tokenize(std::string const &str, const char delim, std::vector<std::string> &out)
{
    size_t start;
    size_t end = 0;
    while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
    {
        end = str.find(delim, start);
        out.push_back(str.substr(start, end - start));
    }
}

bool inVect(vector<int> vect, int searchVal) {
    for (auto i = vect.begin(); i != vect.end(); ++i) {
        if(*i == searchVal) {
            return true;
        }
    }
    return false;
}

void BlockConfirmer(sql::Driver* driver, sql::Connection* con) {
    while(run) {
        while(sqlop) usleep(1000);
        sqlop = true;
        //Gather data for blocks that can be confirmed now(confirmHeight >= networkHeight)
        cpr::Response sResp = cpr::Get(cpr::Url{config.apiHost + "/stats"}, cpr::VerifySsl(0));
        json stats = json::parse(sResp.text);
        int height = stats["network"]["height"];
        vector<int> toConfirm;
        sql::Statement* stmt;
        sql::ResultSet* res;
        stmt = con->createStatement();
        res = stmt->executeQuery("SELECT height FROM BlockData WHERE confirmed = 0 AND confirmHeight <= " + to_string(height));
        while(res->next()) {
            int asd = res->getInt("height");
            toConfirm.push_back(asd);
        }
        //Confirm blocks if there are blocks to confirm
        if(toConfirm.size() != 0) {
            int oldest = *std::min_element(toConfirm.begin(), toConfirm.end());
            bool found = false;
            int oldestQuery = height;
            vector<string> blockData;
            for(auto i = stats["pool"]["blocks"].begin(); i != stats["pool"]["blocks"].end(); ++i) {
                string data = (*i);
                int _height = stoi((string)*++i);
                if(_height <= oldest) found = true;
                if (inVect(toConfirm, _height)) blockData.push_back(data + ":" + to_string(_height));
                if(_height < oldestQuery) oldestQuery = _height;
            }
            int counter = 0;
            while(!found && counter < 50) {
                counter++;
                cpr::Response moreBlocks = cpr::Get(cpr::Url{config.apiHost + "/get_blocks?height=" + to_string(oldestQuery)}, cpr::VerifySsl(0));
                json _moreBlocks = json::parse(moreBlocks.text);
                for(auto i = _moreBlocks.begin(); i != _moreBlocks.end(); ++i) {
                    string data = (*i);
                    int _height = stoi((string)*++i);
                    if(_height <= oldest) found = true;
                    if (inVect(toConfirm, _height)) blockData.push_back(data + ":" + to_string(_height));
                    if(_height < oldestQuery) oldestQuery = _height;
                }
            }
            for (auto i = blockData.begin(); i != blockData.end(); ++i) {
                vector<string> split;
                tokenize(*i, ':', split);
                string _h = split.back();
                //Block might not be confirmed/unconfirmed by pool for black fucking magic regions, I hate my life
                if(split[5] == split[6]) {
                    log("Block " + _h + " incomplete data, waiting another round for confirmation: " + *i);
                    continue;
                }
                sql::PreparedStatement* prep_stmt;
                if(split[6] == "0") {
                    prep_stmt = con->prepareStatement("UPDATE BlockData SET confirmed = 1 WHERE height = ?");
                    log("Block " + _h + " confirmed: " + *i);
                }
                else {
                    prep_stmt = con->prepareStatement("UPDATE BlockData SET confirmed = 2 WHERE height = ?");
                    log("Block " + _h + " orphaned: " + *i);
                }
                prep_stmt->setInt(1, stoi(_h));
                prep_stmt->execute();
                delete prep_stmt;
            }
        }
        delete stmt;
        delete res;
        sqlop = false;

        for(int i = 0; i < config.blockConfirmerInterval && run; i++) {
            usleep(1000);
        }
    }
}

int main() {

    YAML::Node conf = YAML::LoadFile("config.yaml");
    if(!conf["apiHost"] || !conf["apiPass"] || !conf["dbHost"] || !conf["dbUser"] || !conf["dbPass"] || !conf["dbName"] || !conf["refreshDelay"] || !conf["blockConfirmerInterval"]) {
        cout << "Invalid config, exiting" << endl;
        return 1;
    }

    config.apiHost = conf["apiHost"].as<std::string>();
    config.apiPass = conf["apiPass"].as<std::string>();
    config.dbHost = conf["dbHost"].as<std::string>();
    config.dbUser = conf["dbUser"].as<std::string>();
    config.dbPass = conf["dbPass"].as<std::string>();
    config.dbName = conf["dbName"].as<std::string>();
    config.refreshDelay = stoi(conf["refreshDelay"].as<string>());
    config.blockConfirmerInterval = stoi(conf["blockConfirmerInterval"].as<string>());
    config.servicemode = conf["servicemode"].as<std::string>() == "1";
    config.logging = conf["logging"].as<std::string>() == "1";

    sql::Driver *driver;
    sql::Connection *con;
    driver = get_driver_instance();
    con = driver->connect(config.dbHost, config.dbUser, config.dbPass);
    con->setSchema(config.dbName);

    thread commandHandler(CommandHandler);
    thread blockConfirmer(BlockConfirmer, driver, con);

    int lastMined = 0;
    vector<roundContrib> contribs;


    while(run) {
        cpr::Response mResp = cpr::Get(cpr::Url{config.apiHost + "/admin_users"}, cpr::Parameters{{"password", config.apiPass.c_str()}}, cpr::VerifySsl(0));
        cpr::Response sResp = cpr::Get(cpr::Url{config.apiHost + "/stats"}, cpr::VerifySsl(0));
        json miners = json::parse(mResp.text);
        json stats = json::parse(sResp.text);

        //Get height, timestamp, reward for last mined block
        int height = 0;
        string finder = "";
        string hash = "";
        bool solo = false;
        for(auto i = stats["pool"]["blocks"].begin(); i != stats["pool"]["blocks"].end(); ++i) {
            string data = (*i);
            int _height = stoi((string)*++i);
            //Check if current block is newer than previously seen newest
            if(_height > height) {
                height = _height;
                finder = data.substr(5, 17);
                hash = data.substr(23, 64);
                if(data.substr(0, 4) == "solo") {
                    solo = true;
                }
                else {
                    solo = false;
                }
            }
        }

        //We are on a new block, save shit
        if(height > lastMined) {
            long timestamp = solo ? stol((string)stats["pool"]["lastBlockFoundSolo"]) : stol((string)stats["pool"]["lastBlockFound"]);
            int reward = stats["lastblock"]["reward"];
            int confirmHeight = height + (int)stats["config"]["depth"] + 1;
            output("Detected new block mined: " + to_string(height));
            log("Detected new block mined: " + to_string(height));
            if(lastMined != 0) writeSQL(&contribs, &height, &timestamp, &reward, &finder, &confirmHeight, &hash, &solo, driver, con);
            lastMined = height;
        }

        //Get latest contribs
        contribs.clear();
        for(auto m = miners.begin(); m != miners.end(); ++m) {
            contribs.push_back({m.key(), m.value()["roundScore"]});
        }
        usleep(1000 * config.refreshDelay);
    }
    cout << endl << "Exiting..." << flush;
    commandHandler.join();
    blockConfirmer.join();
    return 0;
}
