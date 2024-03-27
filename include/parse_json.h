#ifndef __PARSE_JSON_H_
#define __PARSE_JSON_H_
#include <iostream>
#include <string.h>
#include <vector>
#include <map>
#include <cmath>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

enum class PtsFunc{
    cut,
    add,
    minus,
    sin,
    pulse,
    mult,
    loss,
    repeate,
    null,
    pat_delete,
    pmt_delete,
    others
};

enum class Media{
    all,
    audio,
    video,
    others
};

class FuncPattern
{
public:
    FuncPattern(){
        start_sec  = -1;
        end_sec = -1;
        media = Media::all;
        pts_func = PtsFunc::others;
        pts_base = 0;
    }
    int start_sec;
    int end_sec;
    Media media;
    PtsFunc pts_func;
    float pts_base;
public:
    bool parseJson(const rapidjson::Value& pattern);
    void printPattern();
};

class TaskParam{
public:
    int id;
    std::string input_file;
    std::string output_file;
    std::vector<FuncPattern*> func_pattern;
public:
    TaskParam(){
        id = 0;
        input_file = "";
        output_file = "";
    }
    ~TaskParam(){
        std::cout << "Now, release " << func_pattern.size() << "pattern" << std::endl;
        if(!func_pattern.empty()){
            for(auto func : func_pattern){
                if(func != nullptr){
                    delete func;
                    func = nullptr;
                }
            }
            func_pattern.clear();
        }

        func_pattern.clear();
    }
    bool parseJson(const rapidjson::Value& TaskParam);
    void printTaskParam();
};

std::string PtsFuncToStr(PtsFunc pts_func);
PtsFunc StrToPtsFunc(const std::string& string);
std::string MediaToStr(Media media);
Media StrToMedia(const std::string& string);


#endif //__PARSE_JSON_H_