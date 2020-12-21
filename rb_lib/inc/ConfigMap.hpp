#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <exception>

using namespace std;

class ConfigMap : public map<string, string> {
private:
    vector<string> keyOrder;

public:
    mapped_type& operator[](const key_type& key) {
        if ((*this).find(key) == (*this).end()) {
            keyOrder.push_back(key);
        }
        return map::operator[](key);
    }

    int getNumeric(string key) {
        try {
            string::size_type sz;
            string & val = (*this)[key];
            int num = stoi(val, &sz);
            // check if there are other chars after number
            if (val.substr(sz).size())
                throw invalid_argument("not pure number");
            return num;
        } catch (std::exception& e) {
            std::cout << "Cannot parse numeric value of property <"
                      << key << "> : " << (*this)[key] << std::endl;
            exit(-2);
        }
        return -1;
    }

    void load(string config_file_path) {
        map<string, string>& map = *this;
        ifstream config_file(config_file_path);

        if (!config_file) {
            store(config_file_path);
            cout << "No configuration file found!" << endl
                 << "A default file has been created at "
                 << config_file_path << endl
                 << "Insert the necessary information, then relaunch the application." << endl;
            exit(-1);
        }

        std::string line;
        while (std::getline(config_file, line)) {
            std::istringstream conf_line(line);
            std::string key;
            if (std::getline(conf_line, key, '=')) {
                std::string value;
                if (std::getline(conf_line, value) && map.find(key) != map.end())
                    map[key] = value;
            }
        }

        for (auto key : keyOrder) {
            if (map[key] == "") {
                cout << "Configuration file is invalid. The property <"
                     << key
                     << "> is missing or empty." << endl;
                store(config_file_path);
                exit(-1);
            }
        }
    }

    void store(string config_file_path) {
        std::ofstream def_config_file(config_file_path);
        for (auto key : keyOrder) {
            def_config_file
                << key << "="
                << (*this)[key] << std::endl;
        }
    }
};
