#include "mod_ts_file.h"
#include "parse_json.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>
#include <common/Log.h>
#include <common/Tool.h>

void printHelp();

int main(int argc, char* argv[]){
    // 设置日志级别为debug
    Log::initLogLevel();

    std::vector<std::unique_ptr<TaskParam>> tasks;
    std::vector<std::unique_ptr<ModTsFile>> moders;

    if(argc < 2){
        printHelp();
        return 0;
    }

    Log::info(__FILE__, __LINE__, "Starting...");


    std::string conf_path = argv[1];
    std::ifstream file(conf_path.c_str());
    if(!file.is_open()){
        Log::error(__FILE__, __LINE__, "Failed to open config file: %s", conf_path.c_str());
        return 1;
    }

    std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    rapidjson::Document document;
    document.Parse(jsonStr.c_str());

    if(document.HasParseError()){
        Log::error(__FILE__, __LINE__, "JSON parse error at offset %zu", document.GetErrorOffset());
        return 1;
    }

    if(!document.HasMember("task_array") || !document["task_array"].IsArray()){
        Log::error(__FILE__, __LINE__, "task_array is missing or not an array");
        return 1;
    }

    if(document["task_array"].Empty()){
        Log::error(__FILE__, __LINE__, "task_array is empty");
        return 1;
    }

    rapidjson::Value& task_array = document["task_array"];


    // 解析task列表
    for(size_t i = 0; i < task_array.Size(); i++){
        auto task_param = std::unique_ptr<TaskParam>(new TaskParam());
        if(!task_param->parseJson(task_array[i])){
            Log::error(__FILE__, __LINE__, "Failed to parse the task_param");
        }
        else{
            tasks.push_back(std::move(task_param));
            Log::info(__FILE__, __LINE__, "Success to parse a task_param ");
        }
    }

    // 执行task任务
    for(auto& task_param : tasks){
        auto moder = std::unique_ptr<ModTsFile>(new ModTsFile(std::move(task_param)));
        if(moder->Start()){
            moders.push_back(std::move(moder));
        }
        else{
            moder->Stop();
            Log::error(__FILE__, __LINE__, "Failed to create a new moder thread ");
        }
    }

    // 检查所有任务是否结束
    while(!moders.empty()){
        for(auto iter = moders.begin(); iter != moders.end(); iter++){
            auto& moder = *iter;
            if(moder->Success()){
                iter = moders.erase(iter);
                iter--;
            }
        }
    }

    Log::info(__FILE__, __LINE__, "Success to finish all the tasks");
    // unique_ptr 自动管理内存，无需手动清理
    Log::info(__FILE__, __LINE__, "success to clear all the tasks and moders");
    return 0;
}

void printHelp(){
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "    TS File Modification Tool" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "USAGE:" << std::endl;
    std::cout << "    ./mod_tsfile <config.json>" << std::endl;
    std::cout << std::endl;

    std::cout << "GENERATE CONFIG:" << std::endl;
    std::cout << "    python3 makejson.py -c 264 -i input.ts" << std::endl;
    std::cout << "    python3 makejson.py -c 265 -i input.ts" << std::endl;
    std::cout << std::endl;

    std::cout << "BUILD:" << std::endl;
    std::cout << "    make           # Compile" << std::endl;
    std::cout << "    make clean     # Clean" << std::endl;
    std::cout << std::endl;

    std::cout << "OPERATIONS:" << std::endl;
    std::cout << "    add/minus  - Add/subtract PTS offset" << std::endl;
    std::cout << "    sin/pulse  - Sinusoidal variation" << std::endl;
    std::cout << "    mult       - Multiply PTS factor" << std::endl;
    std::cout << "    cut        - Time-based cutting" << std::endl;
    std::cout << "    loss       - Random packet loss (%)" << std::endl;
    std::cout << "    repeate    - Packet duplication" << std::endl;
    std::cout << "    null       - Fill null packets" << std::endl;
    std::cout << std::endl;

    std::cout << "MATCHING MODES:" << std::endl;
    std::cout << "    media: all/audio/video - Match by stream type" << std::endl;
    std::cout << "    pid:  <number>         - Match by PID directly" << std::endl;
    std::cout << "    (pid mode bypasses PMT, works without PAT/PMT)" << std::endl;
    std::cout << std::endl;

    std::cout << "NOTES:" << std::endl;
    std::cout << "    pts_base = seconds * 90000 (90kHz)" << std::endl;
    std::cout << "    2s offset = 180000" << std::endl;
    std::cout << std::endl;
}