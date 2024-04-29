#include "mod_ts_file.h"
#include "parse_json.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string>
#include <vector>
#include <common/Log.h>
#include <common/Tool.h>

void printHelp();
void releaseTasks(std::vector<TaskParam*>& task_list);
void releaseModers(std::vector<ModTsFile*>& moder_list);

int main(int argc, char* argv[]){
    // 设置日志级别为debug
    Log::initLogLevel();

    std::vector<TaskParam*> tasks;
    std::vector<ModTsFile*> moders;
    
    if(argc < 2){
        printHelp();
        return 0;
    }

    Log::info(__FILE__, __LINE__, "Starting...");


    std::string conf_path = argv[1];
    std::ifstream file(conf_path.c_str());
    if(!file.is_open()){
        Log::error(__FILE__, __LINE__, "Failed to open the file");
    }

    std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    rapidjson::Document document;
    document.Parse(jsonStr.c_str());


    if(document["task_array"].Empty()){
        Log::error(__FILE__, __LINE__, "task_array is not exist");
    }

    if(!document["task_array"].IsArray()){
        Log::error(__FILE__, __LINE__, "task_array is not a array");
    }
    
    rapidjson::Value& task_array = document["task_array"];
    

    // 解task列表
    for(int i = 0; i < task_array.Size(); i++){
        TaskParam* task_param = new TaskParam;
        if(!task_param->parseJson(task_array[i])){
            delete task_param;
            task_param = nullptr;
            Log::error(__FILE__, __LINE__, "Failed to parse the task_param");
        }
        else{
            tasks.push_back(task_param);
            Log::info(__FILE__, __LINE__, "Success to parse a task_param ");
        }
    }

    // 执行task任务
    for(auto task_param : tasks){
        ModTsFile* moder = new ModTsFile(task_param);
        if(moder->Start()){
            moders.push_back(moder);
        }
        else{
            moder->Stop();
            delete moder;
            Log::error(__FILE__, __LINE__, "Failed to create a new moder thread ");
        }
    }

    // 检查所有任务是否结�?
    while(!moders.empty()){
        for(auto iter = moders.begin(); iter != moders.end(); iter++){
            auto moder = *iter;
            if(moder->Success()){
                moders.erase(iter);
                iter--;
            }
        }
    }

    Log::info(__FILE__, __LINE__, "Success to finish all the tasks");
    releaseTasks(tasks);
    Log::info(__FILE__, __LINE__, "success to clear all the tasks");
    releaseModers(moders);
    Log::info(__FILE__, __LINE__, "success to clear all the moders");
    return 0;
}

void printHelp(){
    std::cout << "Please input the conf file:" << std::endl;
    std::cout << "\t./mod_ts_file.exe \tconf.json" << std::endl;
}

void releaseTasks(std::vector<TaskParam*>& task_list){
    return;
    for(auto task : task_list){
        if(task != nullptr)
            delete task;
    }
    task_list.clear();
}

void releaseModers(std::vector<ModTsFile*>& moder_list){
    for(auto moder : moder_list){
        delete moder;
    }
    moder_list.clear();
}