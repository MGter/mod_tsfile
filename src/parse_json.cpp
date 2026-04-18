#include "parse_json.h"
#include "common/Log.h"

bool FuncPattern::parseJson(const rapidjson::Value& pattern) {
    if (!pattern.IsObject()) {
        Log::error(__FILE__, __LINE__, "Invalid pattern format.");
        return false;
    }

    if (!pattern.HasMember("start_sec") || !pattern["start_sec"].IsInt()) {
        Log::error( __FILE__, __LINE__, "Missing or invalid 'start_sec' value.");
        return false;
    }
    start_sec = pattern["start_sec"].GetInt();

    if (!pattern.HasMember("end_sec") || !pattern["end_sec"].IsInt()) {
        Log::error( __FILE__, __LINE__, "Missing or invalid 'end_sec' value.");
        return false;
    }
    end_sec = pattern["end_sec"].GetInt();

    if (!pattern.HasMember("media") || !pattern["media"].IsString()) {
        Log::error( __FILE__, __LINE__, "Missing or invalid 'media' value.");
        return false;
    }
    media = StrToMedia(pattern["media"].GetString());

    if (!pattern.HasMember("func") || !pattern["func"].IsString()) {
        Log::error( __FILE__, __LINE__, "Missing or invalid 'func' value.");
        return false;
    }
    pts_func = StrToPtsFunc(pattern["func"].GetString());

    if (!pattern.HasMember("pts_base")) {
        Log::error( __FILE__, __LINE__, "Missing 'pts_base' value.");
    }
    else if(pattern["pts_base"].IsInt64()){
        pts_base = pattern["pts_base"].GetInt64();
        Log::debug(__FILE__, __LINE__, "pts_base is %f", pts_base);
    }
    else if(pattern["pts_base"].IsFloat()){
        pts_base = pattern["pts_base"].GetFloat();
    }
    else{
        ;
    }

    // 解析可选的 pid 字段，如果指定了 pid 则使用 PID 直接匹配
    if (pattern.HasMember("pid") && pattern["pid"].IsInt()) {
        target_pid = pattern["pid"].GetInt();
        Log::debug(__FILE__, __LINE__, "target_pid is %d", target_pid);
    }


    printPattern();
    return true;
}

void FuncPattern::printPattern(){
    std::cout << "\tstart_sec: "    << start_sec << std::endl;
    std::cout << "\tend_sec: "      << end_sec << std::endl;
    std::cout << "\tmedia: "        << MediaToStr(media) << std::endl;
    std::cout << "\tpts_func: "     << PtsFuncToStr(pts_func) << std::endl;
    std::cout << "\tpts_base: "     << pts_base << std::endl;
    std::cout << "\ttarget_pid: "   << target_pid << std::endl;
}

bool TaskParam::parseJson(const rapidjson::Value& task_param) {
    if (!task_param.IsObject()) {
        Log::error(__FILE__, __LINE__,  "Invalid task_param format.");
        return false;
    }

    if (!task_param.HasMember("id") || !task_param["id"].IsInt()) {
        Log::error(__FILE__, __LINE__,  "Missing or invalid 'id' value.");
        return false;
    }
    id = task_param["id"].GetInt();

    if (!task_param.HasMember("input_file") || !task_param["input_file"].IsString()) {
        Log::error(__FILE__, __LINE__,  "Missing or invalid 'input_file' value.");
        return false;
    }
    input_file = task_param["input_file"].GetString();

    if (!task_param.HasMember("output_file") || !task_param["output_file"].IsString()) {
        Log::error(__FILE__, __LINE__,  "Missing or invalid 'output_file' value.");
        return false;
    }
    output_file = task_param["output_file"].GetString();

    if (!task_param.HasMember("pattern") || !task_param["pattern"].IsArray()) {
        Log::error(__FILE__, __LINE__,  "Missing or invalid 'pattern' value.");
        return false;
    }

    const rapidjson::Value& patternArray = task_param["pattern"];
    for (rapidjson::SizeType i = 0; i < patternArray.Size(); i++) {
        std::unique_ptr<FuncPattern> pattern(new FuncPattern());
        if (!pattern->parseJson(patternArray[i])) {
            return false;
        }
        func_pattern.push_back(std::move(pattern));
    }

    return true;
}

void TaskParam::printTaskParam(){
    std::cout << "task_param id: " << id << std::endl;
    std::cout << "input file is: " << input_file << std::endl;
    std::cout << "output file is: " << output_file << std::endl;
    for(auto& pattern : func_pattern){
        std::cout << "pattern: " << std::endl;
        pattern->printPattern();
    }
}

std::string PtsFuncToStr(PtsFunc pts_func){
    std::string ans = "";
    switch (pts_func)
    {
    case PtsFunc::add:
        ans = "add";
        break;
    case PtsFunc::minus:
        ans = "minus";
        break;
    case PtsFunc::mult:
        ans = "mult";
        break;
    case PtsFunc::sin:
        ans = "sin";
        break;
    case PtsFunc::pulse:
        ans = "pulse";
        break;
    case PtsFunc::loss:
        ans = "loss";
        break;
    case PtsFunc::repeate:
        ans = "repeate";
        break;
    case PtsFunc::cut:
        ans = "cut";
        break;
    case PtsFunc::null:
        ans = "null";
        break;
    case PtsFunc::pat_delete:
        ans = "pat_delete";
        break;
    case PtsFunc::pmt_delete:
        ans = "pmt_delete";
        break;
    default:
        ans = "others";
        break;
    }
    return ans;
}

PtsFunc StrToPtsFunc(const std::string& string) {
    static const std::map<std::string, PtsFunc> stringToEnum {
        {"add", PtsFunc::add},
        {"minus", PtsFunc::minus},
        {"mult", PtsFunc::mult},
        {"sin", PtsFunc::sin},
        {"pulse", PtsFunc::pulse},
        {"loss", PtsFunc::loss},
        {"repeate", PtsFunc::repeate},
        {"cut", PtsFunc::cut},
        {"null", PtsFunc::null},
        {"pat_delete", PtsFunc::pat_delete},
        {"pmt_delete", PtsFunc::pmt_delete},
        {"others", PtsFunc::others}
    };

    auto it = stringToEnum.find(string);
    if (it != stringToEnum.end()) {
        return it->second;
    }

    // 如果输入字符串无法映射到枚举值，则返回默认值"others"
    return PtsFunc::others;
}

std::string MediaToStr(Media media) {
    std::string result;

    switch (media) {
        case Media::all:
            result = "all";
            break;
        case Media::audio:
            result = "audio";
            break;
        case Media::video:
            result = "video";
            break;
        case Media::others:
            result = "others";
            break;
        default:
            result = "unknown";
            break;
    }

    return result;
}

Media StrToMedia(const std::string& string) {
    if (string == "all") {
        return Media::all;
    } else if (string == "audio") {
        return Media::audio;
    } else if (string == "video") {
        return Media::video;
    } else if (string == "others") {
        return Media::others;
    }
    return Media::others;
}